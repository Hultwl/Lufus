# Lufus — Rufus for Linux

Fork of [pbatard/rufus](https://github.com/pbatard/rufus) (GPLv3), ported to Linux.

Goal: full-featured Linux-native port, including GUI — bootable USB creation, ISO/DD writing, partitioning, filesystems, checksums, persistence.

## Status: Phase 2 done (v0.3.0) — see PHASES.md

Phase 2 delivers bootable parity: `partition`, `format`,
`extract`, `install-boot`, `persist`, `badblocks`, `create --mode dd|extract`,
plus GUI mode/scheme/fs. Phase 3 will be the stable v1.0 release.

## Build (Linux scaffold)

```sh
sudo apt install cmake gcc pkg-config libgtk-4-dev libudev-dev libblkid-dev
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/lufus --help
```

The scaffold currently enumerates USB block devices via sysfs/udev, probes ISO files, and opens a GTK4 window. Windows backends (`dev.c`, `drive.c`, `format.c`, Win32 UI) are NOT yet replaced — see `PORTING.md`.

## Origin

All Windows-only code is original Rufus © Pete Batard, GPLv3. See `LICENSE.txt`.
Lufus port work is also GPLv3.
