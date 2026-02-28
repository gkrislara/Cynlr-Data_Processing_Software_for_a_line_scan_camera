#pragma once
#include "IOClient.h"
#include "Data.h"
/* sink strategies */
template <size_t streamWidth>
class StreamSink {
	IOClient<Pixel,dataWidth> src;
public:
	bool setupIO(InterfaceParam_s* intfParams) 
	{
		src.setup(intfParams);
		src.start();
	}

	void receiveAndPrint(bool step)
	{
		Pixel pixel[streamWidth];

		while (step)
		{
			if (src.receiveData(pixel))
			{
				for (int i = 0; i < streamWidth; i++)
				{
					std::cout << pixel[i].value << " ";
				}
			}
		}
	}

	/* other stream sink specific functions*/
};