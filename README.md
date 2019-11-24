# CacheFS - Based on BBFS using FUSE (Filesystem in USErspace)

## Install Dependencies:
```
sudo apt-get install build-essential pkg-config autoconf libfuse-dev sqlite3 libsqlite3-dev
```
## Details
* Uses sqlite3 for out-of-memory metadata tracking for the local cache of the NAS filesystem.
* Due to some of the specific functions/features used by this application such as `fallocate`, this program is currently compatible with only Linux based filesystems such as ext3/4 for the local cache. The idea could be extended to other types of filesystems.
* This program still leaves a strong coupling to the NAS FS as any calls except straight-forward reads/writes are synchronously forwarded to the NAS.
* Performace testing was conduted using IOZone Filesystem Benchmarks and Network Simulator for poor network conditions.
* The baseline for comparison of performance is the performance of the NAS on similar network conditions.
