#include <stdio.h>

size_t cache_size;
size_t block_size;

off_t alignLowerOffset(off_t offset);

off_t alignUpperOffset(off_t offset);