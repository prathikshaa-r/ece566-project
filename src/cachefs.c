/*
  Big Brother File System
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.
  A copy of that code is included in the file fuse.h

  The point of this FUSE filesystem is to provide an introduction to
  FUSE.  It was my first FUSE filesystem as I got to know the
  software; hopefully, the comments in this code will help people who
  follow later to get a gentler introduction.

  This might be called a no-op filesystem:  it doesn't impose
  filesystem semantics on top of any other existing structure.  It
  simply reports the requests that come in, and passes them to an
  underlying filesystem.  The information is saved in a logfile named
  cfsfs.log, in the directory from which you run cfsfs.

  Refernces:
  pathToFileName - https://stackoverflow.com/questions/5457608/how-to-remove-the-character-at-a-given-index-from-a-string-in-c "Fabio Cabral"


*/
#include "config.h"
#include "params.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include <time.h>
#include "log.h"
#include "cacheHelp.h"
#include "metadata/meta.h"

sqlite3 *metaDataBase;

//  All the paths I see are relative to the root of the mounted
//  filesystem.  In order to get to the underlying NAS, I need to
//  have the NAS root directory.  I'll save it away early on in main(), and then
//  whenever I need a path for something I'll call this to construct
//  it.
static void cfs_fullNasPath(char nasPath[PATH_MAX], const char *path) {
  strcpy(nasPath, CFS_DATA->nasdir);
  strncat(nasPath, path, PATH_MAX); // ridiculously long paths will
                                  // break here

  log_msg("    cfs_fullNasPath:  nasdir = \"%s\", path = \"%s\", nasPath = \"%s\"\n",
          CFS_DATA->nasdir, path, nasPath);
}


//Similarly to fullNasPath, when I want to address cachefiles
//I will append our relative path to the absolute path of the cache root directory
static void cfs_fullCachePath(char cachePath[PATH_MAX], const char *path) {
  strcpy(cachePath, CFS_DATA->cachedir);
  strncat(cachePath, path, PATH_MAX); // ridiculously long paths will
                                  // break here

  log_msg("    cfs_fullCachePath:  cachedir = \"%s\", path = \"%s\", cachePath = \"%s\"\n",
          CFS_DATA->cachedir, path, cachePath);
}

//Cache files are stored as the relative paths in the NAS for uniqueness and so 
//that calls to nonexistant cache directories are not a concern 
static void cfs_pathToFileName(char pathFileName[PATH_MAX], const char *path) {
  strcpy(pathFileName, path);

  char *src, *dst;
  for (src = dst = pathFileName; *src != '\0'; src++)
  {
    *dst = *src;
    if (*dst != '/' || src == pathFileName) dst++;
  }
  *dst = '\0';

  log_msg("    cfs_pathToFileName: path = \"%s\", fileName = \"%s\"\n",
          path, pathFileName);
}

///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come from /usr/include/fuse.h
//
/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int cfs_getNASattr(const char *path, struct stat *statbuf) {
  int retstat = 0;
  char nasPath[PATH_MAX];

  log_msg("\ncfs_getNASattr(path=\"%s\", statbuf=0x%08x)\n", path, statbuf);
  cfs_fullNasPath(nasPath, path);

  retstat = log_syscall("lstat", lstat(nasPath, statbuf), 0);

  log_stat(statbuf);

  return retstat;
}

//Primarily used for getting modification times from cache files for coherency checks 
int cfs_getCacheattr(const char *path, struct stat *statbuf) {
  int retstat = 0;
  char cacheFileName[PATH_MAX], cachePath[PATH_MAX];

  cfs_pathToFileName(cacheFileName, path);
  cfs_fullCachePath(cachePath, cacheFileName);

  log_msg("\ncfs_getCacheattr(path=\"%s\", statbuf=0x%08x)\n", path, statbuf);

  retstat = log_syscall("lstat", lstat(cachePath, statbuf), 0);

  log_stat(statbuf);

  return retstat;
}

//Used to print st_mtime in cfs_open
char* formatdate(char* str, time_t val)
{
        strftime(str, 36, "%d.%m.%Y %H:%M:%S", localtime(&val));
        return str;
}

/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.  If the linkname is too long to fit in the
 * buffer, it should be truncated.  The return value should be 0
 * for success.
 */
// Note the system readlink() will truncate and lose the terminating
// null.  So, the size passed to to the system readlink() must be one
// less than the size passed to cfs_readlink()
// cfs_readlink() code by Bernardo F Costa (thanks!)
int cfs_readlink(const char *path, char *link, size_t size) {
  int retstat;
  char nasPath[PATH_MAX];

  log_msg("\ncfs_readlink(path=\"%s\", link=\"%s\", size=%d)\n", path, link,
          size);
  cfs_fullNasPath(nasPath, path);

  retstat = log_syscall("readlink", readlink(nasPath, link, size - 1), 0);
  if (retstat >= 0) {
    link[retstat] = '\0';
    retstat = 0;
    log_msg("    link=\"%s\"\n", link);
  }

  return retstat;
}

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
// shouldn't that comment be "if" there is no.... ?
int cfs_mknod(const char *path, mode_t mode, dev_t dev) {
  int retstat;
  char nasPath[PATH_MAX];

  log_msg("\ncfs_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n", path, mode, dev);
  cfs_fullNasPath(nasPath, path);

  if(mode == 0100000)
  {
    log_msg("\nChanging mode...\n");
    mode = 0100666;
  }

  // On Linux this could just be 'mknod(path, mode, dev)' but this
  // tries to be be more portable by honoring the quote in the Linux
  // mknod man page stating the only portable use of mknod() is to
  // make a fifo, but saying it should never actually be used for
  // that.
  if (S_ISREG(mode)) {
    retstat =
        log_syscall("open", open(nasPath, O_CREAT | O_EXCL | O_WRONLY, mode), 0);
    if (retstat >= 0)
      retstat = log_syscall("close", close(retstat), 0);
  } else if (S_ISFIFO(mode))
    retstat = log_syscall("mkfifo", mkfifo(nasPath, mode), 0);
  else
    retstat = log_syscall("mknod", mknod(nasPath, mode, dev), 0);

  return retstat;
}

/** Create a directory */
int cfs_mkdir(const char *path, mode_t mode) {
  char nasPath[PATH_MAX];

  log_msg("\ncfs_mkdir(path=\"%s\", mode=0%3o)\n", path, mode);
  cfs_fullNasPath(nasPath, path);

  return log_syscall("mkdir", mkdir(nasPath, mode), 0);
}

/** Remove a file from both the NAS Directory and Cache Directory
    Calls to cache directory are expected to complete due to remove calls 
    creating files in cache (ie open is called by rm*/
int cfs_unlink(const char *path) {
  char nasPath[PATH_MAX];
  char cacheFileName[PATH_MAX];
  char cachePath[PATH_MAX];
  cfs_pathToFileName(cacheFileName, path);
  cfs_fullCachePath(cachePath, cacheFileName);

  cfs_fullNasPath(nasPath, path);
  log_msg("cfs_unlink(nasPath=\"%s\", cachePath=\"%s\")\n", nasPath, cachePath);

  delete_file(metaDataBase, cacheFileName);//remove file from metadata file
  log_syscall("Cache unlink", unlink(cachePath), 0);
  return log_syscall("NAS unlink", unlink(nasPath), 0);
}

/** Remove a directory, only from NAS directory */
int cfs_rmdir(const char *path) {
  char nasPath[PATH_MAX];

  log_msg("cfs_rmdir(path=\"%s\")\n", path);
  cfs_fullNasPath(nasPath, path);

  return log_syscall("rmdir", rmdir(nasPath), 0);
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int cfs_symlink(const char *path, const char *link) {
  char nasLink[PATH_MAX];
  char cacheFileName[PATH_MAX], cachePath[PATH_MAX];
  char cacheLinkName[PATH_MAX], cacheLinkPath[PATH_MAX];

  log_msg("\ncfs_symlink(path=\"%s\", link=\"%s\")\n", path, link);
  cfs_fullNasPath(nasLink, link);

  cfs_pathToFileName(cacheFileName, path);
  cfs_pathToFileName(cacheLinkName, link);
  cfs_fullCachePath(cachePath, cacheFileName);
  cfs_fullCachePath(cacheLinkPath, cacheLinkName);

  log_syscall("Cache symlink", symlink(cachePath, cacheLinkPath), 0);
  return log_syscall("NAS symlink", symlink(path, nasLink), 0);
}

/** Rename a file */
// both path and newpath are fs-relative
int cfs_rename(const char *path, const char *newpath) {
  char nasPath[PATH_MAX], nasNewPath[PATH_MAX];
  char cacheFileName[PATH_MAX], cachePath[PATH_MAX];
  char cacheNewName[PATH_MAX], cacheNewPath[PATH_MAX];

  log_msg("\ncfs_rename(nasPath=\"%s\", newpath=\"%s\")\n", path, newpath);
  cfs_fullNasPath(nasPath, path);
  cfs_fullNasPath(nasNewPath, newpath);

  cfs_pathToFileName(cacheFileName, path);
  cfs_pathToFileName(cacheNewName, newpath);

  cfs_fullCachePath(cachePath, cacheFileName);
  cfs_fullCachePath(cacheNewPath, cacheNewName);

  log_syscall("Cache rename", rename(cachePath, cacheNewPath), 0);
  return log_syscall("NAS rename", rename(nasPath, nasNewPath), 0);
}

/** Create a hard link to a file */
int cfs_link(const char *path, const char *newpath) {
  char nasPath[PATH_MAX], nasLinkPath[PATH_MAX];
  char cacheFileName[PATH_MAX], cacheLinkName[PATH_MAX];
  char cachePath[PATH_MAX], cacheLinkPath[PATH_MAX];

  cfs_fullNasPath(nasPath, path);
  cfs_fullNasPath(nasLinkPath, newpath);

  cfs_pathToFileName(cacheFileName, path);
  cfs_pathToFileName(cacheLinkName, newpath);

  cfs_fullCachePath(cachePath, cacheFileName);
  cfs_fullCachePath(cacheLinkPath, cacheLinkName);

  log_msg("\ncfs_link(path=\"%s\", newpath=\"%s\")\nCache link: \"%s\"\n", path, newpath, cacheLinkPath);

  log_syscall("Cache link", link(cachePath, cacheLinkPath), 0);
  return log_syscall("NAS link", link(nasPath, nasLinkPath), 0);
}

/** Change the permission bits of a file */
int cfs_chmod(const char *path, mode_t mode) {
  char nasPath[PATH_MAX];

  log_msg("\ncfs_chmod(nasPath=\"%s\", mode=0%03o)\n", path, mode);
  cfs_fullNasPath(nasPath, path);

  return log_syscall("chmod", chmod(nasPath, mode), 0);
}

/** Change the owner and group of a file */
int cfs_chown(const char *path, uid_t uid, gid_t gid)

{
  char nasPath[PATH_MAX];

  log_msg("\ncfs_chown(path=\"%s\", uid=%d, gid=%d)\n", path, uid, gid);
  cfs_fullNasPath(nasPath, path);

  return log_syscall("chown", chown(nasPath, uid, gid), 0);
}

/** Change the size of a file */
int cfs_truncate(const char *path, off_t newsize) {
  char nasPath[PATH_MAX];
  char cacheFileName[PATH_MAX];
  char cachePath[PATH_MAX];
  cfs_pathToFileName(cacheFileName, path);
  cfs_fullCachePath(cachePath, cacheFileName);
  log_msg("\ncfs_truncate(path=\"%s\", cachePath=\"%s\", newsize=%lld)\n", path, cachePath, newsize);
  cfs_fullNasPath(nasPath, path);

  log_syscall("Cache truncate", truncate(cachePath, newsize),0);
  return log_syscall("NAS truncate", truncate(nasPath, newsize), 0);
}

/** Change the access and/or modification times of a file */
/* note -- I'll want to change this as soon as 2.6 is in debian testing */
int cfs_utime(const char *path, struct utimbuf *ubuf) {
  char nasPath[PATH_MAX];
  char cacheFileName[PATH_MAX];
  char cachePath[PATH_MAX];

  cfs_fullNasPath(nasPath, path);
  cfs_pathToFileName(cacheFileName, path);
  cfs_fullCachePath(cachePath, cacheFileName);

  log_msg("\ncfs_utime(path=\"%s\", CachePath=\"%s\", ubuf=0x%08x)\n", path, cachePath, ubuf);
  
  log_syscall("Cache utime", utime(cachePath, ubuf), 0);
  return log_syscall("NAS utime", utime(nasPath, ubuf), 0);
}


//Make a node in the cache directory
int cfs_mkCacheNod(const char *cachePath, mode_t mode, dev_t dev)
{
  int retstat;

  log_msg("\ncfs_mkCacheNod(cachePath=\"%s\", mode=0%3o, dev=%lld)\n", cachePath, mode, dev);

  // On Linux this could just be 'mknod(path, mode, dev)' but this
  // tries to be be more portable by honoring the quote in the Linux
  // mknod man page stating the only portable use of mknod() is to
  // make a fifo, but saying it should never actually be used for
  // that.
  if (S_ISREG(mode)) {
    retstat =
        log_syscall("open", open(cachePath, O_CREAT | O_EXCL | O_WRONLY, mode), 0);
    if (retstat >= 0)

      retstat = log_syscall("close", close(retstat), 0);
  } else if (S_ISFIFO(mode))
    retstat = log_syscall("mkfifo", mkfifo(cachePath, mode), 0);
  else
    retstat = log_syscall("mknod", mknod(cachePath, mode, dev), 0);

  return retstat;
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int cfs_open(const char *path, struct fuse_file_info *fi) {
  int retstat = 0;
  int nasFileDescriptor;
  int cacheFileDescriptor;
  char nasPath[PATH_MAX];
  char cachePath[PATH_MAX];
  char cacheFileName[PATH_MAX];

  cfs_fullNasPath(nasPath, path);
  cfs_pathToFileName(cacheFileName, path);//convert path into the name of the file in our cache by removing "\"
  cfs_fullCachePath(cachePath, cacheFileName);

  log_msg("\ncfs_open(path=\"%s\", fi=0x%08x)\n, flags= %d", path, fi, fi->flags);
  log_msg("\ncfs_open(flag bitwise: %d)\n", fi->flags & 0x3);
  //Make open calls to both the NAS and Cache
  //Return values are file descriptors for each file
  //fi flags are passed to the open call
  //
  //00- RD 01-WOnly 10-RW
  if((fi->flags & 0x3) == 1)//if write only, chang to read write
  {
    int switchedFlag = fi->flags;
    switchedFlag = switchedFlag & 8589934588;
    switchedFlag = switchedFlag | 2;
    nasFileDescriptor = log_syscall("NAS open", open(nasPath, switchedFlag), 0);
  }
  else
  {
    nasFileDescriptor = log_syscall("NAS open", open(nasPath, fi->flags), 0);//
  }

  if(nasFileDescriptor == -1) {
        if (errno == EACCES) {
            struct stat stbuf;
            int needmode;
            
            switch (fi->flags & O_ACCMODE) {
            case O_RDONLY:
                needmode = 0600;
                break;

            case O_WRONLY:
                needmode = 0600;
                break;
                
            default:
                needmode = 0600;
            }
            
            /* try changing permissions */
            if (stat(nasPath, &stbuf) != -1 &&
                chmod(nasPath, stbuf.st_mode | needmode) != -1)  {
                nasFileDescriptor = open(nasPath, fi->flags);
                chmod(nasPath, stbuf.st_mode);
                if (nasFileDescriptor == -1)
                    return -errno;
            } else
                return -EACCES;
        } else
            return -errno;
    }


  bool presentInCache = false;
  presentInCache = is_file_in_cache(metaDataBase, cacheFileName);

  if(nasFileDescriptor < 0)//file was not present on remote server,if in local, delete 
  {
    if(presentInCache)
    {
      log_msg("\nCache file not present in NAS, deleting cache file...\n");
      delete_file(metaDataBase, cacheFileName);//remove file from metadata file
      log_syscall("Cache unlink", unlink(cachePath), 0);
      presentInCache = false;
    }    
    return nasFileDescriptor;
  }
  

  struct stat nasFileInfo;
  cfs_getNASattr(path, &nasFileInfo);

  if(presentInCache)//check up to dateness 
  {
    struct stat cacheFileInfo;
    cfs_getCacheattr(path, &cacheFileInfo);
    char nasDate[36], cacheDate[36];
    log_msg("\nNAS mtime: %s | Cache mtime: %s\n", formatdate(nasDate, nasFileInfo.st_mtime), formatdate(cacheDate, cacheFileInfo.st_mtime));
    if(cacheFileInfo.st_mtime < nasFileInfo.st_mtime)
    {
      log_msg("\nCache file is behind NAS, deleting cache file...\n");
      delete_file(metaDataBase, cacheFileName);//remove file from metadata file
      log_syscall("Cache unlink", unlink(cachePath), 0);
      presentInCache = false;
    }
  }

  if(!presentInCache)//create a new file in the cache dir
  {
    log_msg("\nFile is not present yet in cache. Making node and inserting into database...\n");
    create_file(metaDataBase, cacheFileName, nasFileInfo.st_size);
    cfs_mkCacheNod(cachePath, nasFileInfo.st_mode, nasFileInfo.st_dev);
  }
  cacheFileDescriptor = log_syscall("Cache open", open(cachePath, O_RDWR), 0);

  // if the open call succeeds, my retstat is the file descriptor,
  // else it's -errno.  I'm making sure that in that case the saved
  // file descriptor is exactly -1.
  if (nasFileDescriptor < 0)
    retstat = log_error("Open in NAS Directory.");
  if (cacheFileDescriptor < 0)
    retstat = log_error("Open in Cache Directory.");

  //Set respective file handles for both the fuse_file_infos
  struct dualFileHandle *dualFH;
  dualFH = malloc(sizeof(struct dualFileHandle));
  dualFH->nasFH = nasFileDescriptor;
  dualFH->cacheFH = cacheFileDescriptor;
  fi->fh = (uint64_t)dualFH;
  log_fi(fi);

  return retstat;
}

//Specifically write to cache
//Need for reads so that future reads can be from cache
//Also called by cfs_write when the file is in cache, has same function
int cfs_cacheWrite(char *cacheFileName, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
  int retstat;
  char cachePath[PATH_MAX];
  cfs_fullCachePath(cachePath, cacheFileName);

  struct dualFileHandle *dualFH;
  dualFH = (struct dualFileHandle *)fi->fh;
  log_msg(
      "\ncfs_cacheWrite(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x, cacheFileHandle = 0x % 016llx)\n",
      cachePath, buf, size, offset, fi, dualFH->cacheFH);

  log_fi(fi); 

  //------------Metadata Adjustments for Write--------------// 
  size_t number_blocks = size/block_size;
  size_t offsetArray[number_blocks];
  for(int block_index = 0; block_index < number_blocks; block_index++)
  {
    offsetArray[block_index] = offset+(block_index*block_size);
  }

  while(get_cache_used_size() >= cache_size*1024)//create space in cache through LRU evictions
  {
    log_msg("\nCache is full, evicting blocks...\n");

    char **evictionFileNames;
    size_t *evictedOffsets;
    int *file_ids;

    evictionFileNames = (char **)malloc(sizeof(*evictionFileNames)*number_blocks);
    memset(evictionFileNames, 0, sizeof(*evictionFileNames)*number_blocks);

    evictedOffsets = (size_t *)malloc(sizeof(*evictedOffsets)*number_blocks);
    memset(evictedOffsets, 0, sizeof(*evictedOffsets)*number_blocks);

    file_ids = (int*)malloc(sizeof(*file_ids)*number_blocks);
    memset(file_ids, 0, sizeof(*file_ids)*number_blocks);

    log_msg("\nAbout to evict...\n");

    evict_blocks(metaDataBase, number_blocks, file_ids, evictionFileNames, evictedOffsets);

    log_msg("\nPast evict blocks...\n");
    printf("Requesting %lu blocks to be evicted\n", number_blocks);

    // char* evictionFileNames[number_blocks];
    // size_t evictedOffsets[number_blocks];
    // evict_blocks(metaDataBase, number_blocks, (char**)&evictionFileNames, (size_t*)&evictedOffsets);  
    for(int evict_index = 0; evict_index < number_blocks; evict_index++){
      log_msg("\nEntered for...\n");
      char cachePath[PATH_MAX], fallocateCall[PATH_MAX];
      printf("File to evict:%s:%lu\n", 
        evictionFileNames[file_ids[evict_index]],
        evictedOffsets[evict_index]);
      cfs_fullCachePath(cachePath, evictionFileNames[file_ids[evict_index]]);

      log_msg("\nAbout to copy...\n");
      // strcpy(fallocateCall,"fallocate -p ");
      // strncat(fallocateCall, cachePath, PATH_MAX);

      snprintf(fallocateCall, PATH_MAX, 
        "fallocate -p -o %lu -l %lu -n %s", 
          evictedOffsets[evict_index],
          block_size,
          cachePath);

      log_msg("\nFallocate -p call: %s\n", fallocateCall);

      log_syscall("Cache punching", system(fallocateCall), 0);
    }
   for (int i = 0; i < number_blocks; ++i){
      if(evictionFileNames[i]){
        free(evictionFileNames[i]);
      }
    }

    free(evictionFileNames);
    free(evictedOffsets);
    free(file_ids);
  }

  write_blks(metaDataBase, cacheFileName, number_blocks, (size_t *)&offsetArray);
  //------------End of Metadata Adjustments for Write--------------// 

  return log_syscall("Cache pwrite", pwrite(dualFH->cacheFH, buf, size, offset), 0);
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
// I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the fusexmp code which returns the amount of data also
// returned by read.
int cfs_read(const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi)
{
  int retstat = 0;

  //Set lower and upper offsets as well as aligned size for the file in cache
  //Need to be in whole block increments (ie % = zero) for block tracking purposes (either have entire block or do not)
  off_t lowerOffset = alignLowerOffset(offset);
  size_t alignedSize = block_size*(size/block_size);
  if(size%block_size != 0)
  {
    alignedSize = alignedSize + block_size;
  }
  off_t upperOffset = alignUpperOffset(offset+alignedSize);

  //Check our cache for if all data is present, otherwise write in all blocks (possibly repetitvely) then read
  //Sequential write speed prioritized, could be bad in case of long write that could be sped up through seeks
  size_t number_blocks = alignedSize/block_size;
  int cacheBlockHitYN[number_blocks];
  size_t offsetArray[number_blocks];
  for(int cnt = 0; cnt<number_blocks; cnt++)
  {
    offsetArray[cnt] = lowerOffset+(block_size*cnt);
  }

  char cacheFileName[PATH_MAX];
  cfs_pathToFileName(cacheFileName, path);
  bool cacheDataHit = false;
  int dataCheck = are_blocks_in_cache(metaDataBase, cacheFileName, number_blocks, (size_t *)&offsetArray, (int *)&cacheBlockHitYN); 
  if(dataCheck < 0)
  {
    log_error("Error in are_blocks_in_cache");
  } 
  else
  {
    cacheDataHit = dataCheck;
  }

  struct dualFileHandle *dualFH;
  dualFH = (struct dualFileHandle *)fi->fh;
  log_msg(
      "\ncfs_read original(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x, nasFH = 0x % 016llx, cacheFH = 0x % 016llx)\n",
      path, buf, size, offset, fi, dualFH->nasFH, dualFH->cacheFH);

  char* cacheBuf = malloc(alignedSize*sizeof(char));
  log_msg(
      "\ncfs_read aligned(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x, nasFH = 0x % 016llx, cacheFH = 0x % 016llx)\n",
      path, cacheBuf, alignedSize, lowerOffset, fi, dualFH->nasFH, dualFH->cacheFH);
  // no need to get nasPath on this one, since I work from fi->fh not the path
  log_fi(fi);

  if(cacheDataHit)//have all necessary data in cache, read only from cache
  {
    retstat = log_syscall("Data hit:cache pread", pread(dualFH->cacheFH, cacheBuf, alignedSize, lowerOffset), 0);//do the possibly enlarged read from the NAS
  }
  else//go block by block, reading from nas and writing to cache for non present, reading from cache for present 
  {
    for(int block_index = 0; block_index < number_blocks; block_index++)
    {
      if(cacheBlockHitYN[block_index])//specific block is in cache, read from there
      {
        retstat = retstat + pread(dualFH->cacheFH, cacheBuf+(block_index*block_size), block_size, lowerOffset+(block_index*block_size));
      }
    }
    for(int block_index = 0; block_index < number_blocks; block_index++)
    {
      if(!cacheBlockHitYN[block_index])//specific block is in cache, read from there
      {//specific block is not in cache, read from nas and write to cache for future reads
        retstat = retstat + pread(dualFH->nasFH, cacheBuf+(block_index*block_size), block_size, lowerOffset+(block_index*block_size)); 
        cfs_cacheWrite(cacheFileName, cacheBuf+(block_index*block_size), block_size, lowerOffset+(block_index*block_size), fi);
      }
    }
  }
  memcpy(buf, cacheBuf+offset-lowerOffset, size);
  free((void*)cacheBuf);
  return retstat;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
// As  with read(), the documentation above is inconsistent with the
// documentation for the write() system call.
int cfs_write(const char *path, const char *buf, size_t size, off_t offset,
             struct fuse_file_info *fi) {
  int retstat = 0;

  //Set lower and upper offsets as well as aligned size for the file in cache
  //Need to be in whole block increments (ie % = zero) for block tracking purposes (either have entire block or do not)
  off_t lowerOffset = alignLowerOffset(offset);
  size_t alignedSize = block_size*(size/block_size);
  if(size%block_size != 0)
  {
    alignedSize = alignedSize + block_size;
    log_msg("\nWrite aligned size: %d, size %d, block size %d\n", alignedSize, size, block_size);
  }
  off_t upperOffset = alignUpperOffset(offset+alignedSize);

  struct dualFileHandle *dualFH;
  dualFH = (struct dualFileHandle *)fi->fh;

  log_msg(
      "\ncfs_write original(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x, nasFH = 0x % 016llx, cacheFH = 0x % 016llx)\n",
      path, buf, size, offset, fi, dualFH->nasFH, dualFH->cacheFH);

  log_fi(fi);

  retstat = log_syscall("pwrite", pwrite(dualFH->nasFH, buf, size, offset), 0);//write to NAS here

  //---------------Cache Aspect of Writes---------------//
  struct stat nasAttr;
  cfs_getNASattr(path, &nasAttr);
  size_t nasReadSize = alignedSize;
  if(nasReadSize+lowerOffset > nasAttr.st_size)
  {
    nasReadSize = nasAttr.st_size-lowerOffset;     
  }

  char* cacheBuf = malloc(alignedSize*sizeof(char));

  log_msg("\nNAS Read for Cache Write(buf=0x%08x, size=%d, offset=%lld, fi=0x%08x, nasFH = 0x % 016llx)\n",
  cacheBuf, nasReadSize, lowerOffset, fi, dualFH->nasFH);
  log_syscall("Reading for write to cache", pread(dualFH->nasFH, cacheBuf, nasReadSize, lowerOffset), 0);//do the possibly enlarged read from the NAS

  char cacheFileName[PATH_MAX];
  cfs_pathToFileName(cacheFileName, path);
  cfs_cacheWrite(cacheFileName, cacheBuf, alignedSize, lowerOffset, fi);// write our new data to cache for future reads 
  free((void*)cacheBuf);
  return retstat;
}

/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
int cfs_statfs(const char *path, struct statvfs *statv) {
  int retstat = 0;
  char nasPath[PATH_MAX];

  log_msg("\ncfs_statfs(path=\"%s\", statv=0x%08x)\n", path, statv);
  cfs_fullNasPath(nasPath, path);

  // get stats for underlying filesystem
  retstat = log_syscall("statvfs", statvfs(nasPath, statv), 0);

  log_statvfs(statv);

  return retstat;
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
// this is a no-op in CFSFS.  It just logs the call and returns success
int cfs_flush(const char *path, struct fuse_file_info *fi) {
  log_msg("\ncfs_flush(path=\"%s\", fi=0x%08x)\n", path, fi);
  // no need to get nasPath on this one, since I work from fi->fh not the path
  log_fi(fi);

  return 0;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int cfs_release(const char *path, struct fuse_file_info *fi) {
  struct dualFileHandle *dualFH;
  dualFH = (struct dualFileHandle *)fi->fh;
  log_msg("\ncfs_release(path=\"%s\", fi=0x%08x, nasFH=0x%016llx, cacheFH=0x%016llx)\n", path, fi,dualFH->nasFH,dualFH->cacheFH);
  log_fi(fi);

  int nasClose;
  // We need to close the file.  Had we allocated any resources
  // (buffers etc) we'd need to free them here as well.
  //Closing file in cache as well
  char cacheFileName[PATH_MAX], cachePath[PATH_MAX], fallocateCall[PATH_MAX];
  cfs_pathToFileName(cacheFileName, path);
  cfs_fullCachePath(cachePath, cacheFileName);

  strcpy(fallocateCall,"fallocate -d ");
  strncat(fallocateCall, cachePath, PATH_MAX);

  log_msg("\nFallocate call: %s\n", fallocateCall);

  log_syscall("Cache digging", system(fallocateCall), 0);
  log_syscall("Cache close", close(dualFH->cacheFH), 0);
  nasClose = log_syscall("NAS close", close(dualFH->nasFH), 0);
  free((void *)fi->fh);
  return nasClose;
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int cfs_fsync(const char *path, int datasync, struct fuse_file_info *fi) {
  struct dualFileHandle *dualFH;
  dualFH = (struct dualFileHandle *)fi->fh;
  log_msg("\ncfs_fsync(path=\"%s\", datasync=%d, fi=0x%08x,  nasFH=0x%016llx, cacheFH=0x%016llx)\n", path, datasync,
          fi, dualFH->nasFH,dualFH->cacheFH);
  log_fi(fi);

  // some unix-like systems (notably freebsd) don't have a datasync call
#ifdef HAVE_FDATASYNC
  if (datasync)
  {
    log_syscall("Cache fdatasync", fdatasync(dualFH->cacheFH), 0);
    return log_syscall("NAS fdatasync", fdatasync(dualFH->nasFH), 0);
  }
#endif
  log_syscall("Cache fsync", fsync(dualFH->cacheFH), 0);
  return log_syscall("NAS fsync", fsync(dualFH->nasFH), 0);
}

#ifdef HAVE_SYS_XATTR_H
/** Note that my implementations of the various xattr functions use
    the 'l-' versions of the functions (eg cfs_setxattr() calls
    lsetxattr() not setxattr(), etc).  This is because it appears any
    symbolic links are resolved before the actual call takes place, so
    I only need to use the system-provided calls that don't follow
    them */

/** Set extended attributes */
int cfs_setxattr(const char *path, const char *name, const char *value,
                size_t size, int flags) {
  char nasPath[PATH_MAX];

  log_msg("\ncfs_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, "
          "flags=0x%08x)\n",
          path, name, value, size, flags);
  cfs_fullNasPath(nasPath, path);

  return log_syscall("lsetxattr", lsetxattr(nasPath, name, value, size, flags),
                     0);
}

/** Get extended attributes */
int cfs_getxattr(const char *path, const char *name, char *value, size_t size) {
  int retstat = 0;
  char nasPath[PATH_MAX];

  log_msg("\ncfs_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, size = "
          "%d)\n",
          path, name, value, size);
  cfs_fullNasPath(nasPath, path);

  retstat = log_syscall("lgetxattr", lgetxattr(nasPath, name, value, size), 0);
  if (retstat >= 0)
    log_msg("    value = \"%s\"\n", value);

  return retstat;
}

/** List extended attributes */
int cfs_listxattr(const char *path, char *list, size_t size) {
  int retstat = 0;
  char nasPath[PATH_MAX];
  char *ptr;

  log_msg("\ncfs_listxattr(path=\"%s\", list=0x%08x, size=%d)\n", path, list,
          size);
  cfs_fullNasPath(nasPath, path);

  retstat = log_syscall("llistxattr", llistxattr(nasPath, list, size), 0);
  if (retstat >= 0) {
    log_msg("    returned attributes (length %d):\n", retstat);
    if (list != NULL)
      for (ptr = list; ptr < list + retstat; ptr += strlen(ptr) + 1)
        log_msg("    \"%s\"\n", ptr);
    else
      log_msg("    (null)\n");
  }

  return retstat;
}

/** Remove extended attributes */
int cfs_removexattr(const char *path, const char *name) {
  char nasPath[PATH_MAX];

  log_msg("\ncfs_removexattr(path=\"%s\", name=\"%s\")\n", path, name);
  cfs_fullNasPath(nasPath, path);

  return log_syscall("lremovexattr", lremovexattr(nasPath, name), 0);
}
#endif

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int cfs_opendir(const char *path, struct fuse_file_info *fi) {
  DIR *dp;
  int retstat = 0;
  char nasPath[PATH_MAX];

  log_msg("\ncfs_opendir(path=\"%s\", fi=0x%08x)\n", path, fi);
  cfs_fullNasPath(nasPath, path);

  // since opendir returns a pointer, takes some custom handling of
  // return status.
  dp = opendir(nasPath);
  log_msg("    opendir returned 0x%p\n", dp);
  if (dp == NULL)
    retstat = log_error("cfs_opendir opendir");

  fi->fh = (intptr_t)dp;

  log_fi(fi);

  return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */

int cfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi) {
  int retstat = 0;
  DIR *dp;
  struct dirent *de;

  log_msg("\ncfs_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, "
          "fi=0x%08x)\n",
          path, buf, filler, offset, fi);
  // once again, no need for fullNasPath -- but note that I need to cast fi->fh
  dp = (DIR *)(uintptr_t)fi->fh;

  // Every directory contains at least two entries: . and ..  If my
  // first call to the system readdir() returns NULL I've got an
  // error; near as I can tell, that's the only condition under
  // which I can get an error from readdir()
  de = readdir(dp);
  log_msg("    readdir returned 0x%p\n", de);
  if (de == 0) {
    retstat = log_error("cfs_readdir readdir");
    return retstat;
  }

  // This will copy the entire directory into the buffer.  The loop exits
  // when either the system readdir() returns NULL, or filler()
  // returns something non-zero.  The first case just means I've
  // read the whole directory; the second means the buffer is full.
  do {
    log_msg("calling filler with name %s\n", de->d_name);
    if (filler(buf, de->d_name, NULL, 0) != 0) {
      log_msg("    ERROR cfs_readdir filler:  buffer full");
      return -ENOMEM;
    }
  } while ((de = readdir(dp)) != NULL);

  log_fi(fi);

  return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int cfs_releasedir(const char *path, struct fuse_file_info *fi) {
  int retstat = 0;

  log_msg("\ncfs_releasedir(path=\"%s\", fi=0x%08x)\n", path, fi);
  log_fi(fi);

  closedir((DIR *)(uintptr_t)fi->fh);

  return retstat;
}

/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
// when exactly is this called?  when a user calls fsync and it
// happens to be a directory? ??? >>> I need to implement this...
int cfs_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi) {
  int retstat = 0;

  log_msg("\ncfs_fsyncdir(path=\"%s\", datasync=%d, fi=0x%08x)\n", path,
          datasync, fi);
  log_fi(fi);

  return retstat;
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
// Undocumented but extraordinarily useful fact:  the fuse_context is
// set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to
// fuse_main().  Really seems like either it should be a third
// parameter coming in here, or else the fact should be documented
// (and this might as well return void, as it did in older versions of
// FUSE).
void *cfs_init(struct fuse_conn_info *conn) {
  log_msg("\ncfs_init()\n");

  log_conn(conn);
  log_fuse_context(fuse_get_context());

  return CFS_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void cfs_destroy(void *userdata) {
  log_msg("\ncfs_destroy(userdata=0x%08x)\n", userdata);
}

/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int cfs_access(const char *path, int mask) {
  int retstat = 0;
  char nasPath[PATH_MAX];

  log_msg("\ncfs_access(path=\"%s\", mask=0%o)\n", path, mask);
  cfs_fullNasPath(nasPath, path);

  retstat = access(nasPath, mask);

  if (retstat < 0)
    retstat = log_error("cfs_access access");

  return retstat;
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
// Not implemented.  I had a version that used creat() to create and
// open the file, which it turned out opened the file write-only.

/**
 * Change the size of an open file
 *
 * This method is called instead of the truncate() method if the
 * truncation was invoked from an ftruncate() system call.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the truncate() method will be
 * called instead.
 *
 * Introduced in version 2.5
 */
int cfs_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi) {
  int retstat = 0;

  struct dualFileHandle *dualFH;
  dualFH = (struct dualFileHandle *)fi->fh;
  log_msg("\ncfs_ftruncate(path=\"%s\", offset=%lld, fi=0x%08x, nasFH=0x%016llx, cacheFH=0x%016llx)\n", path, offset,
          fi, dualFH->nasFH,dualFH->cacheFH);
  log_fi(fi);

  retstat = ftruncate(dualFH->cacheFH, offset);
  if (retstat < 0)
    retstat = log_error("cfs_ftruncate Cache ftruncate");
  retstat = ftruncate(dualFH->nasFH, offset);
  if (retstat < 0)
    retstat = log_error("cfs_ftruncate NAS ftruncate");

  return retstat;
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
int cfs_fgetattr(const char *path, struct stat *statbuf,
                struct fuse_file_info *fi) {
  int retstat = 0;

  log_msg("\ncfs_fgetattr(path=\"%s\", statbuf=0x%08x, fi=0x%08x)\n", path,
          statbuf, fi);
  log_fi(fi);

  // On FreeBSD, trying to do anything with the mountpoint ends up
  // opening it, and then using the FD for an fgetattr.  So in the
  // special case of a path of "/", I need to do a getattr on the
  // underlying root directory instead of doing the fgetattr().
  if (!strcmp(path, "/"))
    return cfs_getNASattr(path, statbuf);

  struct dualFileHandle *dualFH;
  dualFH = (struct dualFileHandle *)fi->fh;
  retstat = fstat(dualFH->nasFH, statbuf);
  if (retstat < 0)
    retstat = log_error("cfs_fgetattr fstat");

  log_stat(statbuf);

  return retstat;
}

struct fuse_operations cfs_oper = {.getattr = cfs_getNASattr,
                                  .readlink = cfs_readlink,
                                  // no .getdir -- that's deprecated
                                  .getdir = NULL,
                                  .mknod = cfs_mknod,
                                  .mkdir = cfs_mkdir,
                                  .unlink = cfs_unlink,
                                  .rmdir = cfs_rmdir,
                                  .symlink = cfs_symlink,
                                  .rename = cfs_rename,
                                  .link = cfs_link,
                                  .chmod = cfs_chmod,
                                  .chown = cfs_chown,
                                  .truncate = cfs_truncate,
                                  .utime = cfs_utime,
                                  .open = cfs_open,
                                  .read = cfs_read,
                                  .write = cfs_write,
                                  /** Just a placeholder, don't set */ // huh???
                                  .statfs = cfs_statfs,
                                  .flush = cfs_flush,
                                  .release = cfs_release,
                                  .fsync = cfs_fsync,

#ifdef HAVE_SYS_XATTR_H
                                  .setxattr = cfs_setxattr,
                                  .getxattr = cfs_getxattr,
                                  .listxattr = cfs_listxattr,
                                  .removexattr = cfs_removexattr,
#endif

                                  .opendir = cfs_opendir,
                                  .readdir = cfs_readdir,
                                  .releasedir = cfs_releasedir,
                                  .fsyncdir = cfs_fsyncdir,
                                  .init = cfs_init,
                                  .destroy = cfs_destroy,
                                  .access = cfs_access,
                                  .ftruncate = cfs_ftruncate,
                                  .fgetattr = cfs_fgetattr};

void cfs_usage() {
  fprintf(stderr, "usage:  cachesize blocksize nasDir mountDir cacheDir\n");
  abort();
}

int main(int argc, char *argv[]) {
  int fuse_stat;
  struct cfs_state *cfs_data;

  // cfsfs doesn't do any access checking on its own (the comment
  // blocks in fuse.h mention some of the functions that need
  // accesses checked -- but note there are other functions, like
  // chown(), that also need checking!).  Since running cfsfs as root
  // will therefore open Metrodome-sized holes in the system
  // security, we'll check if root is trying to mount the filesystem
  // and refuse if it is.  The somewhat smaller hole of an ordinary
  // user doing it with the allow_other flag is still there because
  // I don't want to parse the options string.
  if ((getuid() == 0) || (geteuid() == 0)) {
    fprintf(stderr,
            "Running CFSFS as root opens unnacceptable security holes\n");
    return 1;
  }

  // See which version of fuse we're running
  fprintf(stderr, "Fuse library version %d.%d\n", FUSE_MAJOR_VERSION,
          FUSE_MINOR_VERSION);

  // Perform some sanity checking on the command line:  make sure
  // there are enough arguments, and that neither of the last two
  // start with a hyphen (this will break if you actually have a
  // rootpoint or mountpoint whose name starts with a hyphen, but so
  // will a zillion other programs)
  if ((argc < 6)  || (argv[argc - 3][0] == '-')|| (argv[argc - 2][0] == '-') || (argv[argc - 1][0] == '-'))
    cfs_usage();

  cfs_data = malloc(sizeof(struct cfs_state));
  if (cfs_data == NULL) {
    perror("main calloc");
    abort();
  }

  //cache size supplied as argv[argc-5], block size supplied as argv[argc-4]
  cfs_data->nasdir = realpath(argv[argc - 3], NULL);//save our nas and cachefs directory paths early on
  cfs_data->cachedir = realpath(argv[argc - 1], NULL);

  //cache sizing
  size_t userSize;
  sscanf(argv[argc-5], "%lu", &userSize);//set our cache size in Kb
  fprintf(stderr,"Supplied cache size: %lu\n", userSize);
  struct statvfs *fsBuffer = malloc(sizeof(struct statvfs));
  int statvfsRet = statvfs(cfs_data->cachedir, fsBuffer);
  if(statvfsRet != 0)//error in statvfs
  {
    fprintf(stderr,"Warning, available cache space not found. Supplied size may be too large.\n");
    cache_size = userSize;
  }
  else//check if available is greater than supplied, inform user
  {
    fprintf(stderr,"File system block size: %lu\n", fsBuffer->f_bsize);
    fprintf(stderr,"File system available block count: %lu\n", fsBuffer->f_bavail);

    double KBperBlock = ((double)(fsBuffer->f_bsize)/1024.0);
    long unsigned fsAvailKb = (long unsigned)(KBperBlock*fsBuffer->f_bavail);
    fprintf(stderr,"File system available cache space (Kb): %lu\n", fsAvailKb);

    if(userSize > fsAvailKb-100 && fsAvailKb > 100)//allow 100 kB buffer space
    {
      fprintf(stderr,"Supplied cache size is too large. Cache size will be set to: %lu\n", fsAvailKb-100);
      cache_size = fsAvailKb-100;
    }
    else if(userSize > fsAvailKb)//available space is not large enough for buffer space
    {
      fprintf(stderr,"Supplied cache size is too large. Cache size will be set to: %lu\n", fsAvailKb);
      cache_size = fsAvailKb;
    }
    else//user supplied is fine
    {
      fprintf(stderr,"Supplied cache size is accepted as it is below available space.\n");
      cache_size = userSize;
    }
  }

  sscanf(argv[argc-4], "%lu", &block_size);//set our block size
  argv[argc - 5] = argv[argc - 2];//put mountdir in first non null argv entry
  argv[argc - 4] = NULL;
  argv[argc - 3] = NULL;
  argv[argc - 2] = NULL;
  argv[argc - 1] = NULL;
  argc = 2;
  fprintf(stderr, "Nas Path %s\n", cfs_data->nasdir);
  fprintf(stderr, "Cache Path %s\n", cfs_data->cachedir);
  fprintf(stderr, "New cache size (Kb): %lu\n", cache_size);
  fprintf(stderr, "New block size (Bytes): %lu\n", block_size);
  // Pull the rootdir out of the argument list and save it in my
  // internal data


  cfs_data->logfile = log_open();

  /*-------------------Open Metadata Handle-------------------*/
  if (VERBOSE)
  {
    printf("Starting Metadata Initialization...\n");
  }
  char metadata_file[PATH_MAX]; 

  printf("Hello!\n");

  strcpy(metadata_file, cfs_data->cachedir);
  strncat(metadata_file, "/Metadata-File.db", PATH_MAX);

  if (VERBOSE)
  {
    printf("Metadata File Name: %s\n", metadata_file);
  }  

  open_db(metadata_file, &metaDataBase);
  int ret = create_tables(metaDataBase);
  if((VERBOSE)&&(ret ==-1)){
    printf("Tables probably exist already!\n");
  }

  set_block_size(block_size);

  // function to init cache_used_size 
  if (VERBOSE)
  {
    printf("\n");
  }
  init_cache_used_size(metaDataBase);
  if (VERBOSE)
  {
    print_cache_used_size();
  }

  /*-------------------Open Metadata Handle-------------------*/

  // turn over control to fuse
  fprintf(stderr, "about to call fuse_main\n");
  fuse_stat = fuse_main(argc, argv, &cfs_oper, cfs_data);
  fprintf(stderr, "fuse_main returned %d\n", fuse_stat);

  return fuse_stat;
}
