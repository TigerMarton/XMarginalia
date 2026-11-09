#!/bin/sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-statusbar-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -O2 \
  test_statusbar.cpp -o "$BUILD_DIR/test_statusbar"
"$BUILD_DIR/test_statusbar"
