#!/bin/sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-plugins-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -O2 \
  -I../../src/activities/plugins -I../../src/util \
  test_plugins.cpp -o "$BUILD_DIR/test_plugins"
"$BUILD_DIR/test_plugins"
