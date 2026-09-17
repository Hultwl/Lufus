<p align="center">
  <img src="https://raw.githubusercontent.com/Hultwl/Rufux/main/res/icons/rufus-128.png" width="128" alt="Rufux logo">
</p>

<h1 align="center">Rufux</h1>

<p align="center">
  <strong>Rufus, natively on Linux.</strong> Bootable USBs, ISO/DD imaging,
  partitioning and validation — CLI + GTK4 GUI.
</p>

<p align="center">
  <a href="https://github.com/Hultwl/Rufux/releases/latest"><img src="https://img.shields.io/github/v/release/Hultwl/Rufux?style=flat-square&label=release" alt="Latest release"></a>
  <a href="https://github.com/Hultwl/Rufux/actions/workflows/rufux.yml"><img src="https://github.com/Hultwl/Rufux/actions/workflows/rufux.yml/badge.svg" alt="CI build"></a>
  <a href="https://aur.archlinux.org/packages/rufux-git"><img src="https://img.shields.io/aur/version/rufux-git?style=flat-square&label=AUR" alt="AUR version"></a>
  <a href="https://github.com/Hultwl/Rufux/blob/main/LICENSE.txt"><img src="https://img.shields.io/github/license/Hultwl/Rufux?style=flat-square" alt="License: GPLv3"></a>
  <img src="https://img.shields.io/github/languages/top/Hultwl/Rufux?style=flat-square" alt="Top language: C">
</p>

<p align="center">
  <a href="https://github.com/Hultwl/Rufux/releases/latest/download/Rufux-x86_64.AppImage"><strong>⬇ Download AppImage</strong></a>
</p>

<p align="center">
  <img src="screenshots/rufux-main.png" width="520" alt="Rufux main window">
</p>

---

## ✨ Features

| Area | What you get |
|---|---|
| 🔥 Burn | DD image mode (verified writes) and ISO file mode (partition → format → extract → bootloader, auto-mounted via udisks2) |
| 💾 Layouts | GPT/MBR, FAT32 / NTFS / exFAT / UDF / ext4, cluster-size control, volume labels taken from the image |
| 🖥 GUI | Rufus-style dialog: drive properties, boot selection, persistence (casper-rw), target-system lock, bad-block passes, SHA-256 checker, timestamped log with save |
| 📊 Progress | Real staged percent end-to-end, speed + ETA in the CLI, live percent in the GUI |
| 🛡 Safety | Everything dry-runs by default; real block writes need `--real --yes` + root; fixed disks, mounted targets and source == target refused |
| ✅ Validation | Secure Boot status, EFI bootloader checks, byte-compare verify pass |
| 🌍 i18n | gettext infrastructure (FR/ES shipped) + dark-theme aware |

## 📦 Install

**AppImage** (easiest — portable, self-updating via `update-check`):
```sh
chmod +x Rufux-x86_64.AppImage
./Rufux-x86_64.AppImage   # opens the GUI
```

**Arch Linux (AUR, tracks git):**
```sh
paru -S rufux-git
```

**From source:**
```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build   # 3/3 green
sudo cmake --install build
```

Needs on the host for formatting work: `dosfstools`, `ntfsprogs`, `exfatprogs`,
`e2fsprogs`, `util-linux`, `syslinux`, `udisks2`, `libarchive` — see
[`packaging/README.md`](packaging/README.md).

## 🚀 Quick start

```sh
rufux list                                  # removable drives
rufux probe image.iso --detail              # label, size, bootable?
rufux write image.iso /dev/sdX --dry-run    # always plan first…
sudo rufux write image.iso /dev/sdX --real --verify --yes
rufux --gui                                 # full GUI (escalates per action)
```

## 📚 Docs

- [`PORTING.md`](PORTING.md) — what was kept vs rewritten from Rufus
- [`PHASES.md`](PHASES.md) — the road to 1.0
- [`CHANGELOG.md`](CHANGELOG.md) — release history
- [`tests/HW_MATRIX.md`](tests/HW_MATRIX.md) — hardware validation checklist
- `man rufux` after install

## 🤝 Contributing

Bug reports with log output are gold (`Save` button in the GUI).
Test-driven fixes welcome: `tests/test_phase*.sh` must stay green —
see [`tests/HW_MATRIX.md`](tests/HW_MATRIX.md) before touching real hardware.

## 🙏 Origin

Port of [pbatard/rufus](https://github.com/pbatard/rufus) (© Pete Batard,
GPLv3 — upstream sources kept in-tree for reference). Renamed from Lufus
to Rufux to avoid colliding with [Hogjects/Lufus](https://github.com/Hogjects/Lufus)
— different project, much respect.
