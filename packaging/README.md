# Packaging Lufus

## Arch Linux (AUR)

`packaging/PKGBUILD` builds from the `v1.0.0` tag:

```sh
cp packaging/PKGBUILD /tmp/lufus-pkg/ && cd /tmp/lufus-pkg
makepkg -si
```

## Flatpak

```sh
flatpak-builder --install build-dir packaging/io.github.hultwl.lufus.json
flatpak run io.github.hultwl.lufus --gui
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
