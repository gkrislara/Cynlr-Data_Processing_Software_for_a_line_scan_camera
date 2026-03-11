#pragma once


#include "..\Shared\Platform.h"
#include "..\Shared\Data.h"
#include <stdint.h>
#include <algorithm>
#include <vector>


#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

template <size_t k> //numpixels
class ConnectedComponentLabeller: public CircularBufferFilterW<uint16_t>
{
	 
	 uint16_t width;
	 uint32_t sz;
	 uint16_t c_head;
	 uint16_t c_pos;
	 uint16_t label;

	 std::vector<uint16_t> parent_label;

	 uint16_t neighbourOf(uint8_t i, size_t pos)
	 {				
		 uint32_t nbr[4] = {
				(pos + sz - 1) % sz,  // left
				(pos + sz - uint32_t(width) - 1) % sz,  // top-left
				(pos + sz - uint32_t(width)) % sz,  // top
				(pos + sz - uint32_t(width) + 1) % sz   // top-right
		 };

		 return CircularBufferFilterW::getBufferAt(nbr[i]);
	 }
	 
public:

	ConnectedComponentLabeller(uint32_t size) : CircularBufferFilterW<uint16_t>(size + k + 1)
	{
		width = size;
		sz = size + k + 1;
		c_head = 0;
		label = 1;
		c_pos = 0;

		/* initialize the buffer with 0s, head at 0, tail at 0, count at 0*/

		CircularBufferFilterW<uint16_t>::clear();
	}

	uint16_t algorithm(uint16_t val)
	{
		c_head = CircularBufferFilterW::getHead();
		c_pos = (c_head - 1) % sz;

		if (CircularBufferFilterW<uint16_t>::getBufferAt(c_pos) == 0)
			return 0;

		else
		{

			uint16_t row = CircularBufferFilterW<uint16_t>::level() / width;
			uint16_t col = CircularBufferFilterW<uint16_t>::level() % width;

			if (CircularBufferFilterW::level() == 0)
			{
				/* The first 1' will get label 1 */
				CircularBufferFilterW<uint16_t>::setBufferAt(c_pos, label);
				return CircularBufferFilterW<uint16_t>::getBufferAt(c_pos);
			}
			else
			{
				// Merger logic
				/* in buffer,
					from the position of head */



				if (row == 0)
				{
					if (neighbourOf(0, c_pos) == 0)
					{
						CircularBufferFilterW<uint16_t>::setBufferAt(c_pos,label++);
						return CircularBufferFilterW<uint16_t>::getBufferAt(c_pos);
					}
					else // Assumption : label assigned already
					{
						templbl = neighbourOf(0, c_pos);
						CircularBufferFilterW<uint16_t>::setBufferAt(c_pos, templbl);
						return templbl;
					}

				}
				else if (col == 0)
				{
					uint8_t zerocount = 0;
					for (uint8_t i = 2; i < 4; i++)
					{
						//compare with top and top-right

						if (neighbourOf(i, c_pos) == 0) zerocount++;
						else
						{
							templbl = std::min(templbl, neighbourOf(i, c_pos));
						}
					}

					if(zerocount == 2)
					{
						CircularBufferFilterW<uint16_t>::setBufferAt(c_pos,label++);
						return CircularBufferFilterW<uint16_t>::getBufferAt(c_pos);
					}
					else
					{
						CircularBufferFilterW<uint16_t>::setBufferAt(c_pos, templbl);
						return CircularBufferFilterW<uint16_t>::getBufferAt(c_pos);
					}
				}
				else if (col == (width - 1))
				{
					uint8_t zerocount = 0;
					for (uint8_t i = 0; i < 3; i++)
					{
						//compare with top and top left
						if (neighbourOf(i, c_pos) == 0) zerocount++;
						else
						{
							templbl = std::min(templbl, neighbourOf(i, c_pos));
						}
					}

					if (zerocount == 3) {
						CircularBufferFilterW<uint16_t>::setBufferAt(c_pos,label++);
						return CircularBufferFilterW<uint16_t>::getBufferAt(c_pos);
					}
					else
					{
						CircularBufferFilterW<uint16_t>::setBufferAt(c_pos, templbl);
						return CircularBufferFilterW<uint16_t>::getBufferAt(c_pos);
					}
				}
				else
				{
					// nominal case
					uint8_t zerocount = 0;

					for (int i = 0; i < 4; ++i)
					{
						
						if (neighbourOf(i, c_pos) == 0) zerocount++;
						else
						{
							templbl = std::min(templbl, neighbourOf(i, c_pos));
						}
					}

					if(zerocount == 4)
					{
						CircularBufferFilterW<uint16_t>::setBufferAt(c_pos,label++);
						return CircularBufferFilterW<uint16_t>::getBufferAt((c_pos - 1) % sz);
					}
					else
					{
						CircularBufferFilterW<uint16_t>::setBufferAt(c_pos, templbl);
						return CircularBufferFilterW<uint16_t>::getBufferAt(c_pos);
					}
				}
			}
		}
	}
};




//template <size_t k> // numpixels
//class ConnectedComponentLabeller : public CircularBufferFilterW<uint16_t>
//{
//    uint16_t width;
//    uint32_t sz;
//    uint16_t label;
//    std::vector<uint16_t> parent_label;
//
//    // Union-find helpers
//    uint16_t find(uint16_t x)
//    {
//        if (x == 0) return 0;
//        if (parent_label[x] != x)
//            parent_label[x] = find(parent_label[x]);
//        return parent_label[x];
//    }
//
//    void unite(uint16_t x, uint16_t y)
//    {
//        if (x == 0 || y == 0) return;
//        uint16_t xr = find(x);
//        uint16_t yr = find(y);
//        if (xr != yr)
//            parent_label[std::max(xr, yr)] = std::min(xr, yr);
//    }
//
//    uint16_t neighbourOf(uint8_t i, size_t pos)
//    {
//        uint32_t nbr[4] = {
//            (pos + sz - 1) % sz,                        // left
//            (pos + sz - uint32_t(width) - 1) % sz,      // top-left
//            (pos + sz - uint32_t(width)) % sz,          // top
//            (pos + sz - uint32_t(width) + 1) % sz       // top-right
//        };
//        return CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i]);
//    }
//
//public:
//    ConnectedComponentLabeller(uint32_t size)
//        : CircularBufferFilterW<uint16_t>(size + k + 1,0)
//    {
//        width = size;
//        sz = size + k + 1;
//        label = 1;
//        parent_label.resize(sz * 2, 0); // Large enough for all possible labels
//        CircularBufferFilterW<uint16_t>::clear();
//    }
//
//    // Call this for each pixel (0=background, 1=foreground)
//    uint16_t algorithm(uint8_t pixelValue)
//    {
//        // Advance buffer head for new pixel
//        uint16_t c_head = CircularBufferFilterW<uint16_t>::getHead();
//        uint16_t c_pos = (c_head - 1) % sz;
//
//        if (pixelValue == 0)
//        {
//            CircularBufferFilterW<uint16_t>::push(0);
//            return 0;
//        }
//
//        // Gather neighbor labels
//        uint16_t n[4];
//        for (int i = 0; i < 4; ++i)
//            n[i] = neighbourOf(i, c_pos);
//
//        // Find minimum nonzero label among neighbors
//        uint16_t minLabel = UINT16_MAX;
//        for (int i = 0; i < 4; ++i)
//            if (n[i] != 0)
//                minLabel = std::min(minLabel, find(n[i]));
//
//        if (minLabel == UINT16_MAX)
//        {
//            // No labeled neighbors, assign new label
//            CircularBufferFilterW<uint16_t>::push(label);
//            parent_label[label] = label;
//            return label++;
//        }
//        else
//        {
//            // Assign min label and merge equivalences
//            CircularBufferFilterW<uint16_t>::push(minLabel);
//            for (int i = 0; i < 4; ++i)
//                if (n[i] != 0 && find(n[i]) != minLabel)
//                    unite(minLabel, n[i]);
//            return minLabel;
//        }
//    }
//
//    // Optional: flatten all label equivalences after processing
//    void flattenLabels()
//    {
//        for (uint16_t i = 1; i < label; ++i)
//            parent_label[i] = find(i);
//    }
//
//    // Get resolved label for a given buffer index
//    uint16_t getResolvedLabel(uint32_t idx)
//    {
//        uint16_t raw = CircularBufferFilterW<uint16_t>::getBufferAt(idx);
//        return raw ? find(raw) : 0;
//    }
//};