#pragma once

#include "..\Shared\StreamProcess.h"

// Test
class GA1 : public StreamProcess<Pixel,2> {

	uint32_t M;
	float V;
	uint32_t T;
	char* CSV_file;

public:

	GA1(InterfaceParam_s* clientParams,char* cmdLine)
		: StreamProcess(clientParams, cmdLine)
	{
		std::map<std::string, std::string> args = processCLI.getArgs();

		/* print each arg*/

		std::wcout << L"GA1 Process Args: " << std::endl;
		for (const auto& arg : args) 
		{
			std::wcout << L"  " << arg.first.c_str() << ": " << arg.second.c_str() << std::endl;
		}

		// parse each argument and store them,

		if (args.find("M") != args.end())
			M = std::stoul(args["M"]);
		if (args.find("V") != args.end())
			V = std::stof(args["V"]);
		if (args.find("T") != args.end())
			T = std::stoul(args["T"]);

		if(args.find("CSV_FILE") != args.end())
			CSV_file = const_cast<char*>(args["CSV_FILE"].c_str());

		std::wcout << L" M: " << M << L" V: " << V << L" T: " << T << L" CSV: " << CSV_file << std::endl;

	}

	std::array<Pixel,2> core(std::array<Pixel,2>& data) override
	{

		data[0].col = 1;
		data[0].row = 1;
		data[0].label = 1;
		data[0].value = 250;

		data[1].col = 2;
		data[1].row = 2;
		data[1].label = 2;
		data[1].value = 125;

		return data;
	}

	void update(wchar_t commandId) override
	{
		// execute 
		std::wcout << L" GA1 executing command Id " << commandId << std::endl;

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








