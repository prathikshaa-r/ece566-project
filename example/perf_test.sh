#!/bin/bash
echo "NASDIR"
for i in {1..10};
do echo $i; sudo -u pr109 ./iozone3_487/src/current/iozone -f nasdir/PERF-TEST –azcR –b nas-out-$i.xls > tests/nas-out-$i; sync; echo 1 > /proc/sys/vm/drop_caches; sync; echo 2 > /proc/sys/vm/drop_caches;
   done;

echo "CACHEDIR"
for i in {1..10};
do echo $i; sudo -u pr109 ./iozone3_487/src/current/iozone -f mountdir/PERF-TEST-CACHE –azcR –b cache-out-$i.xls > tests/cache-out-$i; sync;
   echo 1 > /proc/sys/vm/drop_caches; sync; echo 2 > /proc/sys/vm/drop_caches;
   done;

echo "Tweakfs"
for i in {1..10};
do echo $i; sudo -u pr109 ./iozone3_487/src/current/iozone -f ../../tweakfs/example/mountdir/PERF-TEST-TWEAK –azcR –b tweakfs-out-$i.xls > tests/tweakfs-out-$i; sync;
   echo 1 > /proc/sys/vm/drop_caches; sync; echo 2 > /proc/sys/vm/drop_caches;
   done;
