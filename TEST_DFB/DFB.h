#pragma once
#include "..\Shared\Platform.h"
#include "filterCoefficients.h"
#include "PSCoefficients.h"
 
template <typename T>
class PreProcessor : public CircularBufferFilterW<T> {

	const FilterCoefficients<T>* coefficients;

public:
	PreProcessor(const FilterCoefficients<T>* coeffs) : CircularBufferFilterW<T>(coeffs->getSize(),0), coefficients(coeffs) {}

	T algorithm() {

		size_t bufferSize = coefficients->getSize();
		/* check whether the buffer is filled with data*/
		if (CircularBufferFilterW<T>::level() < bufferSize) return 0;

		T result = static_cast<T>(0);
		const T* coeffs = coefficients->getCoefficients();
		
		size_t idx = CircularBufferFilterW<T>::getHead();

		for (uint16_t i = 0; i < bufferSize; ++i) {
			result += CircularBufferFilterW<T>::getBufferAt(idx) * coeffs[i];
			idx = (idx + 1) % bufferSize;
		}
		return result;
	}
};

/* 
*  old algorithm for reference
* 	void add(float value) {
		buffer[head] = value;
		head = (head + 1) % bufferSize;
		if (filled < bufferSize) {
			filled++;
		}
	}

	float filter() {
		if (filled < bufferSize) return 0;
		float sum = 0;
		int idx = head;
		const T* coeffs = coefficients->getCoefficients();
		for (uint16_t i = 0; i < bufferSize; ++i) {
			sum += buffer[idx] * coeffs[i];
			idx = (idx + 1) % bufferSize;
		}
		return sum;
	}
*/