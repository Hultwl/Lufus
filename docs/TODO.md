# TODO (parked, not forgotten)

## Windows To Go (`--mode wintogo`) — parked
- Recipe researched and saved in `docs/wintogo-research.md`. Verdict: feasible.
- **Blocked on hardware, not code:** needs a 32GB+ *fast* USB stick
  (SSD-class, USB 3.x — a bargain Cruzer won't cut it) plus a Win11
  ISO + license for ground truth and boot testing.
- **Blocked on bandwidth:** expect 2-4 boot-test rounds with photos
  of bluescreens. Unstart until both exist.
- When unblocked: BCD module (template seed + hivex edits) behind
  `--experimental`, hardware loop per the research doc. No code exists
  yet on purpose.

## Candidates (not committed)
- UDisks2 D-Bus backend (would unlock a future Flatpak revisit).
- More locales beyond EN/FR/ES (contributions welcome).
- `mbr`/`gpt` hybrid BIOS-compatible disks (see reviewer note in 1.2.1).
