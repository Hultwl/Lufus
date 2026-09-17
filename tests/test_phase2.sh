#!/usr/bin/env bash
# Phase 2 acceptance: partition, format, extract, boot, persist, badblocks, create.
set -u
LUFUS="${1:-./build/lufus}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
pass=0; fail=0
ok() { echo "PASS: $1"; pass=$((pass+1)); }
bad() { echo "FAIL: $1"; fail=$((fail+1)); }
[ -x "$LUFUS" ] || { echo "binary not found: $LUFUS"; exit 1; }

# fixture: small tree -> real ISO via xorriso
mkdir -p "$TMP/tree/EFI/BOOT"
echo hello-lufus > "$TMP/tree/README.txt"
echo boot-stub > "$TMP/tree/EFI/BOOT/bootx64.efi"
ISO="$TMP/test.iso"
xorriso -as mkisofs -quiet -V LUFUS2 -o "$ISO" "$TMP/tree" 2>/dev/null \
  || { bad "fixture xorriso mkisofs"; echo "--- $pass passed, $fail failed ---"; exit 1; }
ok "fixture iso built"

# 1. extract (real, to dir)
OUT="$TMP/out"
"$LUFUS" extract "$ISO" "$OUT" >/dev/null 2>&1 && [ -f "$OUT/README.txt" ] && ok "extract" || bad "extract"
# dry-run prints plan
"$LUFUS" extract "$ISO" "$TMP/out2" --dry-run | grep -q "dry-run" && ok "extract --dry-run" || bad "extract --dry-run"

# 2. partition a disk image file (64M) gpt + dos
IMG="$TMP/disk.img"
truncate -s 64M "$IMG"
"$LUFUS" partition "$IMG" --scheme gpt --layout single --real --allow-file --yes >/dev/null 2>&1 \
  && sfdisk -d "$IMG" 2>/dev/null | grep -q "label: gpt" && ok "partition gpt" || bad "partition gpt"
truncate -s 64M "$IMG"
"$LUFUS" partition "$IMG" --scheme dos --layout esp+main --real --allow-file --yes >/dev/null 2>&1 \
  && sfdisk -d "$IMG" 2>/dev/null | grep -q "label: dos" && ok "partition dos esp+main" || bad "partition dos esp+main"
# safety: without --yes must fail
truncate -s 64M "$IMG"
"$LUFUS" partition "$IMG" --scheme gpt --real --allow-file >/dev/null 2>&1 && bad "partition needs --yes" || ok "partition needs --yes"

# 3. format whole-file images (rootless)
F32="$TMP/f32.img"; truncate -s 32M "$F32"
"$LUFUS" format "$F32" --fs vfat --label LUFUS --real --allow-file --yes >/dev/null 2>&1 \
  && blkid -o value -s TYPE "$F32" 2>/dev/null | grep -q vfat && ok "format vfat" || bad "format vfat"
FE4="$TMP/e4.img"; truncate -s 64M "$FE4"
"$LUFUS" format "$FE4" --fs ext4 --label LUFUS --real --allow-file --yes >/dev/null 2>&1 \
  && blkid -o value -s TYPE "$FE4" 2>/dev/null | grep -q ext4 && ok "format ext4" || bad "format ext4"
FX="$TMP/ex.img"; truncate -s 32M "$FX"
"$LUFUS" format "$FX" --fs exfat --real --allow-file --yes >/dev/null 2>&1 \
  && blkid -o value -s TYPE "$FX" 2>/dev/null | grep -q exfat && ok "format exfat" || bad "format exfat"
FN="$TMP/nt.img"; truncate -s 64M "$FN"
"$LUFUS" format "$FN" --fs ntfs --label LUFUS --real --allow-file --yes >/dev/null 2>&1 \
  && blkid -o value -s TYPE "$FN" 2>/dev/null | grep -q ntfs && ok "format ntfs" || bad "format ntfs"

# 4. install-boot MBR preserves partition table
truncate -s 64M "$IMG"
"$LUFUS" partition "$IMG" --scheme dos --layout single --real --allow-file --yes >/dev/null 2>&1
BEFORE_PT="$(sfdisk -d "$IMG" 2>/dev/null | grep -v '^label:')"
"$LUFUS" install-boot "$IMG" --mbr bios --real --allow-file --yes >/dev/null 2>&1 \
  && AFTER_PT="$(sfdisk -d "$IMG" 2>/dev/null | grep -v '^label:')" \
  && [ "$BEFORE_PT" = "$AFTER_PT" ] && ok "install-boot preserves table" || bad "install-boot preserves table"

# 5. persist file
PDIR="$TMP/mnt"; mkdir -p "$PDIR"
"$LUFUS" persist "$PDIR" --size 16 --label casper-rw >/dev/null 2>&1 \
  && [ -f "$PDIR/casper-rw" ] \
  && blkid -o value -s TYPE "$PDIR/casper-rw" 2>/dev/null | grep -q ext4 && ok "persist" || bad "persist"

# 6. badblocks clean file
"$LUFUS" badblocks "$FE4" --allow-file | grep -q "0 bad" && ok "badblocks" || bad "badblocks"

# 7. create: dd dry-run + real to file; extract to dir; disk plan
SRC4="$TMP/src4.img"; head -c 1048576 /dev/urandom > "$SRC4"
DST4="$TMP/dst4.img"; truncate -s 4M "$DST4"
"$LUFUS" create "$SRC4" "$DST4" --mode dd --dry-run --allow-file | grep -q "dry-run" && ok "create dd dry-run" || bad "create dd dry-run"
"$LUFUS" create "$SRC4" "$DST4" --mode dd --real --allow-file --yes --verify >/dev/null 2>&1 \
  && cmp -n "$(stat -c%s "$SRC4")" "$SRC4" "$DST4" && ok "create dd real" || bad "create dd real"
CDIR="$TMP/cdir"; mkdir -p "$CDIR"
"$LUFUS" create "$ISO" "$CDIR" --mode extract --real >/dev/null 2>&1 \
  && [ -f "$CDIR/README.txt" ] && ok "create extract to dir" || bad "create extract to dir"
truncate -s 64M "$IMG"
"$LUFUS" create "$ISO" "$IMG" --mode extract --scheme gpt --fs vfat --dry-run --allow-file | grep -q "steps:" \
  && ok "create disk plan" || bad "create disk plan"

echo "--- $pass passed, $fail failed ---"
[ "$fail" -eq 0 ]
