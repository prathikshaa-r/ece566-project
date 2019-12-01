#!/bin/bash
dd if=/dev/zero of=cacheFS.img bs=50M count=1
mkfs.ext4 -b 1024 cacheFS.img 
mount -o sync cacheFS.img ./cachedir 
chown wjb24:wjb24 cachedir
./run_cachefs.sh
