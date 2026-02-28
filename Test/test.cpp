#include "pch.h"
#include <fstream>
#include <chrono>
#include "..\Shared\CSVDataGenerator.h"
#include "..\Shared\RandomNumberGenerator.h"


TEST(CSV_DATAGEN, DATAGEN)
{
	try {
		/* virtual calls are sporadic in nature due unpredicatable vtable lookups*/
		CSVDataGenerator CSVGen("E:\\LS_PROD_BASELINE\\Test\\random10.csv");

		uint16_t data;
		bool res;

		auto start = std::chrono::high_resolution_clock::now();

		data = CSVGen.gen();

		auto end = std::chrono::high_resolution_clock::now();

		auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
		std::cout << "Data: " << data << " Duration: " << duration_ns << " ns" << std::endl;
		EXPECT_LE(duration_ns, 500) << "with " << duration_ns - 500 << " difference";
	}
	catch (const std::exception& ex) {
		std::cerr << "Exception: " << ex.what() << std::endl;
		FAIL();
	}

}

TEST(CSV_DATAGEN_INLINE, DATAGEN)
{
	/* for understanding the performance hit of vtables in the virtual call - nominal performance all the time */
	/* atomisation makes the logic loose all its properties for stream / batch processing but its okay given the statelessness of pipes */
	/* the core idea here is that any access to the row object happens in O(1) due to even access from a memory mapped/cache coherent fetch*/

	std::unique_ptr<csv::CSVReader> csvReader;
	csv::CSVReader::iterator it;
	csv::CSVReader::iterator itEnd;
	uint32_t clm;
	uint32_t rw;
	csv::CSVRow row;

	csv::CSVFormat format;
	format.no_header();
	csvReader = std::make_unique<csv::CSVReader>("E:\\LS_PROD_BASELINE\\Test\\random10.csv", format);
	rw = 0;
	clm = 11;
	/* all the deferences are done statically*/
	it = csvReader->begin();
	itEnd = csvReader->end();

	row = *it; // first row

	uint16_t data;
	bool res;

	auto start = std::chrono::high_resolution_clock::now();

	uint16_t ret = 0;
	if (it != itEnd)
	{
		if (clm < row.size() - 1)
		{
			data = row[clm].get<uint16_t>();
			clm++;
		}
		else if (clm == row.size() - 1)
		{
			data = row[clm].get<uint16_t>();
			clm = 0;
			rw++;
			it++;
			row = *it;// next row;
		}
	}
	else
		data= 0;

	auto end = std::chrono::high_resolution_clock::now();

	auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
	std::cout << "Data" << data << "Duration: " << duration_ns << " ns" << std::endl;
	EXPECT_LE(duration_ns, 500) << "with " << duration_ns - 500 << " difference";
}


TEST(RNG_DATAGEN, DATAGEN)
{
	RngParam_s param;
	uint64_t sd[2] = { 123, 456 };
	param.seed = sd;
	param.size = 2;
	RandomNumberGenerator RNGGen(&param);

	uint16_t data;
	bool res;

	auto start = std::chrono::high_resolution_clock::now();

	data = RNGGen.gen();

	auto end = std::chrono::high_resolution_clock::now();

	auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

	std::cout << "Data" << data << "Duration: " << duration_ns << " ns" << std::endl;
	EXPECT_LE(duration_ns, 500) << "with " << duration_ns - 500 << " difference";
}


TEST(RNG_INTRINSIC, DATAGEN) 
{
	xorshiftW xorshft;

	xorshft.setup(123, 456);

	auto start = std::chrono::high_resolution_clock::now();

	uint64_t result = xorshft.next();

	auto end = std::chrono::high_resolution_clock::now();

	auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

	std::cout << "random value:" << result << std::endl;
	std::cout << "Duration: " << duration_ns << " ns" << std::endl;
	EXPECT_LE(duration_ns, 500) << "with " << duration_ns - 500 << " difference";
}


TEST(RNG_STREAM_SRC, DATAGEN)
{

}


TEST(CSV_STREAM_SRC, DATAGEN)
{

}


TEST(STREAM_SRC_SWITCH, DATAGEN)
{

}

TEST(STREAM_SINK)
{

}

TEST(MULTI_APP_SPAWN_ROBUSTNESS, PLATFORM)
{

}


/* spawn apps before at the sink side*/
TEST(STREAM_SRC_IO_SETUP, DATAGEN)
{

}


TEST(STREAM_SRC_SINK_DATA_PASS, DATAGEN)
{

}


TEST(STREAM_SRC_SINK_DATA_SWITCH_DATA_PASS, DATAGEN)
{
	 
}

/* Actuate through User Input for user inputs */