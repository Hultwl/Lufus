#!/usr/bin/env bash
# Phase 1 acceptance tests. Usage: test_phase1.sh <path-to-lufus-binary>
set -u
LUFUS="${1:-./build/lufus}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
pass=0; fail=0
ok() { echo "PASS: $1"; pass=$((pass+1)); }
bad() { echo "FAIL: $1"; fail=$((fail+1)); }

[ -x "$LUFUS" ] || { echo "binary not found: $LUFUS"; exit 1; }

# 1. list (human + json)
if "$LUFUS" list >/dev/null 2>&1; then ok "list"; else bad "list"; fi
if "$LUFUS" list --json | grep -q '^\['; then ok "list --json"; else bad "list --json"; fi

# 2. make fake ISO: PVD at 16*2048 + boot record at 17*2048
FAKE="$TMP/fake.iso"
python3 - "$FAKE" <<'EOF'
import sys
p = sys.argv[1]
size = 20*2048
b = bytearray(size)
# PVD
b[16*2048+0] = 1
b[16*2048+1:16*2048+6] = b'CD001'
b[16*2048+40:16*2048+40+11] = b'LUFUS_TEST '
# boot record
b[17*2048+0] = 0
b[17*2048+1:17*2048+6] = b'CD001'
b[17*2048+7:17*2048+30] = b'EL TORITO SPECIFICATION'
open(p,'wb').write(b)
EOF
if "$LUFUS" probe "$FAKE" | grep -q LUFUS_TEST; then ok "probe label"; else bad "probe label"; fi
if "$LUFUS" probe "$FAKE" --detail | grep -q 'bootable: yes'; then ok "probe bootable"; else bad "probe bootable"; fi
if "$LUFUS" probe "$TMP/missing.iso" >/dev/null 2>&1; then bad "probe missing should fail"; else ok "probe missing fails"; fi

# 3. checksum matches sha256sum
if [ "$(sha256sum "$FAKE" | cut -d' ' -f1)" = "$("$LUFUS" checksum "$FAKE" | cut -d' ' -f1)" ]; then
  ok "checksum matches sha256sum"
else
  bad "checksum matches sha256sum"
fi

# 4. dry-run to a file target (needs --allow-file to pass target check, writes nothing)
DST="$TMP/dst.img"
truncate -s 10M "$DST"
BEFORE="$(sha256sum "$DST" | cut -d' ' -f1)"
if "$LUFUS" write "$FAKE" "$DST" --dry-run --allow-file >/dev/null 2>&1; then ok "write --dry-run"; else bad "write --dry-run"; fi
AFTER="$(sha256sum "$DST" | cut -d' ' -f1)"
[ "$BEFORE" = "$AFTER" ] && ok "dry-run writes nothing" || bad "dry-run writes nothing"

# 5. real write to file + verify
if "$LUFUS" write "$FAKE" "$DST" --real --allow-file --yes --verify >/dev/null 2>&1; then ok "write --real --verify"; else bad "write --real --verify"; fi
# dst head must equal src
if cmp -n "$(stat -c%s "$FAKE")" "$FAKE" "$DST"; then ok "written bytes match"; else bad "written bytes match"; fi

# 6. safety: real write without --yes must fail
truncate -s 10M "$DST"
if "$LUFUS" write "$FAKE" "$DST" --real --allow-file >/dev/null 2>&1; then bad "write without --yes should fail"; else ok "write without --yes refused"; fi

# 7. safety: file target without --allow-file must fail
if "$LUFUS" write "$FAKE" "$DST" --dry-run >/dev/null 2>&1; then bad "file target without --allow-file should fail"; else ok "file target refused by default"; fi

echo "--- $pass passed, $fail failed ---"
[ "$fail" -eq 0 ]
