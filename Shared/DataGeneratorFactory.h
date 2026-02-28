#pragma once
#include <memory>
#include "DataGeneratorBase.h"
#include "CSVDataGenerator.h"
#include "RandomNumberGenerator.h"


class DataGeneratorFactory {
public:
	static std::unique_ptr<DataGeneratorBase> createCSVGenerator(const std::string& filename) {
		return std::make_unique<CSVDataGenerator>(filename);
	}

	static std::unique_ptr<DataGeneratorBase> createRandomNumberGenerator(RngParam_s* rngParam) {
		return std::make_unique<RandomNumberGenerator>(rngParam);
	}
};