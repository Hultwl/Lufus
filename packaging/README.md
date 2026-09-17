# Packaging Rufux

## AppImage (recommended portable build)

Attached to every GitHub release, built by
`.github/workflows/appimage.yml` on Ubuntu 22.04:

```sh
chmod +x Rufux-x86_64.AppImage
./Rufux-x86_64.AppImage   # opens the GUI; CLI via --help etc.
```

Like the native package it shells out to host tools for
formatting/partitioning (`dosfstools`, `ntfsprogs`, `exfatprogs`,
`e2fsprogs`, `util-linux`, `syslinux`, `udisks2`, `libarchive`).

## Arch Linux (AUR)

`packaging/PKGBUILD` builds from the `v1.0.0` tag:

```sh
cp packaging/PKGBUILD /tmp/rufux-pkg/ && cd /tmp/rufux-pkg
makepkg -si
```

## Flatpak

```sh
flatpak-builder --install build-dir packaging/io.github.hultwl.rufux.json
flatpak run io.github.hultwl.rufux --gui
```

Note: raw block-device access from Flatpak needs `--device=all`
(which the manifest requests) plus host-side udisks2/polkit.

## From source (any distro)

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
sudo cmake --install build
```

Runtime deps: `gtk4`, `udisks2`, `util-linux` (sfdisk),
`dosfstools`, `ntfsprogs`, `exfatprogs`, `e2fsprogs`,
`libarchive` (bsdtar) or `p7zip`, `syslinux`, `curl`.
