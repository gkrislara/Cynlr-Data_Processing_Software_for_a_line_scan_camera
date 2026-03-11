#pragma once

/* Windows Platform based Process Management*/

#include <windows.h>
#include <tlHelp32.h>

#include "..\Shared\ProcessCommon.h"
#include "..\Shared\IOServer.h"

#include <map>
#include <queue>
#include <iostream>


template<typename T>
class ProcessManager: public PlatformBase<T>{
private:
	IOServer<T, 1> server;

	std::map<std::wstring,PROCESS_INFORMATION> processes;


	std::queue<QueuedCmd<T>> cmdQueue_;
	std::mutex cmdMtx_;
	std::condition_variable cmdCv_;


	std::queue<std::vector<uint8_t>> statusQueue_;   
	std::mutex statusMtx_;                     
	std::condition_variable statusCv_;

	/* ----- ID generation ----- */

	std::unordered_map<DWORD, int> pendingCommands_;
	std::mutex pendingMtx_;
	std::condition_variable pendingCv_;

	std::atomic<bool> running_{ false };
	std::thread dispatchThread_;

	bool status_ = false;

public:

	~ProcessManager() {
		stop();
	}

	bool setup(InterfaceParam_s* serverParam, bool status)
	{
		server.setup(serverParam);
		status_ = status;
		return server.start();
	}

	bool start()
	{
		if (running_) return false;
		running_ = true;

		dispatchThread_ = std::thread(&ProcessManager<T>::dispatchThread, this);
		return true;
	}

	void stop()
	{
		dispatchThread_.join();
		server.stop();
		/* TODO: Closee all the processes */
		/* clear processes map*/
	}

	bool attachProcesses() override {

		std::vector<DWORD> ids;
		ids.reserve(processes.size());            

		for (const auto& proc : processes) {
			ids.push_back(proc.second.dwProcessId);
		}


		if (server.asyncConnect(ids))
		{
			/* FUTURE: populate handles in processes map after checking gClients PID.  This is to ensure IOServer Layer and Process Manager Layer are in sync*/
		}
		else
		{
			/* set error */
		}
		return true;


	}

	bool spawnProcess(std::wstring processName,std::wstring arglist) {

		/* newly created */
			/* createprocess */

		if(processes.find(processName) == processes.end())
		{
			wchar_t buf[MAX_PATH];
			GetModuleFileNameW(nullptr, buf, MAX_PATH);
			std::wstring dir(buf);
			dir = dir.substr(0, dir.find_last_of(L"\\/"));

			std::wstring exePath = dir + L"\\" + processName + L".exe";

			std::vector<wchar_t> argList(arglist.begin(), arglist.end());

			//TODO: Combine Arglists here

			argList.push_back(L'\0'); // null-terminate

			STARTUPINFOW si{ sizeof(si) };
			PROCESS_INFORMATION pi;

			if (CreateProcessW(
				exePath.c_str(),    // <— application name
				argList.data(),            // <— command line 
				nullptr, nullptr, FALSE,
				CREATE_NEW_CONSOLE,
				nullptr, nullptr,
				&si, &pi))
			{
				processes.emplace(processName, pi);
			}
			else
				return false;
		}

		/* else the process is already registered, do maintainance instead */

		return true;
	}

	void enqueueCommand(std::wstring processName, T commandId, const std::vector<uint8_t>& payload = {}, std::vector<uint8_t>* status = nullptr)
	{

		/* processName -> clientId */
		DWORD clientId;

		auto it = processes.find(processName);
		if (it != processes.end())
		{
			clientId = it->second.dwProcessId;
		}
		else
		{
			std::wcerr << "Process not found: " << std::wstring(processName) << std::endl;
			return;
		}

		{
			std::lock_guard<std::mutex> lk(cmdMtx_);
			cmdQueue_.push({ clientId, commandId, payload });
		}
		cmdCv_.notify_one();

		{
			std::lock_guard<std::mutex> lk(pendingMtx_);
			pendingCommands_[clientId]++;
		}

		if (status)
		{
			{
				std::unique_lock<std::mutex> lk(statusMtx_);
				statusCv_.wait(lk, [this] { return !statusQueue_.empty();
					});
			}

			*status = std::move(statusQueue_.front());
			statusQueue_.pop();
		}

	}


	void dispatchThread()	
	{
		while (running_) {

			/* TODO: SRP the subutlities*/

			std::unique_lock<std::mutex> lk(cmdMtx_);
			
			cmdCv_.wait(lk, [this] {
				return !cmdQueue_.empty() || !running_; 
				});

			if (!running_) break;

			/* get the command at the head of the queue */
			QueuedCmd<T> cmd = std::move(cmdQueue_.front());
			cmdQueue_.pop();
			lk.unlock();

			// Build the envelope + optional payload
			CmdEnvelope<T> env{
				cmd.clientId,
				cmd.commandId,
				static_cast<uint16_t>(cmd.payload.size())
			};

			std::vector<uint8_t> packet(sizeof(env) + cmd.payload.size());
			
			memcpy(packet.data(), &env, sizeof(env));
			
			if (!cmd.payload.empty())
				memcpy(packet.data() + sizeof(env), cmd.payload.data(),
					cmd.payload.size());

			// Write the packet
			std::vector<uint8_t> reply;

			if (!server.TransactPipeWithTimeout(cmd.clientId,
				packet.data(),
				static_cast<DWORD>(packet.size()),
				reply,
				5000))
			{

				std::lock_guard<std::mutex> lk(pendingMtx_);
				if (--pendingCommands_[cmd.clientId] == 0)
					pendingCv_.notify_all();
				continue;
			}

			if (status_)
			{
				{
					std::lock_guard<std::mutex> lk(statusMtx_);
					statusQueue_.push(std::move(reply));
				}
				statusCv_.notify_one();
			}

			{
				std::lock_guard<std::mutex> lk(pendingMtx_);
				if (--pendingCommands_[cmd.clientId] == 0) 
				{
						pendingCv_.notify_all();
				}
			}

		}
	}



	void maintainProcesses()
	{
		/* do the maintainance, health checks and robustness */
		/* use notifyProcess wherever necessary for broadcast */
		/* TODO: break this into Health with timeout, (thread) broadcast when error from any client, */

		// Thread to 




	}

};


/*
	bool suspendProcess(std::wstring processName)
		{
	PROCESS_INFORMATION procInfo = processes[processName].first;

	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (snap == INVALID_HANDLE_VALUE) return;

	THREADENTRY32 te{ sizeof(te) };
	if (Thread32First(snap, &te)) {
		do {
			if (te.th32OwnerProcessID == procInfo.dwProcessId) {
				HANDLE th = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
				if (th) {
					SuspendThread(th);
					CloseHandle(th);
				}
			}
		} while (Thread32Next(snap, &te));
	}
	CloseHandle(snap);
		}

		bool resumeProcess(std::wstring processName)
		{

			PROCESS_INFORMATION procInfo = processes[processName].first;

			HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
			if (snap == INVALID_HANDLE_VALUE) return;

			THREADENTRY32 te{ sizeof(te) };
			if (Thread32First(snap, &te)) {
				do {
					if (te.th32OwnerProcessID == procInfo.dwProcessId) {
						HANDLE th = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
						if (th) {
							ResumeThread(th);
							CloseHandle(th);
						}
					}
				} while (Thread32Next(snap, &te));
			}
			CloseHandle(snap);
		}
*/


///* IPC based subject */
//class ProcessManager : public PlatformBase {
//	uint8_t  m_value;
//	std::vector<ProcessBase*>  m_views;
//	IOServer<uint8_t, 1> dst;
//public:
//	void attach(ProcessBase* obs) override {
//		m_views.push_back(obs);
//		/* register inputs as well*/
//	}
//	void set_val(uint8_t value) override {
//		m_value = value;  notify();
//	}
//	void notify() override {
//		for (int i = 0; i < m_views.size(); ++i)
//			m_views[i]->update(&m_value);
//	}
//};
//