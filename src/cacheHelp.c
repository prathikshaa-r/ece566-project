#include <stdio.h>
#include "cacheHelp.h"

off_t alignLowerOffset(off_t offset)
{
  off_t alignedOffset = offset - offset%block_size;
  return alignedOffset;
}

off_t alignUpperOffset(off_t offset)
{
  off_t alignedOffset = offset + offset%block_size;
  return alignedOffset;
}


