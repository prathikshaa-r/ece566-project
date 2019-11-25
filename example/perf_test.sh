#!/bin/bash
echo "NASDIR"
for i in {1..10};
do echo $i; sudo -u wjb24 ../../iozone3_487/src/current/iozone -f nasdir/PERF-TEST –azcR –b nas-out-$i.xls > tests/nas-out-$i; sync; echo 1 > /proc/sys/vm/drop_caches; sync; echo 2 > /proc/sys/vm/drop_caches;
   done;

echo "CACHEDIR"
for i in {1..10};
do echo $i; ../../iozone3_487/src/current/iozone -f mountdir/PERF-TEST-$i –azcR –b cache-out-$i.xls > tests/cache-out-$i; sync;
   echo 1 > /proc/sys/vm/drop_caches; sync; echo 2 > /proc/sys/vm/drop_caches;
   done;
