#!/usr/bin/env bash
# Packaging consistency: the release version must agree everywhere.
# (The -git PKGBUILD tracks branches via pkgver() and is exempt.)
set -u
SRC_DIR="$(cd "$(dirname "$0")/.." && pwd)"
pass=0; fail=0
ok() { echo "PASS: $1"; pass=$((pass+1)); }
bad() { echo "FAIL: $1"; fail=$((fail+1)); }

CMAKE_VER="$(grep -oP 'project\(Rufux VERSION \K[0-9.]+' "$SRC_DIR/CMakeLists.txt")"
[ -n "$CMAKE_VER" ] && ok "cmake version $CMAKE_VER" || bad "cmake version"

PKG_VER="$(grep -oP '^pkgver=\K[0-9.]+' "$SRC_DIR/packaging/PKGBUILD")"
[ "$PKG_VER" = "$CMAKE_VER" ] && ok "PKGBUILD pkgver $PKG_VER" || bad "PKGBUILD pkgver $PKG_VER != $CMAKE_VER"
if grep -q '#tag=v${pkgver}' "$SRC_DIR/packaging/PKGBUILD"; then
  ok "PKGBUILD tag tracks pkgver"
else
  bad "PKGBUILD tag does not track pkgver"
fi

# No Flatpak manifest in-tree (removed deliberately); nothing to check.

# README uses dynamic shields (release badge), no hardcoded version to drift.
if grep -q "^## $CMAKE_VER" "$SRC_DIR/CHANGELOG.md"; then ok "CHANGELOG $CMAKE_VER"; else bad "CHANGELOG entry"; fi

echo "--- $pass passed, $fail failed ---"
[ "$fail" -eq 0 ]
