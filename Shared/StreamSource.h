#pragma once
#include "DataGeneratorFactory.h"
#include "IOServer.h"

typedef enum {
	RNG = 1,
	CSV,
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


/* source parameters are in their respective generator headers*/

typedef struct {
	SourceType_e source;
	RngParam_s rngParam;
	CsvParam_s csvParam;
	/*add source parameters if needed*/
	uint32_t max_cols;
}SourceParam_s;

/* interface parameters are in their respective interface headers */

/* Stream Source */
template <size_t streamWidth>
class StreamSource {
	IOServer<Pixel,streamWidth> dst;
	std::unique_ptr<DataGeneratorBase> dataGen;
	bool genStep;
	uint32_t col;
	uint32_t row;
	uint32_t max_cols;
	uint64_t data;

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

public:
	StreamSource(SourceParam_s* sourceParams, InterfaceParam_s* interfaceParams)
	{
		col = 0;
		row = 0;
		max_cols = sourceParams->max_cols;
		/* Init Source */
		if (sourceParams->source == RNG)
		{
			dataGen = std::move(DataGeneratorFactory::createRandomNumberGenerator(&sourceParams->rngParam));
		}
		else if (sourceParams->source == CSV)
		{
			dataGen = std::move(DataGeneratorFactory::createCSVGenerator(sourceParams->csvParam.filename));
		}
		/* Add other Sources here */

		/* interface setup*/
		this->setupIO(interfaceParams);
	}

	~StreamSource()
	{
		dst.disconnect();
	}

	void setupIO(InterfaceParam_s* intfParam) {

		currentError = streamSourceError::SUCCESS;

		if (intfParam) {
			if (intfParam->interfaceType == IOInterfaceType::NAMED_PIPE_SERVER)
			{	/* create named pipe will take care of all the intitialisations and generates the handle based on the underlying platform*/
				/* ideal scenario */
				/*if (!(dst = std::move(InterfaceGeneratorFactory::createNamedPipe(intfParam->namedPipeParam)))
				{
					return ERR_STREAM_SOURCE_INTF_NAMED_PIPE_OBJECT_INIT_FAIL;
				}*/
				/* but we have hardcoded the server for now*/
				dst.setup(intfParam);
				dst.start();
			}
			/* other interfaces*/
		}
		else
			 currentError = streamSourceError::ERR_STREAM_SOURCE_INTF_PARAM_INIT_FAIL;
	}

	/* other stream source specific functions */
	void switchSourceTo(SourceParam_s* sourceParams)
	{
		currentError = streamSourceError::SUCCESS;
		/* check if gen step is false */
		if (genStep) currentError = streamSourceError::ERR_STREAM_SOURCE_GENERATOR_IS_LIVE;

		if (dataGen)
		{ 
			/* switch to other source*/
			if (sourceParams.source == RNG)
			{
				if (!(dataGen = std::move(DataGeneratorFactory::createRandomNumberGenerator(sourceParams.rngParam)))) 
				{
					currentError = streamSourceError::ERR_STREAM_SOURCE_RNG_OBJECT_INIT_FAIL;
				}

				/* return error code if not*/
			}
			else if (sourceParams.source == CSV)
			{
				if (!(dataGen = std::move(DataGeneratorFactory::createCSVGenerator(sourceParams.csvParam))))
				{
					currentError = streamSourceError::ERR_STREAM_SOURCE_RNG_OBJECT_INIT_FAIL;
				}
			}

			/* add other sources here*/
		}
		else
			currentError = streamSourceError::ERR_STREAM_SOURCE_DATA_GENERATOR_OBJECT_INIT_FAIL;
	}

	void generateAndDispatch(bool step) {
		
		currentError = streamSourceError::SUCCESS;
		
		if (dataGen && dst)
		{
			if (streamWidth > sizeof(uint64_t))
				currentError = streamSourceError::ERR_STREAM_SOURCE_STREAMWIDTH_GT_UINT64;
				return;

			Pixel pixel[streamWidth];
			genStep = step;

			while (genStep)
			{
				
				/* pack pixel struct */
				for (uint8_t i = 0; i < streamWidth ; i++)
				{
					dataGen->gen(&pixel[i].value);
					nextcoordinate(&pixel[i].col, &pixel[i].row);
					pixel[i].label = 0;
				}

				/* dispatch*/
				dst.sendData(pixel);
			}
		}
		else
			currentError = streamSourceError::ERR_STREAM_SOURCE_OBJECT_INIT_FAIL;
	}
};


/*

just in case 

#pragma once
#include "IOInterface.h"



* Stream Factory Pattern
* Factory Pattern that encompases source, filter and sink


typedef enum {
	SOURCE,
	FILTER,
	SINK
}StreamEntityType;


class StreamBase {
public:
	virtual bool setupIO(InterfaceParam_s* intfParams) noexcept = 0;
	virtual ~StreamBase() = default;
};

*/