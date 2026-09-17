#!/usr/bin/env bash
# Manual hardware smoke test. SAFE BY DEFAULT: plans only.
# Destructive steps need BOTH --device /dev/sdX AND --yes-i-understand.
set -u
LUFUS="${1:-./build/lufus}"
DEV=""; ISO=""; LIVE=0
shift || true
while [ $# -gt 0 ]; do
  case "$1" in
    --device) DEV="$2"; shift 2;;
    --iso) ISO="$2"; shift 2;;
    --yes-i-understand) LIVE=1; shift;;
    *) echo "unknown arg: $1"; exit 2;;
  esac
done
[ -n "$DEV" ] || { echo "usage: hw_smoke.sh [lufus-bin] --device /dev/sdX --iso f.iso [--yes-i-understand]"; exit 2; }
case "$DEV" in /dev/sd*|/dev/nvme*|/dev/mmcblk*|/dev/vd*) :;; *) echo "refusing odd device: $DEV"; exit 2;; esac
echo "== H1 list =="; "$LUFUS" list
[ -n "$ISO" ] || exit 0
echo "== H2 probe =="; "$LUFUS" probe "$ISO" --detail
echo "== H3 checksum =="; "$LUFUS" checksum "$ISO"
echo "== H4/H5 plan =="; "$LUFUS" create "$ISO" "$DEV" --mode extract --scheme gpt --fs vfat --dry-run
echo "== H6 secure boot =="; "$LUFUS" secureboot-status
if [ "$LIVE" -eq 1 ]; then
  echo "== LIVE destructive write =="; sudo "$LUFUS" create "$ISO" "$DEV" --mode extract --scheme gpt --fs vfat --yes
  echo "== H8 badblocks (read-only) =="; sudo "$LUFUS" badblocks "$DEV"
else
  echo "(dry-run only; add --yes-i-understand to execute)"
fi
