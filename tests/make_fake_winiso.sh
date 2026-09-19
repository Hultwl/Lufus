#!/bin/bash
# Build a small ISO9660+UDF image shaped like Windows install media.
# Contents are placeholders; only names, layout and sizes matter to Rufux.
# Usage: [FAKE_EFI=file.efi] make_fake_winiso.sh OUT.iso [install_wim_mib]
# FAKE_BOOTMGR replaces bootmgr the same way for BIOS boot tests.
# FAKE_EFI replaces efi/boot/bootx64.efi so a firmware boot test can prove the
# UEFI:NTFS chain reached the NTFS partition.
set -e
out=$1; wim_mib=${2:-6}
d=$(mktemp -d)
mkdir -p "$d"/{sources,boot,efi/boot,efi/microsoft/boot,support}
head -c $((wim_mib*1024*1024)) /dev/urandom > "$d/sources/install.wim"
head -c $((2*1024*1024))       /dev/urandom > "$d/sources/boot.wim"
if [ -n "$FAKE_BOOTMGR" ]; then cp "$FAKE_BOOTMGR" "$d/bootmgr"; else head -c 400000 /dev/urandom > "$d/bootmgr"; fi
head -c 65536  /dev/urandom > "$d/bootmgr.efi"
head -c 262144 /dev/urandom > "$d/boot/bcd"
head -c 262144 /dev/urandom > "$d/efi/microsoft/boot/bcd"
if [ -n "$FAKE_EFI" ]; then cp "$FAKE_EFI" "$d/efi/boot/bootx64.efi"; else head -c 1500000 /dev/urandom > "$d/efi/boot/bootx64.efi"; fi
echo "fake setup" > "$d/setup.exe"
echo "fake" > "$d/support/readme.txt"
genisoimage -quiet -iso-level 3 -udf -allow-limited-size -V WIN_FAKE -o "$out" "$d"
rm -rf "$d"
