#pragma once


#include "..\Shared\Platform.h"
#include "..\Shared\Data.h"
#include <stdint.h>
#include <algorithm>


template <size_t k> //numpixels
class ConnectedComponentLabeller : public CircularBufferFilterW<uint16_t>
{
	 uint16_t nextLabel;
	 uint16_t width;
	 uint32_t sz;
	 uint16_t c_head;
	 uint16_t c_tail;
	 uint16_t templbl;
	 KModCounterW<uint16_t, UINT16_MAX>* kmodCounter;
public:

	ConnectedComponentLabeller(uint32_t size) : CircularBufferFilterW(size+k+1, 0) // equivalent to saying fill buffer with zeros until headPos
	{
		nextLabel = 0;
		width = size;
		sz = size + k + 1;
		templbl = 0;

		kmodCounter = new KModCounterW<uint16_t, UINT16_MAX>[sz];	
	}

	void algorithm(bool val)
	{
		size_t c_pos = CircularBufferFilterW<uint16_t>::getHead();

		if (!val)
		{	
			kmodCounter[c_pos].reset();
			CircularBufferFilterW<uint16_t>::push(0);
		}
		else
		{
			if (CircularBufferFilterW<uint16_t>::level() == 0)
			{
				CircularBufferFilterW<uint16_t>::push(kmodCounter[c_pos].increment());
			}
			else
			{
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
				for (int i = 1; i < 4; ++i)
				{
					/* if label == 0 continue */
					if ((
						(CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i]) == 0) ||
						(CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i - 1]) == 0)
						) ||
						(nbr[i] > c_pos && c_tail < c_pos))
						continue;
					else
						//find non-zero min out these 4 neighbours 
					/*	templbl = std::min(
							CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i]),
							CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i - 1])
						);*/

						templbl = std::min(templbl, CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i]));

				}
				/* and
				   return the value else
				   increment nextlabel
				   and
				   return the value of next label*/
				if (templbl > 0) CircularBufferFilterW<uint16_t>::push(templbl);
				else {

					/* Recycling logic here*/
					nextlabel = kmodCounter[c_pos].increment();
					
					CircularBufferFilterW<uint16_t>::push(nextLabel);
				}
					
			}
		}
	}




};