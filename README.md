# Rufux — Rufus for Linux (stable 1.1.1)

Fork of [pbatard/rufus](https://github.com/pbatard/rufus) (GPLv3), ported to Linux.

Bootable USB creation, ISO/DD writing, partitioning, filesystems, checksums,
persistence, Secure Boot validation — CLI + GTK4 GUI.

## Install

Arch (AUR): see `packaging/PKGBUILD`. Flatpak: `packaging/io.github.hultwl.rufux.json`.
From source (`packaging/README.md`):

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
sudo cmake --install build
rufux --gui
```

## Status: stable 1.1.1 — see PHASES.md + CHANGELOG.md

Quick check: `./build/rufux list`, `./build/rufux probe file.iso --detail`,
`./build/rufux --gui`. Destructive commands default to `--dry-run`;
real block writes need `--real --yes` + root. Details in `PORTING.md`.

## Origin

All Windows-only code is original Rufus © Pete Batard, GPLv3. See `LICENSE.txt`.
Rufux port work is also GPLv3.
