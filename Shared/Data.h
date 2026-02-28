#pragma once
#define DEFAULT_T_NS 500
#define MAX_COL 2048
#define DEFAULT_N_ELEMENTS 2

#include <cstdint>


/**
* @struct Pixel
 * @brief Represents a data value with an associated defect status.
 *
 * This structure holds a pixel value, its position in a 2D grid (row and column) and its label.
*/

typedef struct {
	uint8_t value;
	uint32_t row;
	uint32_t col;
	uint16_t label;
}Pixel;