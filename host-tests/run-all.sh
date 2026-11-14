#!/bin/bash
# Master host test runner for CrossInk / CrossPlay features.
# Runs nearby, stats, typography, statusbar, and settingsorder suites.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE/.."

SUITES=(
  "nearby"
  "stats"
  "typography"
  "statusbar"
  "settingsorder"
  "tabletop"
  "companion"
)

TOTAL_CHECKS=0
FAILED=0
START_TIME=$(date +%s)

printf "====================================================\n"
printf "CrossInk / CrossPlay Master Host Test Runner\n"
printf "====================================================\n"

for suite in "${SUITES[@]}"; do
  suite_dir="host-tests/$suite"
  if [ ! -x "$suite_dir/run.sh" ]; then
    printf "  %-16s [ERROR: run.sh missing or not executable]\n" "$suite"
    FAILED=1
    continue
  fi

  T0=$(date +%s)
  OUTPUT=$("$suite_dir/run.sh" 2>&1) || {
    printf "  %-16s [FAILED]\n" "$suite"
    echo "$OUTPUT" | sed 's/^/      /'
    FAILED=1
    continue
  }
  T1=$(date +%s)
  DURATION=$((T1 - T0))

  CHECKS=$(echo "$OUTPUT" | grep -oE '[0-9]+ checks' | awk '{print $1}' | tail -n 1 || true)
  if [ -n "$CHECKS" ]; then
    TOTAL_CHECKS=$((TOTAL_CHECKS + CHECKS))
    printf "  %-16s [PASS] (%'d checks, %ds)\n" "$suite" "$CHECKS" "$DURATION"
  else
    printf "  %-16s [PASS] (%ds)\n" "$suite" "$DURATION"
  fi
done

TOTAL_TIME=$(( $(date +%s) - START_TIME ))
printf "====================================================\n"
if [ "$FAILED" -eq 0 ]; then
  printf "RESULT: ALL SUITES PASSED (%'d total checks in %ds)\n" "$TOTAL_CHECKS" "$TOTAL_TIME"
  printf "====================================================\n"
  exit 0
else
  printf "RESULT: ONE OR MORE SUITES FAILED\n"
  printf "====================================================\n"
  exit 1
fi
