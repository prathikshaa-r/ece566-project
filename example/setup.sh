#!/bin/bash
dd if=/dev/zero of=cacheFS.img bs=5M count=1
mkfs.ext4 -b 1024 cacheFS.img 
mount cacheFS.img ./cachedir 
chown pr109:pr109 cachedir
./run_cachefs.sh
