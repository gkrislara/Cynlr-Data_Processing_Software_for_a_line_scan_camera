#pragma once
#ifndef FILTER_COEFFICIENTS_H
#define FILTER_COEFFICIENTS_H

/**
* @file filterCoefficients.h
* @brief Header file for filter coefficients.
* This file contains the definition of the FilterCoefficients class and its derived classes for specific filter coefficients.
*/


#include <stdint.h>
#include <vector>
#include <numeric>

/**
* @brief Abstract base class for filter coefficients.
*/
template<typename T>
class FilterCoefficients {
public:
	virtual ~FilterCoefficients() = default;
	virtual size_t getSize()           const noexcept = 0;
	virtual const float* getCoefficients()   const noexcept = 0;
};


/**
* @brief Generic filter coefficients class for static arrays.
*/
template<typename T>
class DynamicFilterCoefficients : public FilterCoefficients<T> {
public:
    // Construct from any container with T-elements
    template<typename T>
    explicit DynamicFilterCoefficients(const T& coeffs)
        : table_(coeffs.begin(), coeffs.end())
    {
        normalize();
    }

    size_t getSize() const noexcept override {
        return static_cast<size_t>(table_.size());
    }

    const T* getCoefficients() const noexcept override {
        return table_.data();
    }

private:
	// Scale taps so their sum == 1; solves numerical issues with small coefficients; Corner Case1
    void normalize() {
        T sum = std::accumulate(table_.begin(), table_.end(), T{ 0 });
        if (sum != T{ 0 }) {
            for (auto& v : table_) {
                v /= sum;
            }
        }
    }

    std::vector<T> table_;
};


#endif// filterCoefficients.h