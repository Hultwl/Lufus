# Changelog

## 1.1.4 (Flathub submission release)

- AppStream metadata + real dark-theme screenshot + icon/desktop
  install set, for the Flathub submission.
- Portal theme fix: unwrap variant-wrapped color-scheme replies
  (COSMIC answers on the legacy namespace); proven live.
- Manifest: UDisks2 + PolicyKit1 talk-names declared; tag tracks
  the release. Flatpak block writes still refused up front
  (documented) — needs a real `flatpak run` device test.

- Partition rescan actually runs (was log-only): standalone
  `partition` rescans via partprobe + udevadm settle.
- Honest UEFI validation message: header/subsystem check only,
  no signature verified.
- Per-filesystem label limits everywhere (vfat 11, exfat 15,
  ext 16, ntfs/udf 32): GUI truncates, CLI fails fast.
- Flatpak: block devices refused up front with directions
  (no host pkexec path); scope documented in packaging/README.
- Version drift guard: tests/test_packaging.sh asserts CMake,
  PKGBUILD, Flatpak manifest, and CHANGELOG agree.
- No more shell-outs: udisksctl via fork+execvp capture,
  du via nftw, curl/bootctl via shared rufux_capture helper.
- SHA-256 now OpenSSL EVP (hand-rolled implementation deleted);
  build requires libcrypto.
- El Torito parsed structurally (catalog validation-entry
  platform id) instead of byte-scanning for 0xEF.
- mkfs argv on stack (reentrant); persist checks tools before
  truncating; badblocks labeled a read-only surface scan.

- UI stays alive during burns: worker pipes drain non-blocking
  (previously the window froze through long silent phases).
- Theme follows the desktop via the Settings portal (both
  namespaces), GTK settings.ini, then COSMIC-dark default.

- Worker stderr now streams into the GUI log (auth failures, refusal
  reasons) plus the worker exit code — failures are never silent.
- The worker dismounts the target's own partitions before touching
  it (Rufus behavior; consent was the START warning) instead of
  refusing auto-mounted sticks.

- Real progress bar: every flow reports staged percent end to end
  (bad-blocks 0-10, zero 10-15, write 15-85, verify 85-100; extract
  8-82 via destination-growth polling, rest named stages). CLI shows
  speed + ETA; GUI status shows live percent.
- Extraction progress for CLI `extract` too (was silent).
- Run safety: START/CLOSE lock while the worker runs (no double
  burns, no closing mid-write); writer refuses source == target.
- vfat + >4GiB image refused early with an NTFS/exFAT pointer
  (FAT32 cannot hold such files; UEFI:NTFS driver is future work).

- Consolidated stable release: everything below in one cut.
- AppImage attached to the release (built by CI on Ubuntu 24.04).
- Bare `rufux` with a display opens the GUI (app-grid friendly).
- Root escalation that works from AppImages: resolve the real
  executable (readlink, not /proc/self/exe through env) and
  re-run the $APPIMAGE file itself (FUSE mounts are user-private,
  root gets EACCES inside them).
- GUI runs as the invoking user; START escalates per-operation
  (pkexec worker with streamed progress) instead of running the
  whole app as root. Kills the root-on-Wayland display failures,
  theme loss, portal loss, and dconf spam at the root.

## v1.0.3

- Fix GPT ESP type: real GUID C12A7328-F81F-11D2-BA4B-00A0C93EC93B
  (was a literal placeholder) + explicit portable sfdisk lines.
- Fix stack buffer overflow in update-check error path (bound is
  now the 128-byte stack buffer, not the caller's errcap).
- Fix CLI-only link failure: gui stub always compiles.
- Portable tool lookup: bare names resolved via PATH instead of
  hardcoded /usr/bin (Debian/Ubuntu keep mkfs.* in /usr/sbin).
- CI: install ntfs-3g for the NTFS format test.

## v1.0.2

- GUI polish: portal-native file pickers (GtkFileDialog opens the
  system file manager), theme inheritance (flag > rufux config >
  GTK settings > COSMIC dark default), log timestamps, app icon
  installed; dconf silenced for root sessions.

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
