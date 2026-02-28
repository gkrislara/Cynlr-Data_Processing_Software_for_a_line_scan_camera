#pragma once

#include "..\Shared\StreamProcess.h"
#include "DLB.h"
#include <thread>

class DataLabellingBlock : public StreamProcess<Pixel, 2> {

	std::unique_ptr<ConnectedComponentLabeller<2>> ccLabeller;
	std::thread labelRecyclingThread;
	std::mutex recycledLabelMtx;
	std::atomic<bool> stopThread{ false };
	uint32_t width;

	std::atomic<bool> requestAuxSetup{ false };
	std::thread auxSetupThread;

	void handleLabelRecycling(std::wstring auxName)
	{
		// Future: implement label recycling logic
		while (!stopThread)
		{
			// Placeholder for recycling logic
			HANDLE event = StreamProcess<Pixel, 2>::getAuxEvent(auxName);
			while (!stopThread)
			{
				DWORD waitResult = WaitForSingleObject(event, 100);
				if (waitResult == WAIT_OBJECT_0) {
					std::vector<uint8_t> data;
					{
						if (!StreamProcess<Pixel, 2>::popAuxData(auxName, data)) continue;
					}
					if (data.size() == sizeof(uint16_t)) {
						uint16_t label;
						std::memcpy(&label, data.data(), sizeof(uint16_t));
						std::lock_guard<std::mutex> lk(recycledLabelMtx);
						ccLabeller->recycleLabel(label); // pushes the label to ccLabeller stack

						std::wcout << L"Recycled Label received: " << label << std::endl;
					}
				}
			}
		}
	}

public:
	DataLabellingBlock(InterfaceParam_s* clientParams, char* cmdLine)
		: StreamProcess(clientParams, cmdLine)
	{

		std::map<std::string, std::string> args = processCLI.getArgs();

		std::wcout << L"Data Labelling Block Process Args: " << std::endl;
		
		if (args.find("M") != args.end())
			width = std::stoul(args["M"]);

		std::wcout << L"  M: " << width << std::endl;

		ccLabeller = std::make_unique<ConnectedComponentLabeller<2>>(width);


		auxSetupThread = std::thread(&DataLabellingBlock::auxSetupLoop, this);
	}
	~DataLabellingBlock() {
		stopThread = true;
		if (labelRecyclingThread.joinable()) labelRecyclingThread.join();
	}

	void enableAuxChannels()
	{
		StreamProcess<Pixel, 2>::enableAux(L"AuxRecycledLabels", true, interfaceDirection::UPSTREAM); // get name from cmdLine

		// Start label recycling thread
		labelRecyclingThread = std::thread(&DataLabellingBlock::handleLabelRecycling, this, L"AuxRecycledLabels");

	}

	std::array<Pixel, 2> core(std::array <Pixel,2>& data) override
	{
		for (uint8_t i = 0; i < data.size(); i++) // for larger sizes loop unrolling can be done
		{
			ccLabeller->push(static_cast<uint16_t>(data[i].label));
			data[i].label = ccLabeller->algorithm(static_cast<uint16_t>(data[i].value), static_cast<uint16_t>(data[i].row), static_cast<uint16_t>(data[i].col));
		}

		return data;
	}

	void auxSetupLoop()
	{
		while (!stopThread)
		{
			if (requestAuxSetup.exchange(false))
			{
				enableAuxChannels();
			}
		}
	}

	void update(wchar_t commandId) override
	{
		// execute 
		std::wcout << L" DLB executing command Id " << commandId << std::endl;

		/* start stop is implemented in stream process*/

		switch (commandId)
		{
		case 'Q': StreamProcess::update(commandId); break;
		case 'X': StreamProcess::update(commandId); break;
		case 'U': 
			StreamProcess::update(commandId); 
			requestAuxSetup = true;
			break;
		case 'D': StreamProcess::update(commandId); break;
		case 'M': StreamProcess::update(commandId); break;
		case 'P': StreamProcess::update(commandId); break;
			/* Future : DLB specific commands */
		case '8': // switch to ccl8
			break;
		case '6': // switch to ccl6
			break;
		case '4': // switch to ccl4 
			break;
		default: StreamProcess::stop(); break;
		}
	}

};

