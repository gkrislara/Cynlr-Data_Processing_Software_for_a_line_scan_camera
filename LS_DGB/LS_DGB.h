#pragma once

#include "..\Shared\StreamProcess.h"
#include "..\Shared\DataGeneratorFactory.h"

typedef enum {
	RNG = 1,
	CSV,
	NONE,
	/* Add sources if needed */
	NUM_SRC_TYPE
}SourceType_e;


/* TODO: Move this to separate Unified Error Code header*/
typedef enum class streamSourceError {
	SUCCESS,
	ERR_STREAM_SOURCE_OBJECT_INIT_FAIL,
	ERR_STREAM_SOURCE_DATA_GENERATOR_OBJECT_INIT_FAIL,
	ERR_STREAM_SOURCE_RNG_OBJECT_INIT_FAIL,
	ERR_STREAM_SOURCE_CSV_OBJECT_INIT_FAIL,
	ERR_STREAM_SOURCE_INTF_PARAM_INIT_FAIL,
	ERR_STREAM_SOURCE_INTF_NAMED_PIPE_OBJECT_INIT_FAIL,
	ERR_STREAM_SOURCE_GENERATOR_IS_LIVE,
	ERR_STREAM_SOURCE_STREAMWIDTH_GT_UINT64,
	/* Add error codes here*/
	NUM_ERR
}streamSourceErrorCode;



typedef struct {
	SourceType_e sourceType;
	RngParam_s rngParam;
	CsvParam_s csvParam;
	/*add source parameters if needed*/
	uint32_t max_cols;
}SourceParam_s;



// Test
class DataGeneratorBlock : public StreamProcess<Pixel, 2> {

private:

	std::unique_ptr<DataGeneratorBase> dataGen;
	SourceParam_s RNGSource;
	SourceParam_s CSVSource;
	SourceType_e currentSourceType;
	uint32_t col;
	uint32_t row;
	uint32_t max_cols;

	streamSourceErrorCode currentError;

	void nextcoordinate(uint32_t* clm, uint32_t* rw)
	{
		if (col++ >= max_cols)
		{
			col = 0;
			row++;
		}
		*clm = col;
		*rw = row;
	}

	void switchSourceTo(SourceParam_s* source)
	{
		currentError = streamSourceError::SUCCESS;
		/* check if gen step is false */

		if (dataGen)
		{
			/* switch to other source*/
			if (source->sourceType == SourceType_e::RNG)
			{
				if (!(dataGen = std::move(DataGeneratorFactory::createRandomNumberGenerator(&source->rngParam))))
				{
					currentError = streamSourceError::ERR_STREAM_SOURCE_RNG_OBJECT_INIT_FAIL;
				}

				currentSourceType = SourceType_e::RNG; 


				/* return error code if not*/
			}
			else if (source->sourceType == SourceType_e::CSV)
			{
				if (!(dataGen = std::move(DataGeneratorFactory::createCSVGenerator(source->csvParam.filename))))
				{
					currentError = streamSourceError::ERR_STREAM_SOURCE_RNG_OBJECT_INIT_FAIL;
				}

				currentSourceType = SourceType_e::CSV;
			}

			/* add other sources here*/
		}
		else
			currentError = streamSourceError::ERR_STREAM_SOURCE_DATA_GENERATOR_OBJECT_INIT_FAIL;
	}

	void toggleSource()
	{
		if (currentSourceType == SourceType_e::RNG)
		{
			switchSourceTo(&CSVSource);
		}
		else if (currentSourceType == SourceType_e::CSV)
		{
			switchSourceTo(&RNGSource);
		}
	}


public:

	DataGeneratorBlock(InterfaceParam_s* clientParams, char* cmdLine)
		: StreamProcess(clientParams, cmdLine)
	{
		std::map<std::string, std::string> args = processCLI.getArgs();


		if (args.find("M") != args.end())
			max_cols = std::stoul(args["M"]);

		if (args.find("CSV_FILE") != args.end())
			CSVSource.csvParam.filename = args["CSV_FILE"];


		std::wcout << L"Data Generator Block Process Args: " << std::endl;

		std::wcout << L" M:" << max_cols << std::endl;
		std::wcout << L" CSV_FILE:" << CSVSource.csvParam.filename.c_str() << std::endl;

		// specific initialisations
		col = 0;
		row = 0;

		// RNG
		RNGSource.sourceType = SourceType_e::RNG;
		std::random_device rd;
		std::mt19937_64 eng(rd());
		uint64_t seed0 = eng();
		uint64_t seed1 = eng();
		if (seed0 == 0 && seed1 == 0) { seed1 = 0xdeadbeefULL; }
		uint64_t RNGSEED[2] = { seed0, seed1 };
		RNGSource.rngParam.seed = RNGSEED;
		RNGSource.rngParam.size = 2;
		RNGSource.max_cols = max_cols;

		//CSV
		CSVSource.sourceType = SourceType_e::CSV;
		CSVSource.max_cols = max_cols;

		// Default Source
		currentSourceType = SourceType_e::RNG;
		dataGen = DataGeneratorFactory::createRandomNumberGenerator(&RNGSource.rngParam);
		currentError = streamSourceError::SUCCESS;
	}

	std::array<Pixel,2> core(std::array<Pixel,2>& data) override
	{
		data = { 0 };
		std::array<Pixel, 2> sourceData;

		for (uint8_t i = 0; i < sourceData.size() ; i++)
		{
			
			sourceData[i].value = static_cast<uint8_t>(dataGen->gen());
			nextcoordinate(&sourceData[i].col, &sourceData[i].row);
			sourceData[i].label.labelValue = 0;
		}

		return sourceData;
	}

	void update(wchar_t commandId) override
	{
		// execute 
		std::wcout << L" DGB executing command Id " << commandId << std::endl;
		// process the commandId and perform actions accordingly

		//Logic for Start / Stop

		switch (commandId)
		{
			/* Standard Commands */
			case 'Q': StreamProcess::update(commandId); break;
			case 'X': StreamProcess::update(commandId); break;
			case 'U': StreamProcess::update(commandId); break;
			case 'D': StreamProcess::update(commandId); break;
			case 'M': StreamProcess::update(commandId); break;
			case 'P': StreamProcess::update(commandId); break;
			/* Data Generator Specific Commands */ 
			case 'R': switchSourceTo(&RNGSource); /*Print Current Source == RNG */ 
				std::wcout << L" Switched Source to RNG" << std::endl;
					break;
			case 'C': switchSourceTo(&CSVSource); /*Print Current Source == CSV */ 
				std::wcout << L" Switched Source to CSV" << std::endl;
					break;
			case 'T': toggleSource(); break;
		default: StreamProcess::stop(); break;
		}
		
	}
};





