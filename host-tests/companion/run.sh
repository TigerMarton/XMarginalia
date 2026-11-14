#!/bin/sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-companion-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/companion

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -O2 -I"$SRC" \
  "$SRC/CompanionCore.cpp" test_companion_core.cpp -o "$BUILD_DIR/test_companion_core"
"$BUILD_DIR/test_companion_core"
