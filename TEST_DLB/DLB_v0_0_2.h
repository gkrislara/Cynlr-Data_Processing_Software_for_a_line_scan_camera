#pragma once


#include "..\Shared\Platform.h"
#include "..\Shared\Data.h"
#include <stdint.h>
#include <algorithm>


#ifdef min
#undef min
#endif


template <size_t k> //numpixels
class ConnectedComponentLabeller: public KModCounterW<uint16_t>
{
	 
	 uint16_t width;
	 uint32_t sz;
	 uint16_t c_head;
	 uint16_t c_pos;
	 uint16_t templbl;
	 uint16_t label;

	 uint16_t neighbourOf(uint8_t i, size_t pos)
	 {				
		 uint32_t nbr[4] = {
				(pos + sz - 1) % sz,  // left
				(pos + sz - uint32_t(width) - 1) % sz,  // top-left
				(pos + sz - uint32_t(width)) % sz,  // top
				(pos + sz - uint32_t(width) + 1) % sz   // top-right
		 };

		 return KModCounterW::get(nbr[i]);
	 }
	 
public:

	ConnectedComponentLabeller(uint32_t size) : KModCounterW<uint16_t>(size + k + 1)
	{
		width = size;
		sz = size + k + 1;
		templbl = UINT16_MAX;
		c_head = 0;
		label = 1;
		c_pos = 0;

		/* initialize the buffer with 0s, head at 0, tail at 0, count at 0*/
		for (uint16_t i = 0; i < sz; i++)
		{
			KModCounterW<uint16_t>::reset(i);
		}

		CircularBufferFilterW<uint16_t>::clear();
	}

	uint16_t algorithm(uint8_t val)
	{
		c_head = KModCounterW::getHead();
		c_pos = (c_head - 1) % sz;
		if (val == 0)
		{	
			KModCounterW<uint16_t>::reset(c_pos);
			KModCounterW<uint16_t>::incrementHead();
			return KModCounterW<uint16_t>::get(c_pos);

			 /* if the value is 0, return 0 and increment head */
			 /* else, if the value is 1, check the neighbours and return the label according to the rules */
			 /* if no non-zero neighbour, create new label and return it*/
		}
		else
		{
			if (KModCounterW::level() == 0)
			{
				/* The first 1' will get label 1 */
				KModCounterW<uint16_t>::set(c_pos, label);
				KModCounterW<uint16_t>::incrementHead();
				return KModCounterW<uint16_t>::get(c_pos);
			}
			else
			{
				// Merger logic
				/* in buffer,
					from the position of head */

				uint16_t row = KModCounterW<uint16_t>::level() / width;
				uint16_t col = KModCounterW<uint16_t>::level() % width;

				if (row == 0)
				{
					if (neighbourOf(0, c_pos) == 0)
					{
						KModCounterW<uint16_t>::increment(c_pos,label++);
						KModCounterW<uint16_t>::incrementHead();
						return KModCounterW<uint16_t>::get(c_pos);
					}
					else
					{
						templbl = neighbourOf(0, c_pos);
						KModCounterW<uint16_t>::set(c_pos, templbl);
						KModCounterW<uint16_t>::incrementHead();
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
						KModCounterW<uint16_t>::increment(c_pos,label++);
						KModCounterW<uint16_t>::incrementHead();
						return KModCounterW<uint16_t>::get(c_pos);
					}
					else
					{
						KModCounterW<uint16_t>::set(c_pos, templbl);
						KModCounterW<uint16_t>::incrementHead();
						return KModCounterW<uint16_t>::get(c_pos);
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
						KModCounterW<uint16_t>::increment(c_pos,label++);
						KModCounterW<uint16_t>::incrementHead();
						return KModCounterW<uint16_t>::get(c_pos);
					}
					else
					{
						KModCounterW<uint16_t>::set(c_pos, templbl);
						KModCounterW<uint16_t>::incrementHead();
						return KModCounterW<uint16_t>::get(c_pos);
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
						KModCounterW<uint16_t>::increment(c_pos,label++);
						KModCounterW<uint16_t>::incrementHead();
						return KModCounterW<uint16_t>::get((c_pos - 1) % sz);
					}
					else
					{
						KModCounterW<uint16_t>::set(c_pos, templbl);
						KModCounterW<uint16_t>::incrementHead();
						return KModCounterW<uint16_t>::get(c_pos);
					}
				}
			}
		}
	}
};