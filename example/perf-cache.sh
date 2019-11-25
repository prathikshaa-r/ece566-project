#!/bin/bash
echo "CACHEDIR"
for i in {1..10};
do echo $i; sudo -u pr109 ./iozone3_487/src/current/iozone -f mountdir/PERF-TEST –azcR –b cache-out-$i.xls > tests/cache-out-$i; sync; echo 1 > /proc/sys/vm/drop_caches; sync; echo 2 > /proc/sys/vm/drop_caches;
   done;
