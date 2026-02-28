#pragma once

#include "..\Shared\ProcessCommon.h"
#include "..\Shared\IOClient.h"
#include <thread>
#include <vector>

/* Acts as the Observer spinoff- can be used as Platform Extended utility for Monitoring*/

template <typename T>
class ProcessClient {
private:
    IOClient<T,1> client;
    std::wstring processName;
    ProcessState_e processState;
    bool running;
    std::thread listenerThread;
    std::vector<T> data; //is template necessary in this case?
    
	InterfaceParam_s clntParams;

    struct status {
        DWORD clientId;
        bool pass;
    }status_;

public:
    ProcessClient(std::wstring process, InterfaceParam_s* clientParams)
    {
        processName = process;
        processState = ProcessState::IDLE;
        running = true;
		clntParams = *clientParams;
        client.setup(clientParams);
    }
    ~ProcessClient() 
    {
        running = false;
        if (listenerThread.joinable()) {
            listenerThread.join();
        }
    }

    /* setup function that the apps must populate for logic init and function map*/
    

    bool connectToPlatform()
    {
        return client.start();
    }

    void disconnectFromPlatform()
    {
        running = false;
        if (listenerThread.joinable()) {
            listenerThread.join();
        }
        client.stop();
    }

    void listeningThreadFunc() {
        while (running) {
            T commandId;
            if (client.receiveData(commandId,data))
            {
                //std::cout << "client received Data with command Id " << commandId << std::endl;
                update(commandId);

				status_.clientId = clntParams.namedPipeClientParam.Id;
				status_.pass = true; // TODO: set based on execution result

				client.sendData(&status_);
            }
            // TODO: establish status semantics
            
        }
    }
    // T is preferably Enum
    virtual void update(T data) {
        // execute 
        std::wcout << " Process pinged command Id " << data << std::endl;
    }

    void startListeningToPlatform()
    {
        listenerThread = std::thread(&ProcessClient::listeningThreadFunc, this);
    }

};