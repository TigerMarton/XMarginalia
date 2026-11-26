#!/bin/sh
# The Daybook browser plugin against the device's core (see test.js). Needs node and the jsdom package; without
# them it says so and passes, so it never blocks a machine that only builds firmware.
#
#   host-tests/daybook/plugin/run.sh
set -e
cd "$(dirname "$0")"
if ! command -v node >/dev/null 2>&1 || ! node -e "require('jsdom')" >/dev/null 2>&1; then
  echo "daybook plugin: skipped (needs node and the jsdom package)"
  exit 0
fi
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-daybook-plugin-$(cd ../../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../../src/apps_local/daybook
"${CXX:-c++}" -std=c++17 -Wall -Wextra -O1 -I"$SRC" parity.cpp "$SRC/DaybookCore.cpp" -o "$BUILD_DIR/parity"
PARITY="$BUILD_DIR/parity" node test.js
