#pragma once
#include "..\Shared\ProcessClient.h"
#include "..\Shared\UserInterfaceClient.h"
#include "..\Shared\IOServer.h"
#include "..\Shared\IOClient.h"
#include "..\Shared\Data.h"
#include <map>
#include <queue>
#include <iostream>
#include <string>
#include <cstdint>
#include <sstream>
#include <array>

//Prelimnary Profiling - ReportGA120260106-1844.diagsession is a GO. No major bottlenecks observed. and HotPaths are not the datapaths -ie Dispatch, Reader and writer as expected

#define DEFAULT_STRPROC_PIPESERVER L"\\\\.\\pipe\\DOWNSTREAM"
#define DEFAULT_AUXSTRPROC_PIPESERVER L"\\\\.\\pipe\\_"

typedef enum class direction
{
	UPSTREAM,
	DOWNSTREAM
}interfaceDirection;

// Test -done OK
template <typename T, size_t streamWidth>
class StreamProcess : public ProcessClient<wchar_t> {

	//Future: IOServer and IOClient to replace direct pipe handling for better abstraction and error handling, improving process scalability and maintainability
	
	// Channel Config - Upstream
	struct UpstreamChannel {
		explicit UpstreamChannel(HANDLE h) : pipe(h) {}
		// Deleting the copy constructors to avoid ambiguity
		UpstreamChannel(UpstreamChannel&& other) noexcept
			: pipe(other.pipe), event(other.event), dataQueue(std::move(other.dataQueue)),
			readerThread(std::move(other.readerThread)) {
		}
		UpstreamChannel& operator=(UpstreamChannel&& other) noexcept {
			if (this != &other) {
				pipe = other.pipe;
				event = other.event;
				dataQueue = std::move(other.dataQueue);
				readerThread = std::move(other.readerThread);
			}
			return *this;
		}
		UpstreamChannel(const UpstreamChannel&) = delete;
		UpstreamChannel& operator=(const UpstreamChannel&) = delete;
		
		HANDLE pipe;
		HANDLE event;
		std::queue<std::array<T, streamWidth>> dataQueue;
		std::mutex mtx;
		std::thread readerThread;
	};

	//Channel Config - Downstream
	struct DownstreamChannel {
		explicit DownstreamChannel(HANDLE h) : pipe(h) {}
		HANDLE pipe;
		HANDLE event;
		std::queue<std::array<T, streamWidth>> dataQueue;
		std::mutex mtx;
		std::thread writerThread;
		std::atomic<bool> stop{ false };
	};

	// Aux Channel 
	struct AuxChannel {
		AuxChannel() : pipe(INVALID_HANDLE_VALUE), event(nullptr), stop(false), enabled(false) {}
		explicit AuxChannel(HANDLE h) : pipe(h) {}
		AuxChannel(AuxChannel&& other) noexcept
			: pipe(other.pipe), event(other.event), dataQueue(std::move(other.dataQueue)),
			dir(other.dir),
			ioThread(std::move(other.ioThread)),
			stop(other.stop.load()), enabled(other.enabled.load()) {
		}

		AuxChannel& operator=(AuxChannel&& other) noexcept {
			if (this != &other) {
				pipe = other.pipe;
				event = other.event;
				dataQueue = std::move(other.dataQueue);
				dir = other.dir;
				ioThread = std::move(other.ioThread);
				stop = other.stop.load();
				enabled = other.enabled.load();
			}
			return *this;
		}

		AuxChannel( AuxChannel&) = delete;
		AuxChannel& operator=(const AuxChannel&) = delete;
		HANDLE pipe;
		HANDLE event;
		std::queue<std::vector<uint8_t>> dataQueue;
		std::mutex mtx;
		interfaceDirection dir;
		std::thread ioThread;
		std::atomic<bool> stop{ false };
		std::atomic<bool> enabled{ false };
	};

	std::map<std::wstring, AuxChannel> auxChannels_;

	
	// Event Based Flags - New
	std::unordered_map<HANDLE, HANDLE> routingMap_;
	std::vector<UpstreamChannel> upstreamChannels_;
	std::vector<bool> upstreamChannelStatus;
	std::unordered_map<HANDLE, DownstreamChannel> writerMap_;
	std::vector<bool> downstreamChannelStatus;
	HANDLE hStop;
	std::thread dispatcherThread_;

	std::wstring uint8ToWString(uint8_t value)
	{
		std::wstringstream ws;
		ws << static_cast<unsigned int>(value);   // promote to avoid printing as char
		return ws.str();
	}
	
	HANDLE connectUpStreamInterface(std::wstring upstream)
	{
		// PipeName = Application Name + US + count
		std::wcout << procName << L"'s Upstream is " << upstream;
		std::wstring pipeName = DEFAULT_STRPROC_PIPESERVER + upstream;

		std::wcout << L"Connecting to " << pipeName;

		HANDLE interfaceHandle = CreateFileW(
			pipeName.c_str(),
			GENERIC_READ,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED,
			NULL
		);

		return interfaceHandle;
	}

	HANDLE createDownStreamInterface(std::wstring downstream)
	{
		//PipeName = Application Name + DS + count
		
		std::wstring pipeName = DEFAULT_STRPROC_PIPESERVER + procName;

		std::wcout << L"Connecting to " << pipeName;

		HANDLE intf = CreateNamedPipeW(
			pipeName.c_str(),
			PIPE_ACCESS_OUTBOUND,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, // should indeally be byte stream for performance but okay for now. no major bottlenecks observed in profiling
			PIPE_UNLIMITED_INSTANCES,
			4096,
			4096,
			INFINITE,
			nullptr
		);


		if (intf == INVALID_HANDLE_VALUE) {
			std::wcerr << L"CreateNamedPipeW failed: " << GetLastError() << std::endl;
			return intf;
		}

		BOOL connected = ConnectNamedPipe(intf, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
		if (!connected) {
			std::wcerr << L"ConnectNamedPipe failed: " << GetLastError() << std::endl;
			CloseHandle(intf);
			return INVALID_HANDLE_VALUE;
		}

		return intf;
	}

	void createAuxChannel(const std::wstring& name, const std::wstring& pipeName, interfaceDirection dir)
	{
		if (auxChannels_.count(name))
		{
			return;
		}

		if (dir == interfaceDirection::UPSTREAM)
		{
			HANDLE pipe = CreateFileW(
				pipeName.c_str(),
				GENERIC_READ,
				0,
				nullptr,
				OPEN_EXISTING,
				0,
				nullptr
			);

			if (pipe == INVALID_HANDLE_VALUE)
			{
				std::wcerr << L"CreateFileW failed for aux: " << pipeName << std::endl;
				return;
			}

			AuxChannel aux(pipe);
			aux.event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			aux.enabled = false;
			aux.dir = dir;
			aux.stop = false;
			auxChannels_[name] = std::move(aux);

			std::wcout << L"Created up aux channel: " << name << L" with pipe: " << pipeName << std::endl;

			return;
		}
		else if (dir == interfaceDirection::DOWNSTREAM)
		{
			HANDLE pipe = CreateNamedPipeW(
				pipeName.c_str(),
				PIPE_ACCESS_OUTBOUND,
				PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
				PIPE_UNLIMITED_INSTANCES,
				4096, 4096, INFINITE, nullptr
			);
			if (pipe == INVALID_HANDLE_VALUE) {
				std::wcerr << L"CreateNamedPipeW failed for aux: " << pipeName << std::endl;
				return;
			}

			// Wait for client to connect
			BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
			if (!connected) {
				std::wcerr << L"ConnectNamedPipe failed: " << GetLastError() << std::endl;
				CloseHandle(pipe);
				return ;
			}

			AuxChannel aux(pipe);
			aux.event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			aux.enabled = false;
			aux.dir = dir;
			aux.stop = false;
			auxChannels_[name] = std::move(aux);

			


			std::wcout << L"Created down aux channel: " << name << L" with pipe: " << pipeName << std::endl;
			return;
		}
	}

	void startAuxChannels() 
	{
		for (auto& kv : auxChannels_)
		{
			auto& aux = kv.second;
			if (aux.enabled)
			{
				aux.stop = false;
				if (aux.dir == interfaceDirection::UPSTREAM)
				{
					aux.ioThread = std::thread(&StreamProcess<T, streamWidth>::auxReaderThreadFunc, this, std::ref(aux));
				}
				else if (aux.dir == interfaceDirection::DOWNSTREAM)
				{
					aux.ioThread = std::thread(&StreamProcess<T, streamWidth>::auxWriterThreadFunc, this, std::ref(aux));
				}
			}
		}
	}

	void stopAuxChannels() {
		for (auto& kv : auxChannels_)
		{
			auto& aux = kv.second;
			aux.stop = true;
			SetEvent(aux.event);
			if (aux.ioThread.joinable()) aux.ioThread.join();
			if (aux.event) CloseHandle(aux.event);
			if (aux.pipe != INVALID_HANDLE_VALUE) CloseHandle(aux.pipe);
		}
	}

	void auxReaderThreadFunc(AuxChannel& aux)
	{
		while (!aux.stop) {
			std::vector<uint8_t> buffer(4096);
			DWORD read = 0;
			BOOL ok = ReadFile(aux.pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr);
			if (!ok || read == 0) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}
			buffer.resize(read);
			{
				std::lock_guard<std::mutex> lk(aux.mtx);
				aux.dataQueue.push(buffer);
			}
			SetEvent(aux.event);
		}
	}
	
	void auxWriterThreadFunc(AuxChannel& aux)
	{
		while (!aux.stop) {
			DWORD waitResult = WaitForSingleObject(aux.event, INFINITE);
			if (waitResult != WAIT_OBJECT_0) break;
			std::vector<uint8_t> data;
			{
				std::lock_guard<std::mutex> lk(aux.mtx);
				if (aux.dataQueue.empty()) {
					ResetEvent(aux.event);
					continue;
				}
				data = std::move(aux.dataQueue.front());
				aux.dataQueue.pop();
				if (aux.dataQueue.empty()) ResetEvent(aux.event);
			}
			DWORD written = 0;
			WriteFile(aux.pipe, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
		}
	}

	void enableAuxChannel(const std::wstring& name, bool enable, interfaceDirection dir)
	{
		if (auxChannels_.count(name))
		{
			auxChannels_[name].enabled = enable;
		}
		else
		{
			std::wstring pipeName = DEFAULT_AUXSTRPROC_PIPESERVER + std::wstring(name.begin(), name.end());
			createAuxChannel(name, pipeName, dir);
			auxChannels_[name].enabled = enable;
		}
	}

	//QA: //Pipelined // Should be lock free queues for performance @ < 500ns latency

	/* Profiling: More on https://learn.microsoft.com/en-us/visualstudio/profiling/?view=visualstudio
	*/
	void upstreamReaderThreadFunc(UpstreamChannel& channel)
	{
		
		while (true)
		{

			std::array<T,streamWidth> arr{};
			DWORD read = 0;
			T* data = arr.data();
			BOOL ok = ReadFile(channel.pipe, data, sizeof(T) * streamWidth , &read, nullptr);
			if (!ok || read == 0)
			{
				SetEvent(hStop);
				break;
			}
			{
				std::lock_guard<std::mutex> lk(channel.mtx);
				std::memcpy(arr.data(), data, sizeof(T) * streamWidth);
				channel.dataQueue.push(arr);
			}

			SetEvent(channel.event);
		}
	}

	//QA: //Pipelined // Should be lock free queues for performance @ < 500ns latency

	void dispatcherLoop()
	{
		//source node.
		if (upstreamChannels_.empty()) {
			while (WaitForSingleObject(hStop, 1) == WAIT_TIMEOUT) {
				// Generate or fetch data as a source

				std::array<T, streamWidth> dummydata = { 0 };

				std::array<T, streamWidth> data = core(dummydata); 
				// Route to all downstreams if any
				for (auto& kv : writerMap_) {
					routeDataSource(kv.first, data);
				}
				// Sleep or throttle as needed
			}
			return;
		}

		// sink
		if (writerMap_.empty()) {
			std::vector<HANDLE> waitHandles;
			for (auto& up : upstreamChannels_)
				waitHandles.push_back(up.event);
			waitHandles.push_back(hStop);

			while (true) {
				DWORD idx = WaitForMultipleObjects(
					static_cast<DWORD>(waitHandles.size()), waitHandles.data(),
					FALSE, INFINITE);

				if (idx == WAIT_FAILED || idx == waitHandles.size() - 1) // hStop
					break;

				UpstreamChannel& src = upstreamChannels_[idx];
				std::array<T, streamWidth> data;
				{
					std::lock_guard<std::mutex> lk(src.mtx);
					if (src.dataQueue.empty())
						continue;
					data = std::move(src.dataQueue.front());
					src.dataQueue.pop();
				}
				// Process data, but do not route
				core(data);
			}
			return;
		}

		// filter node: both upstream and downstream present - this logic considers 1:1 mapping 
		std::vector<HANDLE> waitHandles;
		for (auto& up : upstreamChannels_)
			waitHandles.push_back(up.event);
		waitHandles.push_back(hStop);

		while (true) {
			DWORD idx = WaitForMultipleObjects(
				static_cast<DWORD>(waitHandles.size()), waitHandles.data(),
				FALSE, INFINITE);

			if (idx == WAIT_FAILED || idx == waitHandles.size() - 1) // hStop
				break;

			UpstreamChannel& src = upstreamChannels_[idx];
			std::array<T, streamWidth> data;
			{
				std::lock_guard<std::mutex> lk(src.mtx);
				if (src.dataQueue.empty())
					continue;
				data = std::move(src.dataQueue.front());
				src.dataQueue.pop();
			}
			std::array<T, streamWidth> processed_data = core(data);
			routeData(src.pipe, processed_data);
		}
	}


	void routeDataSource(HANDLE downstream, std::array<T, streamWidth>& data)
	{
		auto it = writerMap_.find(downstream);
		if (it == writerMap_.end()) return;
		auto& downChannel = it->second;
		{
			std::lock_guard<std::mutex> lk(downChannel.mtx);
			downChannel.dataQueue.push(data);
		}
		SetEvent(downChannel.event);
	}


	//QA: //Pipelined -  Event based // for performance @ < 500ns latency

	void routeData(HANDLE upstream,std::array<T,streamWidth>& data)
	{
		HANDLE downstream = routingMap_.at(upstream);
		auto& downChannel = writerMap_.at(downstream);
		{
			std::lock_guard<std::mutex> lk(downChannel.mtx);
			downChannel.dataQueue.push(std::move(data));
		}
		SetEvent(downChannel.event);
	}

	//QA: //Pipelined -  Event based  // for performance @ < 500ns latency 

	void downstreamWriterThreadFunc(DownstreamChannel& channel)
	{
		while (true)
		{
			DWORD waitResult = WaitForSingleObject(channel.event, INFINITE);
			if (waitResult != WAIT_OBJECT_0)
			{
				std::cerr << "WaitForSingleObject failed: " << GetLastError() << std::endl;
				break;
			}

			std::unique_lock<std::mutex> lk(channel.mtx);

			if (channel.stop && channel.dataQueue.empty())
			{
				break;
			}

			if (channel.dataQueue.empty())
			{
				ResetEvent(channel.event);
				continue;
			}

			std::array<T,streamWidth> data = std::move(channel.dataQueue.front());
			channel.dataQueue.pop();

			if (channel.dataQueue.empty())
			{
				ResetEvent(channel.event);
			}

			lk.unlock();

			DWORD written = 0;
			BOOL ok = WriteFile(channel.pipe,
				data.data(),
				static_cast<DWORD>(sizeof(T)*streamWidth),
				&written,
				nullptr);
			if (!ok || written != (sizeof(T) * streamWidth))
			{
				std::cerr << "Error writing to downstream pipe: " << GetLastError() << std::endl;
			}

		}
	}

	std::wstring GetOwnPath()
	{
		wchar_t buf[MAX_PATH];
		DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
		return std::wstring(buf, len);
	}

	std::wstring GetOwnFileName()
	{
		std::wstring path = GetOwnPath();
		size_t pos = path.find_last_of(L"\\/");

		std::wstring fileName = (pos == std::wstring::npos)
			? path
			: path.substr(pos + 1);

		std::size_t dotPos = fileName.find_last_of(L'.');
		if (dotPos != std::wstring::npos)
			fileName.erase(dotPos);

		return fileName;
	}

	uint8_t upStreamCount, downStreamCount;

	std::wstring procName;

	std::atomic<bool> requestUpstreamSetup{ false };
	std::atomic<bool> requestDownstreamSetup{ false };
	std::atomic<bool> running{ true };
	std::thread ioSetupThread;

protected:
	ProcessCLI processCLI;

	HANDLE getAuxEvent(const std::wstring& name)
	{
		auto it = auxChannels_.find(name);
		return (it != auxChannels_.end()) ? it->second.event : nullptr;
	}

	bool popAuxData(const std::wstring& name, std::vector<uint8_t>& out)
	{
		auto it = auxChannels_.find(name);
		if (it == auxChannels_.end()) return false;
		std::lock_guard<std::mutex> lk(it->second.mtx);
		if (it->second.dataQueue.empty()) return false;
		out = std::move(it->second.dataQueue.front());
		it->second.dataQueue.pop();
		return true;
	}

	void sendAux(const std::wstring& name, const std::vector<uint8_t>& data)
	{
		auto it = auxChannels_.find(name);
		if (it == auxChannels_.end()) return;
		AuxChannel& aux = it->second;
		if (!aux.enabled || aux.dir != interfaceDirection::DOWNSTREAM) return;
		{
			std::lock_guard<std::mutex> lk(aux.mtx);
			aux.dataQueue.push(data);
		}
		SetEvent(aux.event);
	}

	void enableAux(const std::wstring& name, bool enable, interfaceDirection dir)
	{
		enableAuxChannel(name, enable, dir);
	}




public:

	StreamProcess(InterfaceParam_s* clientParams,char* cmdLine)
		: ProcessClient<wchar_t>(GetOwnFileName(), clientParams)
	{
		upStreamCount = downStreamCount = 0;

		procName = GetOwnFileName();

		try {
			if (processCLI.parseJson(cmdLine))
			{
				processCLI.findProcessConfigByName(procName);
				processCLI.parseArgs();
			}
		}
		catch (const std::exception& ex)
		{
			std::cerr << "Exception: " << ex.what() << std::endl;
		}

		running = true;
		ioSetupThread = std::thread(&StreamProcess::ioSetupLoop, this);
	}

	void setupUpstream()
	{
		uint8_t count=0;
		//Data upstreams
		for (const auto& interface_ : processCLI.getProcessConfig().interfaces)
		{
			if (!interface_.upstream.empty() && interface_.interfaceType == L"Data")
			{
				upStreamCount++;
				HANDLE upHandle = connectUpStreamInterface(interface_.upstream);

				if (upHandle != INVALID_HANDLE_VALUE)
				{
					upstreamChannels_.push_back(UpstreamChannel(upHandle));
					upstreamChannelStatus.push_back(true);
				}
				else
				{
					upstreamChannelStatus.push_back(false);
				}
			}
		}

		for (uint8_t i = 0; i < upstreamChannelStatus.size(); i++)
		{
			if (upstreamChannelStatus[i]) count++;
		} 

		std::wcout << count << L" upstream channels connected" << std::endl;
	}

	void setupDownstream()
	{
		uint8_t count = 0;
		for(const auto& interface_ : processCLI.getProcessConfig().interfaces)
		{
			if (!interface_.downstream.empty() && interface_.interfaceType == L"Data")
			{
				downStreamCount++;
				HANDLE downHandle = createDownStreamInterface(interface_.downstream);
				
				if (downHandle != INVALID_HANDLE_VALUE)
				{
					writerMap_.emplace(std::piecewise_construct,
						std::forward_as_tuple(downHandle),
						std::forward_as_tuple(downHandle));
					downstreamChannelStatus.push_back(true);
				}
				else
				{
					downstreamChannelStatus.push_back(false);
				}
			}
		}

		for (uint8_t i = 0; i < downstreamChannelStatus.size(); i++)
		{
			if (downstreamChannelStatus[i]) count++;
		}

		std::wcout << count << L" downstream channels connected" << std::endl;
	}

	void setupRoutingMap()
	{
		// setup routing map
		// for simplicity, assuming 1:1 mapping in order of appearance
		for (size_t i = 0; i < upstreamChannels_.size() && i < writerMap_.size(); ++i)
		{
			routingMap_[upstreamChannels_[i].pipe] = std::next(writerMap_.begin(), i)->first;
		}
	}

	void ioSetupLoop()
	{
		while (running)
		{
			if (requestUpstreamSetup.exchange(false))
			{
				setupUpstream();
			}
			if (requestDownstreamSetup.exchange(false))
			{
				setupDownstream();
			}

		}
	}

	std::map<std::string,std::string> getArgs()
	{
		return processCLI.getArgs();
	}


	void start()
	{
		for (auto& up : upstreamChannels_)
		{
			up.event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			up.readerThread = std::thread(&StreamProcess::upstreamReaderThreadFunc, this, std::ref(up));
		}

		for (auto& kv : writerMap_)
		{
			auto& down = kv.second;
			down.event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			down.writerThread = std::thread(&StreamProcess::downstreamWriterThreadFunc, this, std::ref(down));
		}

		hStop = CreateEvent(nullptr, TRUE, FALSE, nullptr);
		dispatcherThread_ = std::thread(&StreamProcess::dispatcherLoop, this);

		startAuxChannels();
	}

	void stop()
	{
		SetEvent(hStop);

		for (auto& up : upstreamChannels_)
		{
			up.readerThread.join();
			CloseHandle(up.event);
		}
		for (auto& kv : writerMap_)
		{
			auto& down = kv.second;
			{
				std::lock_guard<std::mutex> lk(down.mtx);
				down.stop = true;
			}
			SetEvent(down.event);
			if (down.writerThread.joinable())
				down.writerThread.join();
			CloseHandle(down.event);
		}

		if (dispatcherThread_.joinable())
		{
			dispatcherThread_.join();
		}

		CloseHandle(hStop);

		stopAuxChannels();
	}

	virtual ~StreamProcess()
	{

		running = false;
		if (ioSetupThread.joinable())
			ioSetupThread.join();

		this->disconnectFromPlatform();
		stop();
	}
 
	void update(wchar_t commandId) override
	{
		// execute 
		std::wcout << L" Stream Process executing command Id " << commandId << std::endl;
		// process the commandId and perform actions accordingly

		/* Commands to Process*/
		/*
		*
		* Start Process Client with status
		*
		* Stop Process Client with status
		*
		* Connect to Upstream Client with status
		*
		* Connect to Downstream Client with status
		*
		* Ping Process Client
		*
		*/

		switch (commandId)
		{
			case 'Q': start(); break;
			case 'X': stop(); break;
			case 'U': requestUpstreamSetup = true; break;
			case 'D': requestDownstreamSetup = true; break;
			case 'M': setupRoutingMap(); break;
			case 'P': ProcessClient::update(commandId); break;
			default: stop(); break;
		}
	}

protected:
	virtual std::array<T,streamWidth> core(std::array<T,streamWidth>& data) = 0;

};