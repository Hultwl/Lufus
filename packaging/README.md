# Packaging Rufux

## AppImage (recommended portable build)

Attached to every GitHub release, built by
`.github/workflows/appimage.yml` on Ubuntu 24.04 (GTK 4.10+ floor):

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

Scope (honest): probing/checksum/extract-to-directory work, but raw
block-device writes are refused inside the sandbox — there is no host
`pkexec` path and no device access worth having, so the app fails fast
with directions instead of obscure errors. Full functionality needs the
AppImage or a native package. (A future udisks2 D-Bus backend could lift
this; tracked, not started.)

## From source (any distro)

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
sudo cmake --install build
```

Runtime deps: `gtk4`, `udisks2`, `util-linux` (sfdisk), `openssl`,
`dosfstools`, `ntfsprogs`, `exfatprogs`, `e2fsprogs`,
`libarchive` (bsdtar) or `p7zip`, `syslinux`, `curl`.
