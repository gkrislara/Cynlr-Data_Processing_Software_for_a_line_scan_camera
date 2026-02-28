#pragma once
#define NOMINMAX
#include "..\Shared\StreamProcess.h"
#include "..\Shared\Platform.h"
#include "DFB.h"
#include <limits>

class DataFilteringBlock : public StreamProcess<Pixel, 2> {
private:
	std::unique_ptr<PreProcessor<float>> preProcessor;
	float threshold,th;
	uint32_t maxVal;
public:
	DataFilteringBlock(InterfaceParam_s* clientParams, char* cmdLine)
		: StreamProcess(clientParams, cmdLine)
	{

		std::map<std::string, std::string> args = processCLI.getArgs();

		std::wcout << L"Data Filter Block Process Args: " << std::endl;

		if(args.find("V") != args.end())
			th = std::stof(args["V"]);

		std::wcout << L" V:" << th << std::endl;

		// Filter Coefficients Init
		FilterCoefficients<float>* coeffs = new DynamicFilterCoefficients<float>(PS1_COEFFS_FLOAT);
		preProcessor = std::make_unique<PreProcessor<float>>(coeffs);

		// Threshold Init - to be got from cmdline args later
		maxVal = std::numeric_limits<uint8_t>::max();
		threshold = th * maxVal; 
		std::wcout <<L" Threshold: " << threshold << std::endl;
		/* * Max value of uint8_t*/
		
	}

	std::array<Pixel, 2> core(std::array<Pixel, 2>& data) override
	{
		for (uint8_t i = 0; i < data.size(); i++) // for larger sizes loop unrolling can be done
		{
			preProcessor->push(static_cast<float>(data[i].value));
			float filteredValue = preProcessor->algorithm();
			// Apply threshold
			data[i].value = 1 ? (filteredValue >= threshold) : 0;
		}
		return data;
	}

	void update(wchar_t commandId) override
	{
		// execute 
		std::wcout << L" DFB executing command Id " << commandId << std::endl;

		/* start stop is implemented in stream process*/

		switch (commandId)
		{
			case 'Q': StreamProcess::update(commandId); break;
			case 'X': StreamProcess::update(commandId); break;
			case 'U': StreamProcess::update(commandId); break;
			case 'D': StreamProcess::update(commandId); break;
			case 'M': StreamProcess::update(commandId); break;
			case 'P': StreamProcess::update(commandId); break;
			/* DFB specific commands */
			case '1': // Adjust Filter Coefficients - to be implemented
				break;
			case '2': // Adjust Threshold - to be implemented
				break;
			default: StreamProcess::stop(); break;
		}
	}


};