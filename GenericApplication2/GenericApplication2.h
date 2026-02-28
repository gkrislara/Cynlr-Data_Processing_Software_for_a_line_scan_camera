#pragma once
#include "..\Shared\StreamProcess.h"

// Test - Done Ok
class GA2 : public StreamProcess<Pixel, 2> {
public:

	GA2(InterfaceParam_s* clientParams,char* cmdLine)
		: StreamProcess(clientParams, cmdLine)
	{
	}

	std::array<Pixel,2> core(std::array <Pixel, 2>& data) override
	{

		std::wcout << "Pixel[0]: " << " col: " << data[0].col << " row: " << data[0].row << " value: " << data[0].value << " label: " << data[0].label.labelValue << std::endl;

		std::wcout << "Pixel[1]: " << " col: " << data[1].col << " row: " << data[1].row << " value: " << data[1].value << " label: " << data[1].label.labelValue << std::endl;

		//Clear the internal buffer
		std::wcout.flush();

		return data;
	}

	void update(wchar_t commandId) override
	{
		// execute 
		std::wcout << L" GA2 executing command Id " << commandId << std::endl;

		/* start stop is implemented in stream process*/

		switch (commandId)
		{
			case 'Q': StreamProcess::update(commandId); break;
			case 'X': StreamProcess::update(commandId); break;
			case 'U': StreamProcess::update(commandId); break;
			case 'D': StreamProcess::update(commandId); break;
			case 'M': StreamProcess::update(commandId); break;
			case 'P': StreamProcess::update(commandId); break;
			default: StreamProcess::stop(); break;
		}
	}
};