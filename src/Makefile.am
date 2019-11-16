bin_PROGRAMS = cachefs
cachefs_SOURCES = cachefs.c log.c log.h params.h cacheHelp.c cacheHelp.h metadata/meta.h metadata/meta.c
AM_CFLAGS = @FUSE_CFLAGS@
LDADD = @FUSE_LIBS@ -lsqlite3
