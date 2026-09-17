# Lufus — 3 Phases to Stable

- **Phase 1 — Safe Core Foundation (DONE, v0.2.0):** real device scan, ISO detail probe, SHA-256, safe writer (dry-run + verify), CLI + GTK progress, tests. No destructive defaults.
- **Phase 2 — Bootable Feature Parity (DONE, v0.3.0):** sfdisk GPT/DOS partition, mkfs dispatch (vfat/ntfs/exfat/ext4), ISO extract (bsdtar/7z), syslinux MBR (table-preserving), persistence file, bad-blocks scan, `create` planner + DD/extract flows, GUI mode/scheme/fs.
- **Phase 3 — Stable Release v1.0 (next):** polkit + udisks2 auto-mount, Secure Boot validation, packaging (AUR/Flatpak/deb), auto-update, 38-lang UI, dark mode, HW test matrix, CI release.

## Phase 1 exit criteria (all met)

- [x] `lufus list [--json] [--allow-fixed]` shows node/size/usb/vendor/model/serial/mounted
- [x] `lufus probe <iso> --detail` shows label/size/bootable
- [x] `lufus checksum <file>` matches `sha256sum`
- [x] `lufus write SRC DST --dry-run` simulates with progress, writes nothing
- [x] `lufus write SRC DST --allow-file --yes [--verify]` writes + verifies, refuses fixed disks and mounted targets by default
- [x] GTK GUI: device refresh, ISO pick + checksum, dry-run Start + progress bar + log
- [x] `tests/test_phase1.sh` passes
