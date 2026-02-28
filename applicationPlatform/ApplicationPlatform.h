#pragma once

#include "..\Shared\UserInterfacePlatform.h"
#include "..\Shared\ProcessManager.h"
#include <memory>

typedef struct {
	std::string userInterfaceSchema;
	/* add server Param here*/
	/* other parmas like m,t,v*/
}ApplicationPlatformParams;

/* purpose - SoC for UI and PM */

class ApplicationPlatformW {
	ProcessManager<wchar_t> processManager;
	std::wstring schemaPath;
	UserInterfacePlatform userInterfacePlatform;
	PlatformConfig_s platformConfig;
	UserInput_s userInput;

public:
	ApplicationPlatformW(ApplicationPlatformParams* platformParam)
	{
		if (!userInterfacePlatform.parseSchema(platformParam->userInterfaceSchema, nullptr)) {
			/* set error*/
		}
		schemaPath = std::wstring(platformParam->userInterfaceSchema.begin(), platformParam->userInterfaceSchema.end());
		userInterfacePlatform.getPlatformConfig(&platformConfig);
		userInterfacePlatform.registerInput();
	}


	~ApplicationPlatformW()
	{
		processManager.stop();
	}

	bool setupProcesses()
	{
		InterfaceParam_s serverParam;
		serverParam.interfaceType = IOInterfaceType::NAMED_PIPE_SERVER;
		serverParam.namedPipeServerParam.pipeName = DEFAULT_NAMED_PIPE_SERVER;
		serverParam.namedPipeServerParam.accessType = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
		serverParam.namedPipeServerParam.maxClients = platformConfig.processConfig.size();
		serverParam.namedPipeServerParam.pipeBufferSize = 4096;
		serverParam.namedPipeServerParam.connectTimeout = 5000;

		
		
		if (!processManager.setup(&serverParam, false))
		{
			return false;
		}

		for (ProcessConfig_s process : platformConfig.processConfig)
		{
			//check whether upstream stream process is enabled; the first process as upstream is also handled
			// Proposition / Precondition : Also at this point server would started and waiting for  clients to connect
			while (!processManager.spawnProcess(process.serviceName, schemaPath /*args*/ 
				));
		}
		if (processManager.attachProcesses())
		{
			// all processes are spawned and attached; start the command dispatcher
			processManager.start();
			return true;
		}
		else
			return false;

	}



	void run()
	{
		/* a continuous thread that provides runtime context */
		/* get user input via userInterface: something like userInterface.buttonClicked */
		/* userinterface.emit signal */
		/* signal event has occured -> send the message to appropriate slot */
		/* 
			register.notify(signal)
		*/

		/* 
		   get a confirmation whether all apps are intialised -> processbase contains all the inputs mapped to apps
		*/

		std::wcout << "Available commands: " << std::endl;

		std::wcout << "Q: Start Stream" << std::endl;
		std::wcout << "X: Stop Stream" << std::endl;
		std::wcout << "U: Connect Upstream " << std::endl;
		std::wcout << "D: Connect Downstream " << std::endl;
		std::wcout << "M: Map streams " << std::endl;
		std::wcout << "P: Ping Status " << std::endl;
		// order of commands
		
		std::wcout << std::endl;

		std::wcout << "Order of commands to enter for stream setup: " << std::endl;

		std::wcout << " LS_DGB : D" << std::endl;
		std::wcout << " LS_DFB : D" << std::endl;
		std::wcout << " LS_DLB : D" << std::endl;
		std::wcout << " LS_DAB : D" << std::endl;
		
		std::wcout << " LS_DLogB : U" << std::endl;
		std::wcout << " LS_DAB : U" << std::endl;
		std::wcout << " LS_DLB : U" << std::endl;
		std::wcout << " LS_DFB : U" << std::endl;

		std::wcout << " LS_DGB : M" << std::endl;
		std::wcout << " LS_DFB : M" << std::endl;
		std::wcout << " LS_DLB : M" << std::endl;
		std::wcout << " LS_DAB : M" << std::endl;
		std::wcout << " LS_DLogB : M" << std::endl;

		std::wcout << L"Enable streams " << std::endl;

		std::wcout << " LS_DLogB : Q" << std::endl;
		std::wcout << " LS_DAB : Q" << std::endl;
		std::wcout << " LS_DLB : Q" << std::endl;
		std::wcout << " LS_DFB : Q" << std::endl;
		std::wcout << " (Opt) LS_DGB : R/C" << std::endl;
		std::wcout << " LS_DGB : Q" << std::endl;
		
		while (true) {
			/*1. User Input Enqueue */
			UserInput_s userIn;
			if (userInterfacePlatform.inputContext(userIn))
			{
				processManager.enqueueCommand(userIn.processName, userIn.paramShort);
			}
			else
			{
				std::wcout << "unregistered input, try again" << std::endl;
			}
		}

		/*2. Process Monitoring and logging*/
		/*processManager.stop();*/

	}

};