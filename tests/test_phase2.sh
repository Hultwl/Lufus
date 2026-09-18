#!/usr/bin/env bash
# Phase 2 acceptance: partition, format, extract, boot, persist, badblocks, create.
set -u
RUFUX="${1:-./build/rufux}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
pass=0; fail=0
ok() { echo "PASS: $1"; pass=$((pass+1)); }
bad() { echo "FAIL: $1"; fail=$((fail+1)); }
[ -x "$RUFUX" ] || { echo "binary not found: $RUFUX"; exit 1; }

# fixture: small tree -> real ISO via xorriso
mkdir -p "$TMP/tree/EFI/BOOT"
echo hello-rufux > "$TMP/tree/README.txt"
echo boot-stub > "$TMP/tree/EFI/BOOT/bootx64.efi"
ISO="$TMP/test.iso"
xorriso -as mkisofs -quiet -V RUFUX2 -o "$ISO" "$TMP/tree" 2>/dev/null \
  || { bad "fixture xorriso mkisofs"; echo "--- $pass passed, $fail failed ---"; exit 1; }
ok "fixture iso built"

# 1. extract (real, to dir)
OUT="$TMP/out"
"$RUFUX" extract "$ISO" "$OUT" >/dev/null 2>&1 && [ -f "$OUT/README.txt" ] && ok "extract" || bad "extract"
# dry-run prints plan
"$RUFUX" extract "$ISO" "$TMP/out2" --dry-run | grep -q "dry-run" && ok "extract --dry-run" || bad "extract --dry-run"

# 2. partition a disk image file (64M) gpt + dos
IMG="$TMP/disk.img"
truncate -s 64M "$IMG"
"$RUFUX" partition "$IMG" --scheme gpt --layout single --real --allow-file --yes >/dev/null 2>&1 \
  && sfdisk -d "$IMG" 2>/dev/null | grep -q "label: gpt" && ok "partition gpt" || bad "partition gpt"
truncate -s 64M "$IMG"
"$RUFUX" partition "$IMG" --scheme dos --layout esp+main --real --allow-file --yes >/dev/null 2>&1 \
  && sfdisk -d "$IMG" 2>/dev/null | grep -q "label: dos" && ok "partition dos esp+main" || bad "partition dos esp+main"
# safety: without --yes must fail
truncate -s 64M "$IMG"
"$RUFUX" partition "$IMG" --scheme gpt --real --allow-file >/dev/null 2>&1 && bad "partition needs --yes" || ok "partition needs --yes"

# 3. format whole-file images (rootless)
F32="$TMP/f32.img"; truncate -s 32M "$F32"
"$RUFUX" format "$F32" --fs vfat --label RUFUX --real --allow-file --yes >/dev/null 2>&1 \
  && blkid -o value -s TYPE "$F32" 2>/dev/null | grep -q vfat && ok "format vfat" || bad "format vfat"
FE4="$TMP/e4.img"; truncate -s 64M "$FE4"
"$RUFUX" format "$FE4" --fs ext4 --label RUFUX --real --allow-file --yes >/dev/null 2>&1 \
  && blkid -o value -s TYPE "$FE4" 2>/dev/null | grep -q ext4 && ok "format ext4" || bad "format ext4"
FX="$TMP/ex.img"; truncate -s 32M "$FX"
"$RUFUX" format "$FX" --fs exfat --real --allow-file --yes >/dev/null 2>&1 \
  && blkid -o value -s TYPE "$FX" 2>/dev/null | grep -q exfat && ok "format exfat" || bad "format exfat"
FN="$TMP/nt.img"; truncate -s 64M "$FN"
"$RUFUX" format "$FN" --fs ntfs --label RUFUX --real --allow-file --yes >/dev/null 2>&1 \
  && blkid -o value -s TYPE "$FN" 2>/dev/null | grep -q ntfs && ok "format ntfs" || bad "format ntfs"

# 4. install-boot MBR preserves partition table
truncate -s 64M "$IMG"
"$RUFUX" partition "$IMG" --scheme dos --layout single --real --allow-file --yes >/dev/null 2>&1
BEFORE_PT="$(sfdisk -d "$IMG" 2>/dev/null | grep -v '^label:')"
if "$RUFUX" install-boot "$IMG" --mbr bios --real --allow-file --yes >"$TMP/boot.log" 2>&1 \
  && AFTER_PT="$(sfdisk -d "$IMG" 2>/dev/null | grep -v '^label:')" \
  && [ "$BEFORE_PT" = "$AFTER_PT" ]; then
  ok "install-boot preserves table"
else
  bad "install-boot preserves table"
  echo "--- install-boot output ---"; cat "$TMP/boot.log"
fi

# 5. persist file
PDIR="$TMP/mnt"; mkdir -p "$PDIR"
"$RUFUX" persist "$PDIR" --size 16 --label casper-rw >/dev/null 2>&1 \
  && [ -f "$PDIR/casper-rw" ] \
  && blkid -o value -s TYPE "$PDIR/casper-rw" 2>/dev/null | grep -q ext4 && ok "persist" || bad "persist"

# 6. badblocks clean file
"$RUFUX" badblocks "$FE4" --allow-file | grep -q "0 bad" && ok "badblocks" || bad "badblocks"

# 7. create: dd dry-run + real to file; extract to dir; disk plan
SRC4="$TMP/src4.img"; head -c 1048576 /dev/urandom > "$SRC4"
DST4="$TMP/dst4.img"; truncate -s 4M "$DST4"
"$RUFUX" create "$SRC4" "$DST4" --mode dd --dry-run --allow-file | grep -q "dry-run" && ok "create dd dry-run" || bad "create dd dry-run"
"$RUFUX" create "$SRC4" "$DST4" --mode dd --real --allow-file --yes --verify >/dev/null 2>&1 \
  && cmp -n "$(stat -c%s "$SRC4")" "$SRC4" "$DST4" && ok "create dd real" || bad "create dd real"
CDIR="$TMP/cdir"; mkdir -p "$CDIR"
"$RUFUX" create "$ISO" "$CDIR" --mode extract --real --yes >/dev/null 2>&1 \
  && [ -f "$CDIR/README.txt" ] && [ -f "$CDIR/autorun.inf" ] && ok "create extract to dir" || bad "create extract to dir"
truncate -s 64M "$IMG"
"$RUFUX" create "$ISO" "$IMG" --mode extract --scheme gpt --fs vfat --dry-run --allow-file | grep -q "steps:" \
  && ok "create disk plan" || bad "create disk plan"

# 8. vfat refuses >4GiB images early (sparse fixture, instant)
BIG="$TMP/big.iso"; truncate -s 5G "$BIG"
if "$RUFUX" create "$BIG" "$TMP/cdir2" --mode extract --fs vfat --dry-run --allow-file 2>&1 | grep -q "4 GiB"; then
  ok "vfat >4GiB refused"
else
  bad "vfat >4GiB refused"
fi

# 10. fixed VHD writes payload minus footer; dynamic refused
python3 - "$TMP/fixed.vhd" "$TMP/dyn.vhd" <<'EOF'
import sys, struct
payload = bytes((i * 37) & 0xFF for i in range(4096))
for p, dtype in ((sys.argv[1], 2), (sys.argv[2], 3)):
    foot = bytearray(512)
    foot[0:8] = b'conectix'
    struct.pack_into('>I', foot, 8, 2)          # features
    struct.pack_into('>Q', foot, 48, 4096)      # current size
    struct.pack_into('>I', foot, 60, dtype)     # disk type
    s = sum(b for i, b in enumerate(foot) if not 64 <= i < 68)
    struct.pack_into('>I', foot, 64, (~s) & 0xFFFFFFFF)
    open(p, 'wb').write(payload + bytes(foot))
EOF
VOUT="$TMP/vhd-out.img"; truncate -s 8K "$VOUT"
"$RUFUX" write "$TMP/fixed.vhd" "$VOUT" --real --allow-file --yes --verify >/dev/null 2>&1 \
  && cmp -n 4096 "$TMP/fixed.vhd" "$VOUT" && ok "fixed VHD payload" || bad "fixed VHD payload"
if "$RUFUX" write "$TMP/dyn.vhd" "$VOUT" --real --allow-file --yes >/dev/null 2>&1; then
  bad "dynamic VHD refused"
else
  ok "dynamic VHD refused"
fi

# 11. badblocks write-pattern pass on a file
WB="$TMP/wb.img"; head -c 2097152 /dev/urandom > "$WB"
"$RUFUX" badblocks "$WB" --allow-file --write-patterns 1 --yes >/dev/null 2>&1 \
  && [ "$(head -c 1 "$WB" | od -An -tx1 | tr -d ' \n')" = "aa" ] && ok "badblocks write pass" || bad "badblocks write pass"

# 9. over-long labels refused before any work (vfat 11, exfat 15, ext 16)
LONG="ThisLabelIsDefinitelyWayTooLongForAnyFilesystem"
if "$RUFUX" format "$F32" --fs vfat --label "$LONG" --real --allow-file --yes >/dev/null 2>&1; then
  bad "vfat long label refused"
else
  ok "vfat long label refused"
fi
if "$RUFUX" format "$FX" --fs exfat --label "SixteenCharsLong!" --real --allow-file --yes >/dev/null 2>&1; then
  bad "exfat long label refused"
else
  ok "exfat long label refused"
fi

echo "--- $pass passed, $fail failed ---"
[ "$fail" -eq 0 ]
