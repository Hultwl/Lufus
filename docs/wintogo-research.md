# Windows To Go — research writeup (no code yet)

Question: can Rufux build a bootable full-Windows USB (`--mode wintogo`)
from Linux? Short answer: **yes, with one genuinely fiddly part (BCD
device descriptors) and mandatory hardware testing.** Details below.

## 1. Prior art: has anyone built a Windows BCD from Linux?

Yes — twice over, with the same pattern.

**A. BCD-SYS (jpz4085, GPL-3.0, 47 stars, AppImage-shipped)**
https://github.com/jpz4085/BCD-SYS — "Setup the Windows BCD and boot
files from Linux or macOS… after applying a Windows image with the
wimlib tools. Similar to the bcdboot utility." Dependencies are
literally `hivexsh` + `hivexregedit`. It handles physical installs,
VHDX installs, UEFI+BIOS, locales, descriptions, clean-slate rebuilds.
This is ~90% of our missing backend, proven in the wild, GPL-compatible.

**B. Independent confirmation (PXE deployment, 2011)**
Comment thread on Richard WM Jones's hivex/BCD post
(https://rwmj.wordpress.com/2010/04/03/): a deployment engineer wrote
a perl+hivexsh script doing BCD edits for PXE boot, noting the key
constraint: *"Since hivex can not create a BCD file I just use the one
I find on the installation DVD, clean it out entirely and then create
the appropriate entries."* Same seed-template pattern, second source.

**C. Boundary of prior art: WoeUSB does NOT do this.**
WoeUSB/WoeUSB-ng only builds Windows *installer* media (copies ISO +
GRUB for BIOS). Nobody in the Linux tools space ships full To Go.
We'd be first among the simple tools — upside and risk both.

**Pattern to mirror:** never build a BCD from zero bytes. Seed from a
template (the ISO's own `EFI/Microsoft/Boot/BCD`, or BCD-SYS's
Win10/11 template approach), wipe/replace the osloader entries,
write back. hivex does keys + REG_BINARY values fine.

## 2. Does hivex actually round-trip BCD hives?

Mostly yes, with understood limits:

- **Reads: proven.** hivex's own author demoed BCD export
  (`hivexregedit --export /tmp/BCD '\'`). BCD *is* a registry hive.
- **Writes: proven via BCD-SYS** (adds/replaces keys and values in
  live BCD stores on real disks).
- **Known gotchas:**
  - hivex does **not** replay transaction logs (`.LOG`/`.LOG1`). For
    us this is a non-issue: we write a fresh BCD Windows has never
    mounted, so there are no logs. Never edit a BCD harvested from a
    running Windows install without expecting staleness.
  - hivex cannot create a hive file from nothing (needs a valid base
    block) — hence the seed-template rule above. Non-negotiable.
  - hivexsh writes need `-w` + `commit`; script every edit, verify by
    re-reading. Our flow will parse-back-verify the finished BCD
    before declaring success.
- **Tooling status here:** `hivexsh`/`hivexregedit` are NOT installed
  on this box (Arch: `hivex` package; Ubuntu: `hivex`). CI will need
  them. Runtime dep, fail-fast with install hint like the other tools.

## 3. Element GUIDs and object structure (Win11 23H2+ target)

Sources: Geoff Chappell's BCD reference
(https://www.geoffchappell.com/notes/windows/boot/bcd/objects.htm,
.../elements.htm) + Microsoft's BCDEdit docs. Ground truth to confirm
against: dump the target ISO's own BCD with `hivexregedit` before
building (no Windows machine needed).

**Objects (fresh store, random GUIDs generated per install):**
- `{bootmgr}` = `9dea862c-5cdd-4e70-acc1-f32b344d4795`, type `0x10100002`
  (application). Elements: `description` (0x12000004, "Windows Boot
  Manager"), `default` (0x23000003 → osloader GUID), `displayorder`
  (0x24000001 → osloader GUID list), `timeout` (0x25000004, e.g. 5).
- OS loader: fresh random GUID, type `0x10200003`. Elements:
  `device` (0x11000001) + `osdevice` (0x21000001) → partition device
  descriptor of the NTFS volume; `path` (0x12000002) →
  `\Windows\system32\winload.efi`; `description` (0x12000004);
  `locale` (0x22000005, `en-US`); `inherit` (0x14000006 →
  `{bootloadersettings}` = `6efb52bf-1766-41db-a6b3-0ee5eff72bd7`);
  `systemroot` (0x22000002, `\Windows`); `resumeobject`
  (0x23000003 → resume GUID); `nx` OptIn (0x26000020); `detecthal`
  Yes (0x26000010).
- Resume object: fresh random GUID, type `0x10200004`,
  path `\Windows\system32\winresume.efi`, same device. (Skipping it
  breaks hibernate-resume; cheap to include, so include it.)
- `{default}`/`{current}` are runtime aliases, not stored objects.
- Skip: memdiag, hypervisor settings, custom boot apps.

**The fiddly part (flagged honestly):** `device`-type elements
(0x11000001/0x21000001) are binary device descriptors encoding the
disk signature + partition offset — which we know, because we
partitioned the disk (MBR signature we wrote, GPT GUIDs from sfdisk).
Exact byte layout per Geoff Chappell's device-elements page; this is
the single most likely source of round-1 failure (symptom:
`0xc000000e`, "a required device isn't connected").

**Secure Boot note:** BCD contents are data, not code — SB verifies
`bootmgfw.efi`/`winload.efi` signatures, not our edits. Our changes
don't weaken the SB chain. (Same reason Rufus's WUE XML is SB-safe.)

## 4. Honest estimate

- Research: done (~this doc).
- Build: BCD module (~250-350 lines: template seed, hivexsh script
  generation, element writers, parse-back verify) + `wintogo` flow
  (partition/format/mount/wimapply/bootfiles/BCD/unmount, mostly
  reusing `windows`-mode code) + `--experimental` gating + tests.
  Roughly 6-10 hours of implementation.
- Hardware rounds (the real cost): expect **2-4 boot-test rounds**.
  Round 1: does the firmware list the stick / does bootmgr appear.
  Round 2: winload path/device fixes (`0xc000000e` territory).
  Round 3+: driver stack (`INACCESSIBLE_BOOT_DEVICE`) or locale/
  description polish. Each round = me changing the recipe, you
  writing + booting + photographing the exact error. Elapsed time
  depends on your testing bandwidth, not mine — budget days, not hours.

## 5. Build preconditions (all doable now)

- `--mode wintogo` hidden behind `--experimental` + `--yes`, loud
  warning, docs labeled EXPERIMENTAL. Never a peer of `--mode dd`.
- Needs from you: a Windows 11 ISO + license, one test machine, and
  willingness to photograph bluescreens. Step 0 of the build is
  dumping the ISO's own BCD with hivex for ground-truth element codes.
- Failure story (what a user sees, worst first):
  1. Firmware ignores the stick entirely (no boot entry) — looks
     dead. Mitigation: pre-flight validates ESP files + BCD
     parse-back; log tells exactly which check failed.
  2. Boot Manager `0xc000000e`/`0xc0000225` — BCD device element
     wrong. The log guide maps status codes to causes.
  3. `INACCESSIBLE_BOOT_DEVICE` after the logo — USB driver stack,
     may be machine-specific; documented as known risk.
  4. First impression protection: dry-run validates every step
     without touching the disk; real runs end with a verification
     summary stamped EXPERIMENTAL.

**Verdict: proceed.** Prior art exists and is GPL-compatible, the
tooling story is understood, the GUIDs are documented (to be
confirmed against the ISO dump), and the risk is bounded by the
experimental flag + your hardware loop. The only thing that would
change this verdict: if no Windows 11 ISO is available for ground
truth — then we stop before writing code.
