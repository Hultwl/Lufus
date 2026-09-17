# Rufux — 3 Phases to Stable

- **Phase 1 — Safe Core Foundation (DONE, v0.2.0):** real device scan, ISO detail probe, SHA-256, safe writer (dry-run + verify), CLI + GTK progress, tests. No destructive defaults.
- **Phase 2 — Bootable Feature Parity (DONE, v0.3.0):** sfdisk GPT/DOS partition, mkfs dispatch (vfat/ntfs/exfat/ext4), ISO extract (bsdtar/7z), syslinux MBR (table-preserving), persistence file, bad-blocks scan, `create` planner + DD/extract flows, GUI mode/scheme/fs.
- **Phase 3 — Stable Release v1.0 (DONE):** udisks2 auto-mount + end-to-end disk `create`, root guard + polkit policy, Secure Boot status + EFI validation, `update-check`, i18n infra (fr/es), GUI theme + SB line, packaging (PKGBUILD/Flatpak/man/desktop), CI, HW matrix.

## Phase 1 exit criteria (all met)

- [x] `rufux list [--json] [--allow-fixed]` shows node/size/usb/vendor/model/serial/mounted
- [x] `rufux probe <iso> --detail` shows label/size/bootable
- [x] `rufux checksum <file>` matches `sha256sum`
- [x] `rufux write SRC DST --dry-run` simulates with progress, writes nothing
- [x] `rufux write SRC DST --allow-file --yes [--verify]` writes + verifies, refuses fixed disks and mounted targets by default
- [x] GTK GUI: device refresh, ISO pick + checksum, dry-run Start + progress bar + log
- [x] `tests/test_phase1.sh` passes
