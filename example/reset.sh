#!/bin/bash
sudo -u pr109 fusermount -u mountdir
umount -l mountdir
umount -l cachedir
rm -f cacheFS.img
