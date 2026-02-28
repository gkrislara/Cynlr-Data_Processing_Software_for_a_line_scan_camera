#pragma once


#include "..\Shared\Platform.h"
#include "..\Shared\Data.h"
#include <stdint.h>
#include <algorithm>


#ifdef min
#undef min
#endif


template <size_t k> //numpixels
class ConnectedComponentLabeller : public CircularBufferFilterW<uint16_t>
{
	 
	 uint16_t width;
	 uint32_t sz;
	 uint16_t c_head;
	 uint16_t c_tail;
	 uint16_t templbl;

	 KModCounterW<uint16_t, UINT16_MAX> label;

public:

	ConnectedComponentLabeller(uint32_t size) : CircularBufferFilterW(size+k+1, 0) // equivalent to saying fill buffer with zeros until headPos
	{
		label.reset();
		width = size;
		sz = size + k + 1;
		templbl = UINT16_MAX;
	}

	uint16_t algorithm(uint8_t val)
	{
		size_t c_pos = CircularBufferFilterW<uint16_t>::getHead();

		if (val == 0)
		{	
			label.reset();
			uint16_t lbl = label.get();
			CircularBufferFilterW<uint16_t>::setBufferAt(
				c_pos, lbl
			);
			return lbl;
		}
		else
		{
			if (CircularBufferFilterW<uint16_t>::level() == 0)
			{
				/* The first 1' will get label 1 */
				return label.increment();
			}
			else
			{
				// Merger logic
				/* in buffer,
					from the position of head */
				
				c_head = CircularBufferFilterW<uint16_t>::getHead();
				c_tail = CircularBufferFilterW<uint16_t>::getTail();

				/* find non - zero min value with its neighbours */
				uint32_t nbr[4] = {
					(c_head + sz - 1) % sz,  // left
					(c_head + sz - uint32_t(width) - 1) % sz,  // top-left
					(c_head + sz - uint32_t(width)) % sz,  // top
					(c_head + sz - uint32_t(width) + 1) % sz   // top-right
				};

				uint16_t row = CircularBufferFilterW<uint16_t>::level() / width;
				uint16_t col = CircularBufferFilterW<uint16_t>::level() % width;

				if (row == 0)
				{
					if (!CircularBufferFilterW<uint16_t>::getBufferAt(nbr[0])) 
						return 0;
					else
					{
						label.set(CircularBufferFilterW<uint16_t>::getBufferAt(nbr[0]));
						/*CircularBufferFilterW<uint16_t>::setBufferAt(c_pos, label.get());*/
						return label.get();
					}
				}
				else if (col == 0)
				{
					
					for (uint8_t i = 2; i < 4; i++)
					{
						templbl = std::min(templbl, CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i]));
					}

					if (templbl == 0) return 0;
					else
					{
						label.set(templbl);
						return label.get();
					}
				}
				else if (col == (width - 1))
				{
					for (uint8_t i = 1; i < 3; i++)
					{
						templbl = std::min(templbl, CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i]));
					}

					if (templbl == 0) return 0;
					else
					{
						label.set(templbl);
						return label.get();
					}
				}
				else
				{
					// nominal case
					for (int i = 1; i < 4; ++i)
					{
						/*if label == 0 continue*/
						if ((
							(CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i]) == 0) ||
							(CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i - 1]) == 0)
							) ||
							(nbr[i] > c_pos && c_tail < c_pos))
							continue;
						else
						//	//find non-zero min out these 4 neighbours 
						templbl = std::min(
							CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i]),
							CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i - 1])
						);


						//if (CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i]) != 0)
						// {
						//	 templbl = std::min(templbl, CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i]));
						//}

					}
					/* and
					   return the value else
					   increment nextlabel
					   and
					   return the value of next label*/
					// Merger logic

					// no non-zero neighbour, create new label

					if (templbl == 0 || templbl == UINT16_MAX)
					{

						/* Recycling logic here*/
						uint16_t nextLabel = label.increment();
						return nextLabel;
					}
					else
					{
						label.set(templbl);
						return label.get();
					}
				}		
			}
		}
	}
};