# Changelog

## v1.0.1

- Rename: Lufus → Rufux. The name Lufus belongs to the established
  Hogjects/Lufus project (Python, MIT); this port rebrands to avoid
  confusion. Binary, app ID, locales, docs, and packaging all renamed.
  No functional changes.

## v1.0.0 (Phase 3 — Stable)

- udisks2 auto-mount: `mount` / `umount`, `create --mode extract` end-to-end on disks
- Privilege guard: clear sudo/pkexec error + polkit policy installed
- Secure Boot: `secureboot-status`, `validate-efi` (PE subsystem check)
- `update-check` against GitHub releases
- i18n infrastructure (`po/`, fr + es samples, `ENABLE_NLS`)
- GUI: `--theme system|dark|light`, Secure Boot status line
- Packaging: `packaging/PKGBUILD`, Flatpak manifest, man page, desktop file
- CI: `.github/workflows/rufux.yml` (build + ctest + artifact)
- `tests/HW_MATRIX.md` + `tests/hw_smoke.sh` for manual hardware validation

## v0.3.0 (Phase 2 — Bootable Parity)

- sfdisk partition, mkfs dispatch, ISO extract, syslinux MBR,
  persistence file, bad-blocks scan, `create` planner, GUI mode/scheme/fs

## v0.2.0 (Phase 1 — Safe Core)

- device scan, ISO probe, SHA-256, safe writer, GTK skeleton, tests

## v0.1.0

- Initial scaffold forked from pbatard/rufus.
