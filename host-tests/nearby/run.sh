#!/bin/sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-nearby-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../freeink-sdk/libs/network/NearbyTransfer/src
INC=../../freeink-sdk/libs/network/NearbyTransfer/include

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I"$INC" \
  "$SRC/NearbyTransfer.cpp" test_nearby.cpp -o "$BUILD_DIR/test_nearby"
"$BUILD_DIR/test_nearby"
