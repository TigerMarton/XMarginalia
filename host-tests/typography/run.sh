#!/bin/sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-typography-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/activities/reader

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -O2 \
  -I../../src/activities/reader \
  "$SRC/BookSettings.cpp" test_typography.cpp -o "$BUILD_DIR/test_typography"
"$BUILD_DIR/test_typography"
