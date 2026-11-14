#!/bin/bash
# Instant, zero-rebuild Linux USB flasher for Xteink X4 Pro and Onyx BOOX Picco.
#
# By default, flashes the already-built image in .pio/build/ directly via esptool.py
# in ~8 seconds WITHOUT triggering a PlatformIO rebuild.
#
# Usage:
#   ./scripts_local/flash-device.sh                    # auto-detects connected device via 0x8000 partition table!
#   ./scripts_local/flash-device.sh picco              # flashes BOOX Picco (0xE0000 app-only)
#   ./scripts_local/flash-device.sh x4pro              # flashes Xteink X4 Pro (0x0 firmware.factory.bin by default)
#   ./scripts_local/flash-device.sh x4pro --app-only   # flashes Xteink X4 Pro at 0xe000+0x10000 (preserves NVS Wi-Fi!)
#   ./scripts_local/flash-device.sh [target] --build   # builds the target first, then flashes
#   ./scripts_local/flash-device.sh [target] --dev     # uses dev env (x4pro / picco) instead of gh_release_*
#   ./scripts_local/flash-device.sh [target] --port /dev/ttyACM0 --dry-run
set -euo pipefail

SCRIPTS_DIR="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)"
REPO="$(cd "$SCRIPTS_DIR/.." && pwd)"
cd "$REPO"

PIO_CORE="${PLATFORMIO_CORE_DIR:-$REPO/.platformio}"
PYTHON_BIN="$PIO_CORE/penv/bin/python"
if [ ! -x "$PYTHON_BIN" ]; then
  PYTHON_BIN="$(command -v python3 || true)"
fi
ESPTOOL_PY="$PIO_CORE/packages/tool-esptoolpy/esptool.py"
if [ ! -f "$ESPTOOL_PY" ]; then
  echo "error: esptool.py not found at $ESPTOOL_PY" >&2
  exit 1
fi

TARGET="auto"
MODE="release"
X4PRO_MODE="factory" # "factory" (0x0 firmware.factory.bin) or "app-only" (0xe000 + 0x10000 firmware.bin)
DO_BUILD=0
DRY_RUN=0
SKIP_VERIFY=0
PORT=""
BAUD="460800"

while [ $# -gt 0 ]; do
  case "$1" in
    auto | x4pro | picco)
      TARGET="$1"
      shift
      ;;
    --dev)
      MODE="dev"
      shift
      ;;
    --release)
      MODE="release"
      shift
      ;;
    --factory)
      X4PRO_MODE="factory"
      shift
      ;;
    --app-only)
      X4PRO_MODE="app-only"
      shift
      ;;
    --build | -b)
      DO_BUILD=1
      shift
      ;;
    --dry-run | -n)
      DRY_RUN=1
      shift
      ;;
    --skip-verify)
      SKIP_VERIFY=1
      shift
      ;;
    --port | -p)
      PORT="${2:-}"
      [ -n "$PORT" ] || { echo "error: --port requires a device path (e.g. /dev/ttyACM0)" >&2; exit 2; }
      shift 2
      ;;
    -h | --help)
      sed -n '2,15p' "$0"
      exit 0
      ;;
    *)
      echo "error: unknown argument '$1'" >&2
      sed -n '2,15p' "$0" >&2
      exit 2
      ;;
  esac
done

detect_serial_port() {
  if [ -n "$PORT" ]; then
    printf '%s' "$PORT"
    return 0
  fi

  # 1. Prefer Linux /dev/serial/by-id containing Espressif or XMarginalia/CrossPoint
  local by_id
  for by_id in /dev/serial/by-id/*Espressif* /dev/serial/by-id/*XMarginalia* /dev/serial/by-id/*CrossPoint*; do
    if [ -e "$by_id" ]; then
      readlink -f "$by_id"
      return 0
    fi
  done

  # 2. Fall back to /dev/ttyACM* or /dev/ttyUSB* on Linux, or /dev/cu.usbmodem* on macOS
  local dev
  for dev in /dev/ttyACM0 /dev/ttyACM1 /dev/ttyUSB0 /dev/cu.usbmodem*; do
    if [ -e "$dev" ]; then
      printf '%s' "$dev"
      return 0
    fi
  done

  return 1
}

if ! PORT="$(detect_serial_port)"; then
  if [ "$DRY_RUN" -eq 1 ]; then
    PORT="/dev/ttyACM0"
    echo "[dry-run] No serial device currently attached; assuming $PORT"
  else
    echo "error: no ESP32-S3 USB serial device found (/dev/ttyACM*)." >&2
    echo "  -> Make sure the device is plugged in and awake (or pass --port /dev/ttyACM0)." >&2
    exit 1
  fi
fi

echo "==> Using serial port: $PORT"

# Probe the connected hardware's partition table at 0x8000 (3KB) so we never
# flash an X4 Pro image onto a BOOX Picco or vice versa.
DETECTED_HW="unknown"
if [ "$DRY_RUN" -eq 0 ] && [ "$SKIP_VERIFY" -eq 0 ]; then
  PT_TMP="$(mktemp "${TMPDIR:-/tmp}/esp32s3-pt.XXXXXX")"
  trap 'rm -f "$PT_TMP" "${OTADATA_TMP:-}"' EXIT
  echo "==> Probing partition table on $PORT (0x8000) to verify hardware identity ..."
  if "$PYTHON_BIN" "$ESPTOOL_PY" --chip esp32s3 --port "$PORT" -b "$BAUD" \
       --before usb-reset --after no-reset read-flash 0x8000 0xc00 "$PT_TMP" >/dev/null 2>&1; then
    DETECTED_HW="$("$PYTHON_BIN" - "$PT_TMP" <<'PY'
import sys
data = open(sys.argv[1], "rb").read()
if b"safeboot" in data:
    print("picco")
elif b"app0" in data:
    print("x4pro")
else:
    print("unknown")
PY
)"
    echo "==> Hardware partition signature: $DETECTED_HW"
  else
    echo "warning: could not read partition table at 0x8000 (continuing with target=$TARGET)" >&2
  fi
fi

if [ "$TARGET" = "auto" ]; then
  if [ "$DETECTED_HW" = "picco" ] || [ "$DETECTED_HW" = "x4pro" ]; then
    TARGET="$DETECTED_HW"
    echo "==> Auto-selected target: $TARGET"
  else
    echo "error: could not auto-detect board from partition table. Please specify 'x4pro' or 'picco' explicitly." >&2
    exit 1
  fi
elif [ "$DETECTED_HW" != "unknown" ] && [ "$DETECTED_HW" != "$TARGET" ]; then
  echo "=====================================================================" >&2
  echo "CRITICAL SAFETY ABORT: Connected device has '$DETECTED_HW' partition table," >&2
  echo "but you requested flashing '$TARGET'!" >&2
  echo "Refusing to flash to protect vendor partitions / safeboot." >&2
  echo "=====================================================================" >&2
  exit 1
fi

if [ "$MODE" = "release" ]; then
  ENV_NAME="gh_release_$TARGET"
else
  ENV_NAME="$TARGET"
fi

if [ "$DO_BUILD" -eq 1 ]; then
  if [ "$MODE" = "release" ]; then
    "$SCRIPTS_DIR/build-release.sh" --release "$TARGET"
  else
    "$SCRIPTS_DIR/build-release.sh" --dev "$TARGET"
  fi
fi

BUILD_DIR="$REPO/.pio/build/$ENV_NAME"

if [ "$TARGET" = "picco" ]; then
  # BOOX Picco: NEVER touch 0x0, 0x8000, or 0xe000. Only write firmware.bin at 0xE0000 (ota_0).
  FW_BIN="$BUILD_DIR/firmware.bin"
  if [ ! -f "$FW_BIN" ]; then
    echo "error: pre-built binary not found at $FW_BIN" >&2
    echo "  -> Run './scripts_local/build-release.sh picco' (or pass --build) first." >&2
    exit 1
  fi
  echo "==> Flashing BOOX Picco [$ENV_NAME] app-only at 0xE0000 ($(ls -lh "$FW_BIN" | awk '{print $5}')) ..."
  CMD=(
    "$PYTHON_BIN" "$ESPTOOL_PY"
    --chip esp32s3 --port "$PORT" -b "$BAUD"
    --before usb-reset --after hard-reset
    write-flash 0xE0000 "$FW_BIN"
  )
else
  # Xteink X4 Pro
  if [ "$X4PRO_MODE" = "factory" ]; then
    FW_FACTORY="$BUILD_DIR/firmware.factory.bin"
    if [ ! -f "$FW_FACTORY" ]; then
      echo "error: pre-built factory binary not found at $FW_FACTORY" >&2
      echo "  -> Run './scripts_local/build-release.sh x4pro' (or pass --build) first." >&2
      exit 1
    fi
    echo "==> Flashing Xteink X4 Pro [$ENV_NAME] factory image at 0x0 ($(ls -lh "$FW_FACTORY" | awk '{print $5}')) ..."
    CMD=(
      "$PYTHON_BIN" "$ESPTOOL_PY"
      --chip esp32s3 --port "$PORT" -b "$BAUD"
      --before usb-reset --after hard-reset
      write-flash 0x0 "$FW_FACTORY"
    )
  else
    FW_BIN="$BUILD_DIR/firmware.bin"
    if [ ! -f "$FW_BIN" ]; then
      echo "error: pre-built binary not found at $FW_BIN" >&2
      echo "  -> Run './scripts_local/build-release.sh x4pro' (or pass --build) first." >&2
      exit 1
    fi
    OTADATA_TMP="$(mktemp "${TMPDIR:-/tmp}/otadata-blank.XXXXXX")"
    trap 'rm -f "${PT_TMP:-}" "$OTADATA_TMP"' EXIT
    "$PYTHON_BIN" -c 'import sys; open(sys.argv[1], "wb").write(b"\xff" * 0x2000)' "$OTADATA_TMP"
    echo "==> Flashing Xteink X4 Pro [$ENV_NAME] app-only at 0xe000 + 0x10000 (NVS preserved, $(ls -lh "$FW_BIN" | awk '{print $5}')) ..."
    CMD=(
      "$PYTHON_BIN" "$ESPTOOL_PY"
      --chip esp32s3 --port "$PORT" -b "$BAUD"
      --before usb-reset --after hard-reset
      write-flash 0xe000 "$OTADATA_TMP" 0x10000 "$FW_BIN"
    )
  fi
fi

if [ "$DRY_RUN" -eq 1 ]; then
  echo "[dry-run] Would execute:"
  printf '  %q' "${CMD[@]}"
  echo ""
  exit 0
fi

"${CMD[@]}"
echo "==> Done! $TARGET ($ENV_NAME) flashed successfully."
