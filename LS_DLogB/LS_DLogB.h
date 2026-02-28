#pragma once

#include "..\Shared\StreamProcess.h"
#include <iostream>
#include "..\LS_DAB\DAB.h" // For FinalLabel

class DataLoggingBlock : public StreamProcess<Pixel, 2> {
    std::thread finalLabelAuxHandlerThread;
    std::mutex finalLabelMtx;
    std::atomic<bool> stopThread{ false };
	std::atomic<bool> requestAuxSetup{ false };
    std::thread auxSetupThread;

    void handleFinalLabelAux(std::wstring auxName)
    {
        HANDLE event = StreamProcess<Pixel, 2>::getAuxEvent(auxName);
        while (!stopThread)
        {
            DWORD waitResult = WaitForSingleObject(event, 100);
            if (waitResult == WAIT_OBJECT_0) {
                std::vector<uint8_t> data;
                {
                    if (!StreamProcess<Pixel, 2>::popAuxData(auxName, data)) continue;
                }
                if (data.size() == sizeof(FinalLabel)) {
                    FinalLabel label;
                    std::memcpy(&label, data.data(), sizeof(FinalLabel));
                    std::lock_guard<std::mutex> lk(finalLabelMtx);
                    // Log or process the final label as needed
                    std::wcout << L"Received FinalLabel: " << L" Label: " << label.label << L" Size: " << label.size
                        << L" TopLeft: (" << label.topLeft.x << L"," << label.topLeft.y << L")"
                        << L" BottomRight: (" << label.bottomRight.x << L"," << label.bottomRight.y << L")" << std::endl;
                }
            }
        }
	}


public:

	DataLoggingBlock(InterfaceParam_s* clientParams, char* cmdLine)
		: StreamProcess(clientParams, cmdLine)
	{
		auxSetupThread = std::thread(&DataLoggingBlock::auxSetupLoop, this);
        
	}

    ~DataLoggingBlock() {
        stopThread = true;
        if (finalLabelAuxHandlerThread.joinable()) finalLabelAuxHandlerThread.join();
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

    void enableAuxChannels()
    {
        StreamProcess<Pixel, 2>::enableAux(L"AuxFinalLabels", true, interfaceDirection::UPSTREAM); // get name from cmdLine

        finalLabelAuxHandlerThread = std::thread(&DataLoggingBlock::handleFinalLabelAux, this, L"AuxFinalLabels");
	}

	std::array<Pixel, 2> core(std::array <Pixel, 2>& data) override
	{

		std::wcout << "Pixel[0]: " << " col: " << data[0].col << " row: " << data[0].row << " value: " << data[0].value << " label: " << data[0].label << std::endl;

		std::wcout << "Pixel[1]: " << " col: " << data[1].col << " row: " << data[1].row << " value: " << data[1].value << " label: " << data[1].label << std::endl;

		//Clear the internal buffer
		std::wcout.flush();

		return data;
	}

	void update(wchar_t commandId) override
	{
		// execute 
		std::wcout << L" DLogB executing command Id " << commandId << std::endl;

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
			default: StreamProcess::stop(); break;
		}
	}
};