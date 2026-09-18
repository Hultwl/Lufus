# Packaging Rufux

Three ways out the door. The AppImage is the one most people want.

## AppImage (recommended)

Attached to every GitHub release, built by
`.github/workflows/appimage.yml` on Ubuntu 24.04:

```sh
chmod +x Rufux-x86_64.AppImage
./Rufux-x86_64.AppImage   # opens the GUI; CLI via --help etc.
```

Same deal as native: it shells out to host tools for the disk work
(`dosfstools`, `ntfsprogs`, `exfatprogs`, `e2fsprogs`, `util-linux`,
`syslinux`, `udisks2`, `libarchive`), so those need to be installed.
No Flatpak — sandboxes and raw disks don't mix, tried that, walked away.

## Arch Linux (AUR)

`packaging/PKGBUILD` builds from the release tag:

```sh
cp packaging/PKGBUILD /tmp/rufux-pkg/ && cd /tmp/rufux-pkg
makepkg -si
```

Living on the edge? `packaging/aur/rufux-git/` tracks `main` instead —
`paru -S rufux-git` and every rebuild follows the latest commit.

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
