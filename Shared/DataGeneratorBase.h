#pragma once
#include "Data.h"



class DataGeneratorBase {
public:
	virtual uint16_t gen() = 0;
	virtual ~DataGeneratorBase() = default;

};