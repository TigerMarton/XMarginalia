#!/bin/bash
# Multi-Upstream Synchronization & Verification Tool.
# Monitors, audits seams, and verifies synchronization across:
#   1. CrossPoint (crosspoint/develop)
#   2. CrossPlay   (origin/xteink)
#   3. CrossInk    (crossink/main)
#
# Usage:
#   ./scripts_local/sync-all.sh           # Audit seams, remotes, partitions, and host tests
#   ./scripts_local/sync-all.sh --fetch   # Fetch remotes and check commit drift
set -euo pipefail

REPO="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/.." && pwd)"
cd "$REPO"

DO_FETCH=0
if [ "${1:-}" = "--fetch" ]; then
  DO_FETCH=1
fi

echo "===================================================="
echo "Multi-Upstream Synchronization & Seam Verification"
echo "===================================================="

# 1. Audit Remotes Configuration
echo "[1/4] Checking Git Remotes..."
REMOTES=$(git remote)

check_remote() {
  local name="$1"
  local url="$2"
  if echo "$REMOTES" | grep -qw "$name"; then
    echo "  [PASS] Remote '$name' configured"
  else
    echo "  [NOTE] Remote '$name' not found. To add it:"
    echo "         git remote add $name $url"
  fi
}

check_remote "crosspoint" "https://github.com/crosspoint-reader/crosspoint-reader.git"
check_remote "origin"     "https://github.com/ma-r-s/crossplay.git"
check_remote "crossink"   "https://github.com/uxjulia/CrossInk.git"

if [ "$DO_FETCH" -eq 1 ]; then
  echo "  Fetching remotes..."
  git fetch --prune crosspoint || true
  git fetch --prune origin || true
  if echo "$REMOTES" | grep -qw "crossink"; then
    git fetch --prune crossink || true
  fi

  echo "  Upstream drift status:"
  if git rev-parse --verify crosspoint/develop >/dev/null 2>&1; then
    BEHIND_CP=$(git rev-list --count HEAD..crosspoint/develop || echo "0")
    AHEAD_CP=$(git rev-list --count crosspoint/develop..HEAD || echo "0")
    echo "    crosspoint/develop: +${AHEAD_CP} / -${BEHIND_CP} commits relative to HEAD"
  fi
  if git rev-parse --verify origin/xteink >/dev/null 2>&1; then
    BEHIND_XT=$(git rev-list --count HEAD..origin/xteink || echo "0")
    AHEAD_XT=$(git rev-list --count origin/xteink..HEAD || echo "0")
    echo "    origin/xteink:      +${AHEAD_XT} / -${BEHIND_XT} commits relative to HEAD"
  fi
  if git rev-parse --verify crossink/main >/dev/null 2>&1; then
    BEHIND_CI=$(git rev-list --count HEAD..crossink/main || echo "0")
    AHEAD_CI=$(git rev-list --count crossink/main..HEAD || echo "0")
    echo "    crossink/main:      +${AHEAD_CI} / -${BEHIND_CI} commits relative to HEAD"
  fi
fi

# 2. Audit Seam Boundaries & Modularity
echo
echo "[2/4] Auditing Modular Seam Boundaries..."
SEAM_VIOLATIONS=0

# Ensure CrossInk / CrossPlay modules are in clean directories
check_dir_exists() {
  local dir="$1"
  local desc="$2"
  if [ -d "$dir" ]; then
    echo "  [PASS] $desc: $dir"
  else
    echo "  [WARN] Missing directory $dir for $desc"
    SEAM_VIOLATIONS=$((SEAM_VIOLATIONS + 1))
  fi
}

check_file_exists() {
  local file="$1"
  local desc="$2"
  if [ -f "$file" ]; then
    echo "  [PASS] $desc: $file"
  else
    echo "  [WARN] Missing file $file for $desc"
    SEAM_VIOLATIONS=$((SEAM_VIOLATIONS + 1))
  fi
}

check_dir_exists "src/apps_local" "CrossPlay Shelf & Games"
check_file_exists "src/activities/network/NearbyBookTransferActivity.cpp" "CrossInk Nearby P2P Transfer"
check_dir_exists "src/components/themes/dashboard" "CrossInk Dashboard Theme"
check_dir_exists "src/components/themes/minimal" "CrossInk Minimal Theme"
check_dir_exists "host-tests/statusbar" "Modular Status Bar Tests"
check_dir_exists "host-tests/stats" "Hardware Reading Stats Tests"
check_dir_exists "host-tests/nearby" "P2P Nearby Protocol Tests"
check_dir_exists "host-tests/typography" "Typography & Guide Dots Tests"

if [ "$SEAM_VIOLATIONS" -ne 0 ]; then
  echo "  [FAIL] $SEAM_VIOLATIONS modular seam boundary violation(s) detected!" >&2
  exit 1
fi

# 3. Partition Table and Flash Offset Safety
echo
echo "[3/4] Verifying Partition & Flash Safety..."
"$REPO/scripts_local/check-partitions.sh"

# 4. Master Host Unit Tests
echo
echo "[4/4] Executing Master Host Unit Tests..."
"$REPO/host-tests/run-all.sh"

echo
echo "===================================================="
echo "Multi-Upstream Synchronization CI: ALL CHECKS GREEN"
echo "===================================================="
