#pragma once
#include <string>
#include "DataGeneratorBase.h"
#include "csv.hpp"

typedef struct {
	std::string filename;
}CsvParam_s;


class CSVDataGenerator :public DataGeneratorBase {
	std::string filename;
	std::unique_ptr<csv::CSVReader> csvReader;
	csv::CSVReader::iterator it;
	csv::CSVReader::iterator itEnd;

	uint32_t clm;
	uint32_t rw,maxRows;
	std::vector<std::vector<uint16_t>> cachedRow; // double/ping-pong buffer for caching rows
	uint8_t cacheRow_, readRow_;

	/* threading variables*/
	std::thread prefetchThread;
	std::mutex mtx;
	std::condition_variable cv;
	bool prefetchNeeded;
	bool prefetchDone;
	bool stopThread;
	bool lastRowCached_;
	
	inline void toggleRow(uint8_t& row_) 
	{
		if (row_ == 0)
			row_ = 1;
		else
			row_ = 0;
	}

	// this is the function that bloats the latency 
	void cacheRow() 
	{
		cachedRow[cacheRow_].clear();
		for (auto& field : *it)
		{
			cachedRow[cacheRow_].push_back(field.get<uint16_t>());
		}
		++it; maxRows++;
		if (it == itEnd) lastRowCached_ = true;
	}

	// prefetching thread function to avoid the latency bloat caused by cacheRow()
	void prefetchLoop() 
	{
		while (!stopThread) 
		{
			std::unique_lock<std::mutex> lock(mtx);
			cv.wait(lock, [&] { return prefetchNeeded || stopThread; });
			if (stopThread) break;
			if (it != itEnd) 
			{
				cacheRow();
				prefetchDone = true;
				prefetchNeeded = false;
				cv.notify_one();
			}
		}
	}

public:
	CSVDataGenerator(std::string filename) : filename(filename) 
	{

		csv::CSVFormat format;
		format.no_header();
		csvReader = std::make_unique<csv::CSVReader>(filename,format);
		rw = cacheRow_ = readRow_ = maxRows = 0;
		clm = 0;

		prefetchNeeded = false;
		prefetchDone = false;
		stopThread = false;
		lastRowCached_ = false;

		/* all the deferences are done statically*/

		it = csvReader->begin(); // refernce to start of memory map
		itEnd = csvReader->end(); // reference to end of memory map
		
		/* cache first two rows in the ping-pong buffer*/
		cachedRow.resize(2);
		if (it != itEnd) cacheRow(); // first row cached in cachedRow[0]
		toggleRow(cacheRow_); // move to next row at this stage cacheRow_ is 1 and readRow_ is 0
		if (it != itEnd) cacheRow(); // move to next row and cache in cachedRow[1]
		assert(cacheRow_ != readRow_); // sanity check

		prefetchThread = std::thread(&CSVDataGenerator::prefetchLoop, this);
	
	}

	~CSVDataGenerator() 
	{
		{
			std::lock_guard<std::mutex> lock(mtx);
			stopThread = true;
			cv.notify_one();
		}
		if (prefetchThread.joinable())
			prefetchThread.join();

		/* cleanup*/
		it = itEnd;
		csvReader.reset();	
	}

	uint16_t gen() override 
	{
		if(rw >= maxRows) return 0; // end of data
		
		uint16_t ret = cachedRow[readRow_][clm]; //main fetch
		
		++clm;

		/* when trying to fetch last element of a row, 
		   prefetch n+1 the row onto the current readrow. 
		   This makes the gen operate at same speed at the last index
		*/
		if (clm == cachedRow[readRow_].size() - 1)
		{
			uint16_t ret = cachedRow[readRow_][clm];
			clm = 0;
			++rw;

			if(!lastRowCached_)
			{
				std::unique_lock<std::mutex> lock(mtx);
				toggleRow(readRow_);
				toggleRow(cacheRow_);
				prefetchNeeded = true;
				cv.notify_one();
				cv.wait(lock, [&] { return prefetchDone; });
				prefetchDone = false;
			}
			else // lastrow already cached so no prefetching needed
			{
				toggleRow(readRow_);
			}
		}
		return ret;
	}
};


/* Other approaches tried for  memory efficiency

//uint16_t gen()
	//{
	//	/* generate data for step */
	//	uint16_t ret = 0;
	//	if (it != itEnd)
	//	{
	//		if (clm < clmSize-1)
	//		{
	//			 ret = row[clm].get<uint16_t>();
	//			 clm++;
	//		}
	//		else if (clm == clmSize-1)
	//		{
	//			ret = row[clm].get<uint16_t>();
	//			clm = 0;
	//			rw++;
	//			it++; // memory map increment
	//			row = *it;// next row;
	//		}
	//		return ret;
	//	}
	//	else
	//		return 0;
	//}

	//uint16_t gen()
	//{
	//	// generate data for step
	//	if (it == itEnd)
	//		return 0;

	//	// Cache clmSize to avoid repeated member access
	//	const size_t localClmSize = clmSize;

	//	// Use pointer access for row to avoid operator[] overhead
	//	const csv::CSVRow& localRow = row;

	//	uint16_t ret = 0;
	//	if (clm < localClmSize - 1)
	//	{
	//		// Avoid repeated bounds checking by using at_unchecked if available
	//		ret = localRow[clm].get<uint16_t>();
	//		++clm;
	//	}
	//	else // clm == localClmSize - 1
	//	{
	//		ret = localRow[clm].get<uint16_t>();
	//		clm = 0;
	//		++rw;
	//		++it;
	//		if (it != itEnd) {
	//			row = *it; // Only dereference if not at end
	//		}
	//	}
	//	return ret;
	//}

