#pragma once

#include "..\Shared\StreamProcess.h"
#include "DAB.h"
#include <thread>

// Trace and Check Logic
class DataAnnotationBlock : public StreamProcess<Pixel, 2> {

	std::unique_ptr<Annotator<uint16_t, 2>> annotator;
    std::thread finalLabelDispatchThread;
    std::thread recycledLabelDispatchThread;
	std::thread labelEndThread;
    std::atomic<bool> stopThreads{ false };
	std::atomic<bool> requestAuxSetup{ false };
	std::thread auxSetupThread;
	uint32_t width;


    // Aux Label Dispatch Threads
    void finalisedLabelDispatch(std::wstring auxName)
    {
        while (!stopThreads)
        {
            FinalLabel finalLabel;
            if (annotator->popFinalisedLabel(finalLabel))
            {
				std::vector<uint8_t> bytes(sizeof(FinalLabel));
				std::memcpy(bytes.data(), &finalLabel, sizeof(FinalLabel));
				StreamProcess<Pixel,2>::sendAux(auxName, bytes);
            }
            else continue;
        }
    }

    void recycledLabelDispatch(std::wstring auxName)
    {
        while (!stopThreads)
        {
            uint16_t recyclableLabel;
            if (annotator->popRecycledLabel(recyclableLabel))
            {
                std::vector<uint8_t> bytes(sizeof(uint16_t));
                std::memcpy(bytes.data(), &recyclableLabel, sizeof(uint16_t));
                StreamProcess<Pixel, 2>::sendAux(auxName, bytes);
            }
			else continue;
        }
    }


public:
	DataAnnotationBlock(InterfaceParam_s* clientParams, char* cmdLine)
		: StreamProcess(clientParams, cmdLine)
	{
        std::map<std::string, std::string> args = processCLI.getArgs();


        std::wcout << L"Data Labelling Block Process Args: " << std::endl;

        if(args.find("M") != args.end())
			width = std::stoul(args["M"]);

		std::wcout << L"  M: " << width << std::endl;
        
        annotator = std::make_unique<Annotator<uint16_t, 2>>(width);

		auxSetupThread = std::thread(&DataAnnotationBlock::auxSetupLoop, this);

	}

    ~DataAnnotationBlock()
    {
		stopThreads = true;
		if (finalLabelDispatchThread.joinable()) finalLabelDispatchThread.join();
		if (recycledLabelDispatchThread.joinable()) recycledLabelDispatchThread.join();
    }

    void auxSetupLoop()
    {
        while (!stopThreads)
        {
            if (requestAuxSetup.exchange(false))
            {
                enableAuxChannels();
            }
        }
    }

    void enableAuxChannels(/*params as args*/)
    {
        StreamProcess<Pixel,2>::enableAux(L"AuxFinalLabels", true, interfaceDirection::DOWNSTREAM);
        StreamProcess<Pixel,2>::enableAux(L"AuxRecycledLabels", true, interfaceDirection::DOWNSTREAM);

        //start Aux dispatch threads
        finalLabelDispatchThread = std::thread(&DataAnnotationBlock::finalisedLabelDispatch, this, L"AuxFinalLabels");
        recycledLabelDispatchThread = std::thread(&DataAnnotationBlock::recycledLabelDispatch, this, L"AuxRecycledLabels");
	}



	std::array<Pixel,2> core(std::array<Pixel,2>& data) override
	{
		
		for (uint8_t i = 0; i < data.size(); i++) // for larger sizes loop unrolling can be done
		{
			annotator->push(static_cast<uint16_t>(data[i].label.labelValue));
			
            annotator->algorithm(static_cast<uint16_t>(data[i].label.labelValue), static_cast<uint16_t>(data[i].row), static_cast<uint16_t>(data[i].col));
		}

		return data;
	}


	void update(wchar_t commandId) override
	{
		// execute 
		std::wcout << L" DAB executing command Id " << commandId << std::endl;
		/* start stop is implemented in stream process*/
		switch (commandId)
		{
			case 'Q': StreamProcess::update(commandId); break;
			case 'X': StreamProcess::update(commandId); break;
			case 'U': StreamProcess::update(commandId); break;
			case 'D': 
                StreamProcess::update(commandId); 
				requestAuxSetup = true;
                break;
			case 'M': StreamProcess::update(commandId); break;
			case 'P': StreamProcess::update(commandId); break;
			/* Future : DAB specific commands */
			default: StreamProcess::stop(); break;
		}
	}
};


