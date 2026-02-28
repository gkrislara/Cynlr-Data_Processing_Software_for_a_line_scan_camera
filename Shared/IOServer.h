#pragma once

/* Windows Only*/

#include "IOInterface.h"
#include "windows.h"
#include <thread>
#include <mutex>
#include <map>

/* TODO: Move this to separate Unified Error Code header*/

typedef enum class IOServerError {
	SUCCESS,
	ERR_IOSERVER_CREATE_INTERFACE_FAILED, // TODO: Implement GLE
	ERR_IOSERVER_CONNECT_TO_INTERFACE_EVENT_FAILED, // TODO: Implement GLE
    ERR_IOSERVER_GLE_ASYNC_CONNECT_WAIT_FOR_OBJECT_FAILED,
    ERR_IOSERVER_GLE_HANDSHAKE_CLIENT_HEADER_RECEIVE_ERROR,
    ERR_IOSERVER_HANDSHAKE_SERVER_RESPONSE_FAILED,
    ERR_IOSERVER_HANDSHAKE_INCORRECT_CLIENT_ID,
    ERR_IOSERVER_SENDDATATOCLIENT_GLE_WRITEFILE_FAIL,
    ERR_IOSERVER_SENDDATATOCLIENT_CLIENT_UNAVAILABLE,
    ERR_IOSERVER_HANDSHAKE_INCORRECT_NUMBER_OF_CLIENTS,
	NUM_IOSERVER_ERR
}IOServerError_e;

#define BUFSIZE 512 // parameterise this in  serverParams

/* purpose : create a named pipe server */
template<typename T, size_t dataWidth>
class IOServer : public IOInterfaceBase<T, dataWidth> {
private:
    InterfaceParam_s serverParam;
    std::thread serverThread;
    std::atomic<int> clientCounter;
    bool running = false;
    std::map <HANDLE, std::thread> clientThreads;
    
    std::vector <HANDLE> clientHandles;
    
    std::mutex mtx;
    T* buffer;

    struct intfInstance {
        HANDLE      intf = INVALID_HANDLE_VALUE;
        HANDLE      hEvent = nullptr;          // overlapped event
        OVERLAPPED  ov = {};
    };

#pragma pack(push, 1)
    struct registerMsg {
        uint32_t type;      // 0x0001
        DWORD Id;  // length in wchar_t units
        // followed by nameLen wchar_t characters (no terminating L'\0')
    };
#pragma pack(pop)


    std::vector<intfInstance>      intfInstances;
    std::vector<HANDLE>            waitEvents;          // events we actually wait on
    std::vector<size_t>            idxMap;              // wait‑event index → instance index
    std::unordered_map<DWORD, HANDLE> clients;      // clientId → pipe handle
    std::mutex                     clientsMtx;
    uint16_t                       nextClientId;

    IOServerError_e currentError;

    bool mode; //Future


    bool createInterface(intfInstance& pi)
    {
        if (serverParam.interfaceType == IOInterfaceType::NAMED_PIPE_SERVER)
        {
            pi.intf = CreateNamedPipeW(
                serverParam.namedPipeServerParam.pipeName.c_str(),
                serverParam.namedPipeServerParam.accessType,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                serverParam.namedPipeServerParam.maxClients,
                serverParam.namedPipeServerParam.pipeBufferSize,
                serverParam.namedPipeServerParam.pipeBufferSize,
                0,
                nullptr
            );
        }

        if (pi.intf == INVALID_HANDLE_VALUE)
            return false;
        
        return true;
    }

    bool asyncConnectEvent(intfInstance& pi)
    {
        pi.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!pi.hEvent)
        {
            CloseHandle(pi.intf);
            pi.intf = INVALID_HANDLE_VALUE;
            return false;
        }

        pi.ov = {};
        pi.ov.hEvent = pi.hEvent;

        if (!ConnectNamedPipe(pi.intf, &pi.ov) && GetLastError() != ERROR_IO_PENDING)
        {
            // pipe connected already
            SetEvent(pi.hEvent);
        }
        return true;

    }
    
    void cleanupInstance(size_t waitIdx, size_t instIdx)
    {
        /* close, remove and recreate */

        intfInstance& pi = intfInstances[instIdx];

        if (pi.intf != INVALID_HANDLE_VALUE)
        {
            CloseHandle(pi.intf);
            pi.intf = INVALID_HANDLE_VALUE;
        }

        if (pi.hEvent)
        {
            CloseHandle(pi.hEvent);
            pi.hEvent = nullptr;
        }

        /* erase the wait events */
        waitEvents.erase(waitEvents.begin() + waitIdx);
        idxMap.erase(idxMap.begin() + waitIdx);

        if (waitEvents.size() + clients.size() < static_cast<size_t>(serverParam.namedPipeServerParam.maxClients))
        {
            intfInstance& repl = intfInstances[instIdx];
            if (createInterface(repl))
            {
                if (asyncConnectEvent(repl))
                {
                    waitEvents.insert(waitEvents.begin() + waitIdx, repl.hEvent);
                    idxMap.insert(idxMap.begin() + waitIdx, instIdx);
                }
            }
        }
    }

public:
    IOServer()
    {
		clientCounter = 0;
        buffer = new T[dataWidth];
    }

    ~IOServer() {
        stop();
        delete buffer;
    }

    bool start() override
    {
        currentError = IOServerError::SUCCESS;

        /* Create Pipes and Async Accept */
        for (int i = 0; i < serverParam.namedPipeServerParam.maxClients; ++i)
        {
            if (!createInterface(intfInstances[i]))
            {
                currentError = IOServerError::ERR_IOSERVER_CREATE_INTERFACE_FAILED;
                continue;
            }
            if (!asyncConnectEvent(intfInstances[i]))
            {
                currentError = IOServerError::ERR_IOSERVER_CONNECT_TO_INTERFACE_EVENT_FAILED;
                continue;
            }
            waitEvents.push_back(intfInstances[i].hEvent);
            idxMap.push_back(i);
        }
        if (currentError == IOServerError::SUCCESS) return true;
        else return false;
    }

    bool stop() override 
    {
        /* close and remove */
        for (auto& pi : intfInstances) {
            if (pi.intf != INVALID_HANDLE_VALUE) CloseHandle(pi.intf);
            if (pi.hEvent) CloseHandle(pi.hEvent);
        }
        {
            std::lock_guard<std::mutex> lk(clientsMtx);
            for (auto& kv : clients) {
                CloseHandle(kv.second);
            }
            clients.clear();
        }
        return true;
    }


    void setup(InterfaceParam_s* param) override {

        memcpy(&serverParam, param, sizeof(serverParam));
        
        if (serverParam.interfaceType == IOInterfaceType::NAMED_PIPE_SERVER)
        {
            intfInstances.resize(serverParam.namedPipeServerParam.maxClients);
            nextClientId = 1;
        }

    }

    bool checkClient(HANDLE clientHandle)
    {
        auto it = std::find(clientHandles.begin(),clientHandles.end(),clientHandle);
        
        if (it != clientHandles.end())
            return true;
        return false;
    }


    bool sendDataToClient(HANDLE clientHandle,std::vector<T> message)
    {
        currentError = IOServerError::SUCCESS;

        /* TODO: Change writefile to TransactNamedPipe for Reliable Mode*/
        std::lock_guard<std::mutex> lock(mtx);

        if (clientHandle != INVALID_HANDLE_VALUE) 
        {
            DWORD cbWritten;
            // blocking file write
            //memcpy(buffer, message, sizeof(T)); // this is generic memcpy

            if (WriteFile(clientHandle, message.data(), message.size(), &cbWritten, NULL)) {
                return true;
            }
            else {
                currentError = IOServerError::ERR_IOSERVER_SENDDATATOCLIENT_GLE_WRITEFILE_FAIL;
                return false;
            }
        }
        currentError = IOServerError::ERR_IOSERVER_SENDDATATOCLIENT_CLIENT_UNAVAILABLE;
        return false;
    }


    bool sendData(void* data) override   // sends data to all clients 
    {
        std::lock_guard<std::mutex> lock(mtx);
		std::vector<T> data_;
		data_ = *((std::vector<T>*)data);
        for (const auto& handle : clientHandles) {
            sendDataToClient(handle, data_);
            /*
                Add exceptions
            */
        }
        return true;
	}

    bool receiveData(T& cmdId,std::vector<T>& data) override {
        // Not implemented for server
        return false;
    }

    bool asyncConnect(const std::vector<DWORD>& clientIds)
    {    
        // TODO: SRPise the block of code
        /* WaitForMultipleObjects */
        currentError = IOServerError::SUCCESS;

        if (serverParam.interfaceType == IOInterfaceType::NAMED_PIPE_SERVER)
        {   
            std::wstring pn;

            while (static_cast<int>(clients.size()) < serverParam.namedPipeServerParam.maxClients)
            {
                if (waitEvents.empty()) break;
                
                DWORD waitIdx = WaitForMultipleObjects(
                static_cast<DWORD>(waitEvents.size()),
                waitEvents.data(),
                FALSE, //wait for any event
                INFINITE); // no timeout; Assumption: clients are guaranteed to connect

                if (waitIdx == WAIT_FAILED)
                {
                    currentError = IOServerError::ERR_IOSERVER_GLE_ASYNC_CONNECT_WAIT_FOR_OBJECT_FAILED;
                    break;
                }

                size_t instIdx = idxMap[waitIdx];
                intfInstance& pi = intfInstances[instIdx];
                HANDLE hPipe = pi.intf;

                registerMsg hdr{};
                std::vector<BYTE> request(sizeof(hdr));
                DWORD bytesRead = 0;
                BOOL ok = ReadFile(hPipe, request.data(), static_cast<DWORD>(request.size()),
                    &bytesRead, nullptr);

                if (!ok)
                {
                    currentError = IOServerError::ERR_IOSERVER_GLE_HANDSHAKE_CLIENT_HEADER_RECEIVE_ERROR;
                    cleanupInstance(waitIdx, instIdx);
                    continue;
                }

                
                std::memcpy(&hdr, request.data(), sizeof(hdr));

                // verify whether the id matches

                if (hdr.type != 0x0001) {
                    cleanupInstance(waitIdx, instIdx);
                    continue;
                }

                // verify size
                size_t expectedSize = sizeof(hdr);
                if (bytesRead < expectedSize)
                {
                    cleanupInstance(waitIdx, instIdx);
                    continue;
                }

				DWORD clientId;
				auto it = std::find(clientIds.begin(), clientIds.end(), hdr.Id);
                if (it != clientIds.end()) clientId = hdr.Id;
                else 
                {
                    currentError = IOServerError::ERR_IOSERVER_HANDSHAKE_INCORRECT_CLIENT_ID;
                    cleanupInstance(waitIdx, instIdx);
                    continue;
				}

                //Respond Back;

                //uint16_t clientId = nextClientId++; // get pid instead until maxcount
                DWORD byteswritten = 0;

				//write back the client id - will be useful for upgrading to diffie hellman like key exchange later and extend to stream mode

                ok = WriteFile(hPipe, &clientId, sizeof(clientId), &byteswritten, nullptr);

                if (!ok)
                {
                    currentError = IOServerError::ERR_IOSERVER_HANDSHAKE_SERVER_RESPONSE_FAILED;
                    cleanupInstance(waitIdx, instIdx);
                    continue;
                }

                // Client handshake ; Add to list
                {
                    std::lock_guard<std::mutex> lk(clientsMtx);
                    clients.emplace(clientId, hPipe); 
                }
 
                /* Remove from wait set */
                waitEvents.erase(waitEvents.begin() + waitIdx);
                idxMap.erase(idxMap.begin() + waitIdx);
            }
        }

        if (currentError == IOServerError::SUCCESS) return true;
        else return false;
    }

    IOServerError_e getServerLastError()
    {
        return currentError;
    }

    bool TransactPipeWithTimeout(DWORD clientId, const void* outBuf,
        DWORD outSize, std::vector<uint8_t>& inBuf,
        DWORD timeoutMs = 5000)          // 5 s default
    {
        // Prepare a reply buffer (adjust size if you expect larger messages)
        inBuf.resize(4096);
        DWORD bytesRead = 0;

        OVERLAPPED ov = { 0 };
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!ov.hEvent) return false;


        /* get the client's hpipe 
        This is the true registration within Server's context
        Either unify everything under lookup or create a map between localclients and clientlookup 
        */
        HANDLE targetPipe = INVALID_HANDLE_VALUE;
        {
            std::lock_guard<std::mutex> lk(clientsMtx);
            auto it = clients.find(clientId);
        	if (it != clients.end())
        		targetPipe = it->second;
        }

        if (targetPipe == INVALID_HANDLE_VALUE) return false;

        BOOL ok = TransactNamedPipe(
            targetPipe,
            const_cast<LPVOID>(outBuf),   // request buffer (non‑const required)
            outSize,
            inBuf.data(),                  // reply buffer
            static_cast<DWORD>(inBuf.size()),
            &bytesRead,
            &ov);                          // overlapped → non‑blocking

        if (!ok && GetLastError() != ERROR_IO_PENDING) {
            CloseHandle(ov.hEvent);
            return false;                 // immediate failure
        }

        // Wait for completion or timeout
        DWORD wait = WaitForSingleObject(ov.hEvent, timeoutMs);
        if (wait == WAIT_TIMEOUT) {
            // Cancel the pending I/O and treat as failure/timeout
            CancelIoEx(targetPipe, &ov);
            CloseHandle(ov.hEvent);
            return false;
        }

        // Completed – retrieve the result
        if (!GetOverlappedResult(targetPipe, &ov, &bytesRead, FALSE)) {
            CloseHandle(ov.hEvent);
            return false;
        }

        CloseHandle(ov.hEvent);
        inBuf.resize(bytesRead);
        return true;
    }
};



/*
        void listeningThread()
        {
            // TODO: Change this is wait for multiple objects 
    if (serverParam.interfaceType == IOInterfaceType::NAMED_PIPE_SERVER)
    {
        while (running) {

            HANDLE hPipe = CreateNamedPipe(
                serverParam.namedPipeServerParam.pipeName.c_str(),
                serverParam.namedPipeServerParam.accessType,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES,
                BUFSIZE,
                BUFSIZE,
                0,
                NULL);

            if (hPipe == INVALID_HANDLE_VALUE) {
                // std::cout << "CreateNamedPipe Error:" << GetLastError();
                continue;
            }

            // add to the list
            if (ConnectNamedPipe(hPipe, NULL) != FALSE || GetLastError() == ERROR_PIPE_CONNECTED) {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    clientHandles.push_back(hPipe);
                    clientThreads[hPipe] = std::thread(&IOServer::handleClient, this, hPipe);
                }
            }
            else {
                CloseHandle(hPipe);
            }
        }

    }
        }

        void handleClient(HANDLE hPipe)
        {

            std::wcout << L"Client connected on thread: " << std::this_thread::get_id() << std::endl;

            // This thread now just keeps the pipe open.
                // The subject will write, and the client will read.
                // not implemented
            while (running) {
                DWORD bytesRead;
                char buffer;
                // PeekNamedPipe - Non Blocking Placeholder
                if (PeekNamedPipe(hPipe, &buffer, 1, &bytesRead, NULL, NULL) && bytesRead == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                else {
                    // A production version might use async I/O or a separate reader thread.
                }

                // Check if the client disconnected.
                DWORD state;
                if (!GetNamedPipeHandleState(hPipe, &state, NULL, NULL, NULL, NULL, 0) && GetLastError() == ERROR_BROKEN_PIPE) {
                    break;
                }
            }
            std::wcout << L"Client disconnected." << std::endl;

            {
                std::lock_guard<std::mutex> lock(mtx);
                clientHandles.erase(std::remove(clientHandles.begin(), clientHandles.end(), hPipe), clientHandles.end());

                if (clientThreads.count(hPipe)) {
                    clientThreads.erase(hPipe);
                }
            }

            FlushFileBuffers(hPipe);
            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
        }

        void cleanup()
        {
            std::lock_guard<std::mutex> lock(mtx);
            for (auto& pair : clientThreads) {
                if (pair.second.joinable()) {
                    pair.second.join();
                }
            }
            clientThreads.clear();
            clientHandles.clear();
        }
*/