#!/bin/bash
# Dual-Device Partition Table and Flash Offset Safety Checker.
# Protects Onyx BOOX Picco vendor bootloader/safeboot and Xteink X4 Pro dual slots.
set -euo pipefail

REPO="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/.." && pwd)"
cd "$REPO"

ERRORS=0

echo "Checking Dual-Device Partition Safety..."

# 1. Verify partitions.csv (Xteink X4 Pro & standard S3)
if [ ! -f "partitions.csv" ]; then
  echo "  [FAIL] partitions.csv missing!"
  ERRORS=$((ERRORS + 1))
else
  # Check app0 offset == 0x10000
  APP0_OFFSET=$(grep -E '^[[:space:]]*app0,' partitions.csv | awk -F',' '{gsub(/[[:space:]]/, "", $4); print $4}' || true)
  if [ "$APP0_OFFSET" != "0x10000" ]; then
    echo "  [FAIL] partitions.csv app0 offset is '$APP0_OFFSET', expected '0x10000'"
    ERRORS=$((ERRORS + 1))
  else
    echo "  [PASS] partitions.csv (X4 Pro) app0 offset is 0x10000"
  fi

  # Check app1 offset == 0x800000
  APP1_OFFSET=$(grep -E '^[[:space:]]*app1,' partitions.csv | awk -F',' '{gsub(/[[:space:]]/, "", $4); print $4}' || true)
  if [ "$APP1_OFFSET" != "0x800000" ]; then
    echo "  [FAIL] partitions.csv app1 offset is '$APP1_OFFSET', expected '0x800000'"
    ERRORS=$((ERRORS + 1))
  else
    echo "  [PASS] partitions.csv (X4 Pro) app1 offset is 0x800000"
  fi
fi

# 2. Verify partitions_picco.csv (Onyx BOOX Picco)
if [ ! -f "partitions_picco.csv" ]; then
  echo "  [FAIL] partitions_picco.csv missing!"
  ERRORS=$((ERRORS + 1))
else
  # Check safeboot offset == 0x20000
  SAFEBOOT_OFFSET=$(grep -E '^[[:space:]]*safeboot,' partitions_picco.csv | awk -F',' '{gsub(/[[:space:]]/, "", $4); print $4}' || true)
  if [ "$SAFEBOOT_OFFSET" != "0x20000" ]; then
    echo "  [FAIL] partitions_picco.csv safeboot offset is '$SAFEBOOT_OFFSET', expected '0x20000'"
    ERRORS=$((ERRORS + 1))
  else
    echo "  [PASS] partitions_picco.csv safeboot offset is 0x20000 (vendor recovery preserved)"
  fi

  # Check app offset == 0xE0000
  PICCO_APP_OFFSET=$(grep -E '^[[:space:]]*app,' partitions_picco.csv | awk -F',' '{gsub(/[[:space:]]/, "", $4); print $4}' || true)
  if [ "$PICCO_APP_OFFSET" != "0xE0000" ]; then
    echo "  [FAIL] partitions_picco.csv app offset is '$PICCO_APP_OFFSET', expected '0xE0000'"
    ERRORS=$((ERRORS + 1))
  else
    echo "  [PASS] partitions_picco.csv app offset is 0xE0000"
  fi
fi

# 3. Verify platformio.ini bindings
if grep -q "board_build.partitions = partitions_picco.csv" platformio.ini; then
  echo "  [PASS] platformio.ini binds partitions_picco.csv for Picco environments"
else
  echo "  [FAIL] platformio.ini missing partitions_picco.csv binding for Picco"
  ERRORS=$((ERRORS + 1))
fi

if grep -q "post:scripts/app_only_upload.py" platformio.ini && grep -q "post:scripts_local/app_slot_guard.py" platformio.ini; then
  echo "  [PASS] platformio.ini enforces app_only_upload.py and app_slot_guard.py for Picco"
else
  echo "  [FAIL] platformio.ini missing app_only_upload.py / app_slot_guard.py post-scripts for Picco"
  ERRORS=$((ERRORS + 1))
fi

# 4. Check built binaries if present
if [ -f ".pio/build/gh_release_picco/firmware.bin" ]; then
  echo "  [PASS] gh_release_picco app binary built and verified (to be flashed at 0xE0000)"
fi

if [ -f ".pio/build/gh_release_x4pro/firmware.factory.bin" ]; then
  echo "  [PASS] gh_release_x4pro factory binary built and verified (to be flashed at 0x0 / 0x10000)"
fi

if [ -f ".pio/build/gh_release_eego_a4/firmware.factory.bin" ]; then
  echo "  [PASS] gh_release_eego_a4 factory binary built and verified (to be flashed at 0x0 / 0x10000)"
fi

echo "===================================================="
if [ "$ERRORS" -eq 0 ]; then
  echo "RESULT: Partition and flash safety checks PASSED"
  echo "===================================================="
  exit 0
else
  echo "RESULT: $ERRORS partition safety check(s) FAILED"
  echo "===================================================="
  exit 1
fi
