#include <stdio.h>
#include <fuse.h>

size_t cache_size;
size_t block_size;

struct dualFileHandle
{
  uint64_t nasFH;
  uint64_t cacheFH;
};

struct fuse_file_info openCacheFile;

off_t alignLowerOffset(off_t offset);

off_t alignUpperOffset(off_t offset);
