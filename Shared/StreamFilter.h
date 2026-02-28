#pragma once
#include "platform.h"
#include "IOServer.h"
#include "IOClient.h"
#include "Data.h"

enum buffer_type {
	FIFO = 0, // First In First Out
	LIFO = 1, // Last In First Out
	QUEUE = 2, // Queue
	CIRCULAR_QUEUE = 3, // Circular Queue
	STACK = 4, // Stack
	BUFFER_TYPE_MAX = 5 // Maximum buffer type count
};

enum data_source_type {
	PIPE = 0, // Data source is a pipe
	EXTERNAL_GENERATOR = 1, // Data source is an external source
	BUFFER = 2, // Data source is a buffer
	DATA_SOURCE_TYPE_MAX = 3 // Maximum data source type count
};


/* Inherited bu  DFB , DLB and TCB*/
/* Extended by
			   DFB in processData
			   DLB in processData
			   TCB in processData
*/

/* invokes platform ultilites like windows optimised circular buffer */

// Input interface - pipe or external source file / network / hardware / RNG

// output interface - pipe or external source - file / network / hardware


/* Prepare the IOInterface to start receing the data. All the flowmechanics happens in IOBridge
*/

/*Filter Template Methods*/
template<typename BT, size_t dataWidth>
class StreamFilter  
{
	IOServer<Pixel, dataWidth> dst; 
	IOClient<Pixel, dataWidth> src;
	CircularBufferFilterW<BT> filter;

public:
	StreamFilter() {
		/* Alt: */
		/* in parent code invoke as */
		/* auto derivedFilter = std::make_unique<DerivedFilter<Pixel, 2>> */
		/* StreamFilter<Pixel, 2> filter(std::move(derivedFilter)); */
	}

	~StreamFilter() {
		~IOClient<Pixel,dataWidth>();
		~IOServer<Pixel,dataWidth>();
		~CircularBufferFilterW<BT>();
	}

	bool setupIO(InterfaceParam_s* srcParam,InterfaceParam_s* dstParam)
	{
		/* setups src*/
		src.setup(srcParam);

		dst.setup(dstParam);

		/* setup dst*/

	}

	bool processStep() {
		// Implementation for processing a step in the pipe
		// fetch() -> processData() -> dispatch()
		// 
		// The code flow flows something like this:
		// bool result = false;
		//
		//if (src.receiveData(inData))
		//{

		//	filter->push(inData);
		//	
		//  

		//	dst.sendData(out);
		//}

		//return result;
	}

	bool passThrough() {
		// Implementation for passing data through the pipe without processing
		// This will be used when the data needs to be sent as is without any processing
		bool result = false;
		if (src.get(data))
		{
			result = dispatch(data);
		}
		return result;
	}

};