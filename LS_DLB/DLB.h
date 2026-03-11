#pragma once

#include "..\Shared\Platform.h"
#include "..\Shared\Data.h"

#ifdef min
#undef min
#endif

// Connected Component-8 Labelling with k-pixel processing


//Dev:Needs Review

template <size_t k> //numpixels
class ConnectedComponentLabeller : public CircularBufferFilterW<uint16_t> 
{
    uint16_t nextLabel;
    uint16_t width;
	std::vector<uint16_t> recycledLabels; // stack for recycled labels


    void mergeWithNeighbor(uint16_t pos, uint16_t& L, int32_t offset) 
    {
        size_t idx = (pos + offset + CircularBufferFilterW<uint16_t>::capacity()) % CircularBufferFilterW<uint16_t>::capacity();
        /* get current label value 0 / 1*/
        uint16_t l = CircularBufferFilterW<uint16_t>::getBufferAt(idx);

        if (l == 0) return;
        if (L == 0)     L = l;
        else if (l != L)
        {
            //uint16_t tmp = std::min(l, L);
            /* any other label - merge inplace with out having to have a seperate buffer */
            CircularBufferFilterW<uint16_t>::setBufferAt(idx, L);
        }

		//Perform union -find like path compression for future merges to be faster. This is to ensure that the equivalence classes are flattened and future lookups are O(1) on average.

    }

    uint16_t labelWithEquivalenceAndMerge(uint16_t pos, uint16_t val, uint16_t row, uint16_t col)
    {
        if (val == 0)
        {
            return 0;
        }
            
        uint16_t L = 0;

        if (col > 0) // Left neighbor
            mergeWithNeighbor(pos, L, -1);

		if (row > 0)
        // Above row neighbors
        {
            if (col > 0)
                mergeWithNeighbor(pos, L, -width - 1);// top-left

            mergeWithNeighbor(pos, L, -width);       // top

            if ((col + 1) < width)
                mergeWithNeighbor(pos, L, -width + 1);// top-right
        }

        /* if none found then assign a new label*/
        if (L == 0)
        {
			//check if a label is available in the recycled stack
			// if available use that else assign new label
			if (!recycledLabels.empty())
			{   // FIFO
                L = recycledLabels.back();
				recycledLabels.pop_back();
			}
            else
            {
                L = nextLabel++;
            }
        }


        CircularBufferFilterW<uint16_t>::setBufferAt(pos, L);
        return L;
    }

public:

	ConnectedComponentLabeller(uint32_t size) : CircularBufferFilterW(size + k + 1, 0) // equivalent to saying fill buffer with zeros until headPos
    {
        nextLabel = 1;
        width = size;
		// reserve space for recycled labels
		recycledLabels.reserve(1024); // initial capacity, can grow as needed
    }


	uint16_t algorithm(uint16_t vl, uint16_t rw, uint16_t cl)
    {
		uint16_t pos = (CircularBufferFilterW<uint16_t>::getHead() + CircularBufferFilterW<uint16_t>::capacity() - 1) % CircularBufferFilterW<uint16_t>::capacity();
        return labelWithEquivalenceAndMerge(pos,vl,rw,cl);
		
	}

    void recycleLabel(uint16_t lbl)
    {
		// check if label is valid
		if (lbl == 0) return;

		// push label to recycled stack
		recycledLabels.push_back(lbl);
	}
};