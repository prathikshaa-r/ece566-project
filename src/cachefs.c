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

#include "log.h"
#include "cacheHelp.h"


//  All the paths I see are relative to the root of the mounted
//  filesystem.  In order to get to the underlying filesystem, I need to
//  have the mountpoint.  I'll save it away early on in main(), and then
//  whenever I need a path for something I'll call this to construct
//  it.
static void cfs_fullpath(char fpath[PATH_MAX], const char *path) {
  strcpy(fpath, CFS_DATA->nasdir);
  strncat(fpath, path, PATH_MAX); // ridiculously long paths will
                                  // break here

  log_msg("    cfs_fullpath:  nasdir = \"%s\", path = \"%s\", fpath = \"%s\"\n",
          CFS_DATA->nasdir, path, fpath);
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
int cfs_getattr(const char *path, struct stat *statbuf) {
  int retstat = 0;
  char fpath[PATH_MAX];

  log_msg("\ncfs_getattr(path=\"%s\", statbuf=0x%08x)\n", path, statbuf);
  cfs_fullpath(fpath, path);

  retstat = log_syscall("lstat", lstat(fpath, statbuf), 0);

  log_stat(statbuf);

  return retstat;
}

time_t getTimestamp(const char *path)
{
  struct stat *statbuf = malloc(sizeof(struct stat));
  int retstat = 0;
  char fpath[PATH_MAX];

  log_msg("\ncfs_getTimeStamp(path=\"%s\", statbuf=0x%08x)\n", path, statbuf);
  cfs_fullpath(fpath, path);

  retstat = log_syscall("lstat", lstat(fpath, statbuf), 0);

  log_stat(statbuf);
  
  return statbuf->st_mtime;
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
  char fpath[PATH_MAX];

  log_msg("\ncfs_readlink(path=\"%s\", link=\"%s\", size=%d)\n", path, link,
          size);
  cfs_fullpath(fpath, path);

  retstat = log_syscall("readlink", readlink(fpath, link, size - 1), 0);
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
  char fpath[PATH_MAX];

  log_msg("\ncfs_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n", path, mode, dev);
  cfs_fullpath(fpath, path);

  // On Linux this could just be 'mknod(path, mode, dev)' but this
  // tries to be be more portable by honoring the quote in the Linux
  // mknod man page stating the only portable use of mknod() is to
  // make a fifo, but saying it should never actually be used for
  // that.
  if (S_ISREG(mode)) {
    retstat =
        log_syscall("open", open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode), 0);
    if (retstat >= 0)
      retstat = log_syscall("close", close(retstat), 0);
  } else if (S_ISFIFO(mode))
    retstat = log_syscall("mkfifo", mkfifo(fpath, mode), 0);
  else
    retstat = log_syscall("mknod", mknod(fpath, mode, dev), 0);

  return retstat;
}

/** Create a directory */
int cfs_mkdir(const char *path, mode_t mode) {
  char fpath[PATH_MAX];

  log_msg("\ncfs_mkdir(path=\"%s\", mode=0%3o)\n", path, mode);
  cfs_fullpath(fpath, path);

  return log_syscall("mkdir", mkdir(fpath, mode), 0);
}

/** Remove a file */
int cfs_unlink(const char *path) {
  char fpath[PATH_MAX];

  log_msg("cfs_unlink(path=\"%s\")\n", path);
  cfs_fullpath(fpath, path);

  return log_syscall("unlink", unlink(fpath), 0);
}

/** Remove a directory */
int cfs_rmdir(const char *path) {
  char fpath[PATH_MAX];

  log_msg("cfs_rmdir(path=\"%s\")\n", path);
  cfs_fullpath(fpath, path);

  return log_syscall("rmdir", rmdir(fpath), 0);
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int cfs_symlink(const char *path, const char *link) {
  char flink[PATH_MAX];

  log_msg("\ncfs_symlink(path=\"%s\", link=\"%s\")\n", path, link);
  cfs_fullpath(flink, link);

  return log_syscall("symlink", symlink(path, flink), 0);
}

/** Rename a file */
// both path and newpath are fs-relative
int cfs_rename(const char *path, const char *newpath) {
  char fpath[PATH_MAX];
  char fnewpath[PATH_MAX];

  log_msg("\ncfs_rename(fpath=\"%s\", newpath=\"%s\")\n", path, newpath);
  cfs_fullpath(fpath, path);
  cfs_fullpath(fnewpath, newpath);

  return log_syscall("rename", rename(fpath, fnewpath), 0);
}

/** Create a hard link to a file */
int cfs_link(const char *path, const char *newpath) {
  char fpath[PATH_MAX], fnewpath[PATH_MAX];

  log_msg("\ncfs_link(path=\"%s\", newpath=\"%s\")\n", path, newpath);
  cfs_fullpath(fpath, path);
  cfs_fullpath(fnewpath, newpath);

  return log_syscall("link", link(fpath, fnewpath), 0);
}

/** Change the permission bits of a file */
int cfs_chmod(const char *path, mode_t mode) {
  char fpath[PATH_MAX];

  log_msg("\ncfs_chmod(fpath=\"%s\", mode=0%03o)\n", path, mode);
  cfs_fullpath(fpath, path);

  return log_syscall("chmod", chmod(fpath, mode), 0);
}

/** Change the owner and group of a file */
int cfs_chown(const char *path, uid_t uid, gid_t gid)

{
  char fpath[PATH_MAX];

  log_msg("\ncfs_chown(path=\"%s\", uid=%d, gid=%d)\n", path, uid, gid);
  cfs_fullpath(fpath, path);

  return log_syscall("chown", chown(fpath, uid, gid), 0);
}

/** Change the size of a file */
int cfs_truncate(const char *path, off_t newsize) {
  char fpath[PATH_MAX];

  log_msg("\ncfs_truncate(path=\"%s\", newsize=%lld)\n", path, newsize);
  cfs_fullpath(fpath, path);

  return log_syscall("truncate", truncate(fpath, newsize), 0);
}

/** Change the access and/or modification times of a file */
/* note -- I'll want to change this as soon as 2.6 is in debian testing */
int cfs_utime(const char *path, struct utimbuf *ubuf) {
  char fpath[PATH_MAX];

  log_msg("\ncfs_utime(path=\"%s\", ubuf=0x%08x)\n", path, ubuf);
  cfs_fullpath(fpath, path);

  return log_syscall("utime", utime(fpath, ubuf), 0);
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
  int fd;
  char fpath[PATH_MAX];

  log_msg("\ncfs_open(path\"%s\", fi=0x%08x)\n", path, fi);
  cfs_fullpath(fpath, path);

  // if the open call succeeds, my retstat is the file descriptor,
  // else it's -errno.  I'm making sure that in that case the saved
  // file descriptor is exactly -1.
  fd = log_syscall("open", open(fpath, fi->flags), 0);
  time_t nasTimestamp = getTimestamp(fpath);
  /*
  if(presentInCache)//open cache file 
  {
    open(fpath, fi->flags)
    time_t cacheTimestamp = getTimestamp(cache path);
  }
  */

  if (fd < 0)
    retstat = log_error("open");

  fi->fh = fd;

  log_fi(fi);

  return retstat;
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

  log_msg("entering read\n");
  off_t lowerOffset = alignLowerOffset(offset);//adapt offset to start at a block offset
  off_t upperOffset = alignUpperOffset(offset+size);//adapt read to end at a block offset for caching whole blocks
  size_t alignedSize = upperOffset - lowerOffset;//our new size from a block start offset to a block end offset  

  char* cacheBuf = malloc(alignedSize*sizeof(char));

  log_msg(
      "\ncfs_read original(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
      path, buf, size, offset, fi);

  log_msg(
      "\ncfs_read aligned(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
      path, cacheBuf, alignedSize, lowerOffset, fi);
  // no need to get fpath on this one, since I work from fi->fh not the path
  log_fi(fi);

  retstat = log_syscall("pread", pread(fi->fh, cacheBuf, alignedSize, lowerOffset), 0);//do the possibly enlarged read from the NAS
  memcpy(buf, cacheBuf+offset-lowerOffset, size);//write to buffer the requested data (offset-lowerOffset will bump up cacheBuf to where the real data starts)

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

  off_t lowerOffset = alignLowerOffset(offset);//adapt offset to start at a block offset
  off_t upperOffset = alignUpperOffset(offset+size);//adapt read to end at a block offset for caching whole blocks
  size_t alignedSize = upperOffset - lowerOffset;//our new size from a block start offset to a block end offset  

  log_msg(
      "\ncfs_write original(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
      path, buf, size, offset, fi);

  log_fi(fi);

  return log_syscall("pwrite", pwrite(fi->fh, buf, size, offset), 0);
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
  char fpath[PATH_MAX];

  log_msg("\ncfs_statfs(path=\"%s\", statv=0x%08x)\n", path, statv);
  cfs_fullpath(fpath, path);

  // get stats for underlying filesystem
  retstat = log_syscall("statvfs", statvfs(fpath, statv), 0);

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
  // no need to get fpath on this one, since I work from fi->fh not the path
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
  log_msg("\ncfs_release(path=\"%s\", fi=0x%08x)\n", path, fi);
  log_fi(fi);

  // We need to close the file.  Had we allocated any resources
  // (buffers etc) we'd need to free them here as well.
  return log_syscall("close", close(fi->fh), 0);
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int cfs_fsync(const char *path, int datasync, struct fuse_file_info *fi) {
  log_msg("\ncfs_fsync(path=\"%s\", datasync=%d, fi=0x%08x)\n", path, datasync,
          fi);
  log_fi(fi);

  // some unix-like systems (notably freebsd) don't have a datasync call
#ifdef HAVE_FDATASYNC
  if (datasync)
    return log_syscall("fdatasync", fdatasync(fi->fh), 0);
  else
#endif
    return log_syscall("fsync", fsync(fi->fh), 0);
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
  char fpath[PATH_MAX];

  log_msg("\ncfs_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, "
          "flags=0x%08x)\n",
          path, name, value, size, flags);
  cfs_fullpath(fpath, path);

  return log_syscall("lsetxattr", lsetxattr(fpath, name, value, size, flags),
                     0);
}

/** Get extended attributes */
int cfs_getxattr(const char *path, const char *name, char *value, size_t size) {
  int retstat = 0;
  char fpath[PATH_MAX];

  log_msg("\ncfs_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, size = "
          "%d)\n",
          path, name, value, size);
  cfs_fullpath(fpath, path);

  retstat = log_syscall("lgetxattr", lgetxattr(fpath, name, value, size), 0);
  if (retstat >= 0)
    log_msg("    value = \"%s\"\n", value);

  return retstat;
}

/** List extended attributes */
int cfs_listxattr(const char *path, char *list, size_t size) {
  int retstat = 0;
  char fpath[PATH_MAX];
  char *ptr;

  log_msg("\ncfs_listxattr(path=\"%s\", list=0x%08x, size=%d)\n", path, list,
          size);
  cfs_fullpath(fpath, path);

  retstat = log_syscall("llistxattr", llistxattr(fpath, list, size), 0);
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
  char fpath[PATH_MAX];

  log_msg("\ncfs_removexattr(path=\"%s\", name=\"%s\")\n", path, name);
  cfs_fullpath(fpath, path);

  return log_syscall("lremovexattr", lremovexattr(fpath, name), 0);
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
  char fpath[PATH_MAX];

  log_msg("\ncfs_opendir(path=\"%s\", fi=0x%08x)\n", path, fi);
  cfs_fullpath(fpath, path);

  // since opendir returns a pointer, takes some custom handling of
  // return status.
  dp = opendir(fpath);
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
  // once again, no need for fullpath -- but note that I need to cast fi->fh
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
  char fpath[PATH_MAX];

  log_msg("\ncfs_access(path=\"%s\", mask=0%o)\n", path, mask);
  cfs_fullpath(fpath, path);

  retstat = access(fpath, mask);

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

  log_msg("\ncfs_ftruncate(path=\"%s\", offset=%lld, fi=0x%08x)\n", path, offset,
          fi);
  log_fi(fi);

  retstat = ftruncate(fi->fh, offset);
  if (retstat < 0)
    retstat = log_error("cfs_ftruncate ftruncate");

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
    return cfs_getattr(path, statbuf);

  retstat = fstat(fi->fh, statbuf);
  if (retstat < 0)
    retstat = log_error("cfs_fgetattr fstat");

  log_stat(statbuf);

  return retstat;
}

struct fuse_operations cfs_oper = {.getattr = cfs_getattr,
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
  if ((argc < 5)  || (argv[argc - 3][0] == '-')|| (argv[argc - 2][0] == '-') || (argv[argc - 1][0] == '-'))
    cfs_usage();

  cfs_data = malloc(sizeof(struct cfs_state));
  if (cfs_data == NULL) {
    perror("main calloc");
    abort();
  }

  //cache size supplied as argv[argc-5], block size supplied as argv[argc-4]
  cfs_data->nasdir = realpath(argv[argc - 3], NULL);//save our nas and cachefs directory paths early on
  cfs_data->cachedir = realpath(argv[argc - 1], NULL);
  sscanf(argv[argc-5], "%lu", &cache_size);//set our cache size in Kb
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

  // turn over control to fuse
  fprintf(stderr, "about to call fuse_main\n");
  fuse_stat = fuse_main(argc, argv, &cfs_oper, cfs_data);
  fprintf(stderr, "fuse_main returned %d\n", fuse_stat);

  return fuse_stat;
}
