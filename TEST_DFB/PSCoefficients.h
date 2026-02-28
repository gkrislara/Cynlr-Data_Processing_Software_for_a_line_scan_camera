#pragma once
/**
* @file PS1Coefficients.h
* @brief Contains the PS1 9-tap filter coefficients
*/

#include <array>



// PS1 9‐tap filter coefficients (float)
constexpr std::array<float, 9> PS1_COEFFS_FLOAT = {
    0.00025177f, 0.008666992f, 0.078025818f,
    0.24130249f,  0.343757629f, 0.24130249f,
    0.078025818f, 0.008666992f, 0.00025177f
};

// for double precision, you can add:
constexpr std::array<double, 9> PS1_COEFFS_DOUBLE = {
    0.00025177, 0.008666992, 0.078025818,
    0.24130249,  0.343757629, 0.24130249,
    0.078025818, 0.008666992, 0.00025177
};

/* Add other Coefficients if needed */
