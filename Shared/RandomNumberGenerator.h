#pragma once
#include "platform.h"
#include "DataGeneratorBase.h"


typedef struct {
	uint64_t* seed;
	uint8_t size;
	/* Add or change parameters if needed */
}RngParam_s;


class RandomNumberGenerator :public DataGeneratorBase{
	xorshiftW xorshift;
public:
	RandomNumberGenerator(RngParam_s* rngParam) {
		
		if (rngParam->size >= 2)
		{
			uint64_t seed1 = *(rngParam->seed++);
			uint64_t seed2 = *(rngParam->seed);
			xorshift.setup(seed1, seed2);
		}
		else
			return;
	}

	uint16_t gen() override {
		
			uint64_t rn = xorshift.next();
			return static_cast<uint16_t>(rn);
	}

};
