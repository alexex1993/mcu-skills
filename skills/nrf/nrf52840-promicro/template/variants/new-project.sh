#!/usr/bin/env bash
# Scaffold a ProMicro nRF52840 (V1940 / nice!nano v2 clone) project.
#
#   ./new-project.sh <target-dir> [--full|--minimal|--ble]
#
#   --full     (default) LED heartbeat + USB-CDC report + 12-bit ADC on A1
#              + die temperature
#   --minimal  blink the on-board LED, nothing else, no USB stack
#   --ble      LED heartbeat + BLE Nordic UART Service bridged to USB CDC
#
# The vendored board definition (boards/) is copied in every case — the board
# does not exist in PlatformIO and the project will not build without it.
#
# Nothing is generated and no paths are embedded: copying template/ by hand
# and swapping src/main.cpp gives exactly the same result.
set -euo pipefail

TEMPLATE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET=""
VARIANT=full

for arg in "$@"; do
  case "$arg" in
    --full)    VARIANT=full ;;
    --minimal) VARIANT=minimal ;;
    --ble)     VARIANT=ble ;;
    -h|--help) sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*)        echo "unknown option: $arg" >&2; exit 2 ;;
    *)         TARGET="$arg" ;;
  esac
done

[ -n "$TARGET" ] || { echo "usage: $0 <target-dir> [--full|--minimal|--ble]" >&2; exit 2; }
[ -e "$TARGET" ] && { echo "$TARGET already exists" >&2; exit 1; }

mkdir -p "$TARGET"
cp -R "$TEMPLATE/src" "$TEMPLATE/include" "$TEMPLATE/boards" "$TARGET/"
cp "$TEMPLATE/platformio.ini" "$TEMPLATE/.gitignore" "$TARGET/"

case "$VARIANT" in
  minimal) cp "$TEMPLATE/variants/minimal/main.cpp" "$TARGET/src/main.cpp" ;;
  ble)     cp "$TEMPLATE/variants/ble/main.cpp"     "$TARGET/src/main.cpp" ;;
esac

echo "scaffolded $VARIANT project in $TARGET"
echo "next:  cd $TARGET && pio run"
echo "       double-tap RESET, then: pio run -t upload -t monitor"
