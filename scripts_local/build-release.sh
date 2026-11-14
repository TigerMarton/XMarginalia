#!/bin/bash
# Build release (or dev) images once for Xteink X4 Pro and/or Onyx BOOX Picco.
#
# Usage:
#   ./scripts_local/build-release.sh                # builds both gh_release_x4pro and gh_release_picco
#   ./scripts_local/build-release.sh x4pro          # builds gh_release_x4pro only
#   ./scripts_local/build-release.sh picco          # builds gh_release_picco only
#   ./scripts_local/build-release.sh --dev [target] # builds dev env(s) (x4pro / picco) instead of gh_release_*
#
# NOTE: PlatformIO wipes unrequested .pio/build/<env> folders on some clean steps,
# so building "all" (the default) passes both environments in a single `pio run` call.
set -euo pipefail

SCRIPTS_DIR="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)"
REPO="$(cd "$SCRIPTS_DIR/.." && pwd)"
cd "$REPO"

PIO_CORE="${PLATFORMIO_CORE_DIR:-$REPO/.platformio}"
PIO_BIN="$PIO_CORE/penv/bin/pio"
if [ ! -x "$PIO_BIN" ]; then
  PIO_BIN="$(command -v pio || true)"
fi
if [ -z "$PIO_BIN" ] || [ ! -x "$PIO_BIN" ]; then
  echo "error: PlatformIO CLI (pio) not found at $PIO_CORE/penv/bin/pio or in PATH" >&2
  exit 1
fi

MODE="release"
TARGET="all"

while [ $# -gt 0 ]; do
  case "$1" in
    --dev)
      MODE="dev"
      shift
      ;;
    --release)
      MODE="release"
      shift
      ;;
    x4pro | picco | all)
      TARGET="$1"
      shift
      ;;
    -h | --help)
      sed -n '2,12p' "$0"
      exit 0
      ;;
    *)
      echo "error: unknown argument '$1' (expected: x4pro | picco | all | --dev | --release)" >&2
      exit 2
      ;;
  esac
done

if [ "$MODE" = "release" ]; then
  ENV_X4PRO="gh_release_x4pro"
  ENV_PICCO="gh_release_picco"
else
  ENV_X4PRO="x4pro"
  ENV_PICCO="picco"
fi

ENV_ARGS=()
case "$TARGET" in
  x4pro)
    ENV_ARGS=(-e "$ENV_X4PRO")
    ;;
  picco)
    ENV_ARGS=(-e "$ENV_PICCO")
    ;;
  all)
    ENV_ARGS=(-e "$ENV_X4PRO" -e "$ENV_PICCO")
    ;;
esac

echo "==> Building ${ENV_ARGS[*]} in $REPO ..."
PLATFORMIO_CORE_DIR="$PIO_CORE" "$PIO_BIN" run "${ENV_ARGS[@]}"

echo ""
echo "==> Running Dual-Device Partition Safety Check ..."
"$SCRIPTS_DIR/check-partitions.sh"

echo ""
echo "==> Ready-to-flash artifacts:"
if [ "$TARGET" = "x4pro" ] || [ "$TARGET" = "all" ]; then
  ls -lh ".pio/build/$ENV_X4PRO/firmware.bin" ".pio/build/$ENV_X4PRO/firmware.factory.bin" 2>/dev/null || true
fi
if [ "$TARGET" = "picco" ] || [ "$TARGET" = "all" ]; then
  ls -lh ".pio/build/$ENV_PICCO/firmware.bin" 2>/dev/null || true
fi
echo ""
echo "Flash without rebuilding using:"
echo "  ./scripts_local/flash-device.sh           # auto-detects plugged-in X4 Pro or Picco"
echo "  ./scripts_local/flash-device.sh x4pro     # flashes X4 Pro"
echo "  ./scripts_local/flash-device.sh picco     # flashes BOOX Picco (0xE0000 app-only)"
