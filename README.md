# Lufus — Rufus for Linux (stable v1.0.0)

Fork of [pbatard/rufus](https://github.com/pbatard/rufus) (GPLv3), ported to Linux.

Bootable USB creation, ISO/DD writing, partitioning, filesystems, checksums,
persistence, Secure Boot validation — CLI + GTK4 GUI.

## Install

Arch (AUR): see `packaging/PKGBUILD`. Flatpak: `packaging/io.github.hultwl.lufus.json`.
From source (`packaging/README.md`):

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
sudo cmake --install build
lufus --gui
```

## Status: stable v1.0.0 — see PHASES.md + CHANGELOG.md

Quick check: `./build/lufus list`, `./build/lufus probe file.iso --detail`,
`./build/lufus --gui`. Destructive commands default to `--dry-run`;
real block writes need `--real --yes` + root. Details in `PORTING.md`.

## Origin

All Windows-only code is original Rufus © Pete Batard, GPLv3. See `LICENSE.txt`.
Lufus port work is also GPLv3.
