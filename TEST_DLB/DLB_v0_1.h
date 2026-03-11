#pragma once

#include "..\Shared\Platform.h"
#include "..\Shared\Data.h"
#include <algorithm>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif


//New algorithm:

//outer loop - get pixel value

/* in buffer,
from the position of head */
/* find non - zero min value with its neighbours */
/* and
return the value else
increment nextlabel
and
return the value of next label*/

// outer loop 
//assign label in pixel struct
// insert label in the buffer




template <size_t k> //numpixels
class ConnectedComponentLabeller : public CircularBufferFilterW<uint16_t> 
{
    uint16_t nextLabel;
    uint16_t width;
	uint32_t sz;
    uint16_t c_head;
    uint16_t c_tail;
    uint16_t templbl;

public:
	ConnectedComponentLabeller(uint32_t size) : CircularBufferFilterW(size, 0) // equivalent to saying fill buffer with zeros until headPos
    {
        nextLabel = 1;
        width = size;
		sz = size + k + 1;
        templbl = 0;
        c_pos = 0;
    }

    uint16_t algorithm(uint16_t vl)
    {
		if (vl == 1) 
        {
            
            if (CircularBufferFilterW<uint16_t>::level() == 0)
            {
                return nextLabel++;
			}
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
                    templbl = std::min(
                        CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i]),
                        CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i - 1])
                    );
            } 

            /* and
               return the value else
               increment nextlabel
               and
               return the value of next label*/
            if (templbl > 0)
            {
                return templbl;

            }
            else return nextLabel++;
        }
        else return 0;
    }

};


/**/
//void mergeWithNeighbor(uint32_t nbr_idx, uint16_t& L) 
//{
//    /* inputs nbr[i], L to be assigned */

//    uint16_t l = CircularBufferFilterW<uint16_t>::getBufferAt(nbr_idx);

//    if (l == 0) return;
//    //if (L == 0)     L = l;
//    //else if (l != L)
//    //{
//    //    /* any other label - merge inplace with out having to have a seperate buffer */
//    //    CircularBufferFilterW<uint16_t>::setBufferAt(idx, L);
//    //}
//}


/* New ALgorithm */

// 1. Find the current position - done
// 2. deduce col, row by buf::count - done
// 3. clockwise NON -ZERO traversal , FIND pair wise min non- zero val = label -done 
// 4 if not, then assign new label and store in buffer -done 


//uint16_t labelWithEquivalenceAndMerge(uint16_t pos, uint16_t val)
//  {
//      if (val == 0)
//      {
//          return 0;
//      }
//      /* get row and col*/
      //uint16_t row = CircularBufferFilterW<uint16_t>::level() / width;
//      uint16_t col = CircularBufferFilterW<uint16_t>::level() % width;
//          /* Add reuse logic here */
// 
//       uint16_t L = nextLabel++;
//       CircularBufferFilterW<uint16_t>::setBufferAt(pos, L);


//      uint32_t nbr[4] = {
//          (pos + sz - 1) % sz,  // left
//          (pos + sz - uint32_t(width) - 1) % sz,  // top-left
//          (pos + sz - uint32_t(width)) % sz,  // top
//          (pos + sz - uint32_t(width) + 1) % sz   // top-right
//      };


//      
//      //Boundary / Edge Case
      //if (col == 0) // check nbr[2] and nbr[3] only
//          {
//          /* if its the first column then we only need to check the above row neighbors */
//          if (row > 0)
//          {
//              if (CircularBufferFilterW<uint16_t>::getBufferAt(nbr[2]) == 0 || CircularBufferFilterW<uint16_t>::getBufferAt(nbr[3]) == 0)
//                  return 0;
//              else
//                  //find non-zero min out these 4 neighbours 
//                  templbl = std::min(
//                      CircularBufferFilterW<uint16_t>::getBufferAt(nbr[3]),
//                      CircularBufferFilterW<uint16_t>::getBufferAt(nbr[2])
//                  );
//          }
      //}
//      //Boundary / Edge Casw
//      if (col == (width - 1)  // check nbr[0], nbr[1] and nbr[2] only
      //	)
//      {
//          /* if its the last column then we only need to check the above row neighbors and left neighbor */
//          //mergeWithNeighbor(pos, L, -1); // left
//          if (row > 0)
//          {
//              for (int i = 1; i < 3; ++i)
//              {
//                  /* if label == 0 continue */
//                  if ((CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i]) == 0) ||
//                      (CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i - 1]) == 0 ))
//                      continue;
//                  else
//                      //find non-zero min out these 4 neighbours 
//                      templbl = std::min(
//                          CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i]),
//                          CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i - 1])
//                      );
//              }
//          }
      //}
//      //Boundary / Edge Case
//      if (col > 0) // Left neighbor - nbr[0]
//          if (nbr[0] > 0)
//              templbl = CircularBufferFilterW<uint16_t>::getBufferAt(nbr[0]);

      //// Nominal case - for the rest of the columns check all 4 neighbors
//      for( int i = 1; i < 4; ++i)
//      {
//          /* if label == 0 continue */
//          if ((CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i]) == 0) || 
//               (CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i - 1]) == 0))
//              continue;
//          else
//              //find non-zero min out these 4 neighbours 
//              templbl = std::min(
//                  CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i]),
//                  CircularBufferFilterW<uint16_t>::getBufferAt(nbr[i - 1])
//              );
      //}

      //nextLabel = std::max(nextLabel, L);
//      
//      L = templbl;

//      CircularBufferFilterW<uint16_t>::setBufferAt(pos, L);
//      return L;
//  }





