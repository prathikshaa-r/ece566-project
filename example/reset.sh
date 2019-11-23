#!/bin/bash
sudo -u wjb24 fusermount -u mountdir
umount -l mountdir
umount -l cachedir
rm -f cacheFS.img