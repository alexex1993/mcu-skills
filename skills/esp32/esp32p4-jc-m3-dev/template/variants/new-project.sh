#!/usr/bin/env bash
# Scaffold a PlatformIO + ESP-IDF project for the Guition JC-ESP32P4-M3-DEV.
#
#   new-project.sh <target-dir> [--full|--minimal]
#
# --full (default)  board self-test: chip + silicon revision, measured clock,
#                   flash, partitions, memory, PSRAM/SRAM/CPU benchmarks, die
#                   temperature, I2C scan, GPIO read-back, then live telemetry
# --minimal         toolchain + silicon revision + USB console + PSRAM check
#                   and a blink, nothing else
#
# Nothing is generated and no paths are embedded — copying template/ by hand and
# deleting what you do not want works identically.
set -euo pipefail

TEMPLATE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET="${1:-}"
VARIANT="${2:---full}"

if [ -z "$TARGET" ]; then
  echo "usage: $(basename "$0") <target-dir> [--full|--minimal]" >&2
  exit 2
fi
if [ -e "$TARGET" ] && [ -n "$(ls -A "$TARGET" 2>/dev/null)" ]; then
  echo "error: $TARGET exists and is not empty" >&2
  exit 1
fi

mkdir -p "$TARGET/src" "$TARGET/include"
for f in platformio.ini sdkconfig.defaults CMakeLists.txt .gitignore; do
  cp "$TEMPLATE/$f" "$TARGET/$f"
done
cp "$TEMPLATE/src/CMakeLists.txt" "$TARGET/src/CMakeLists.txt"
cp "$TEMPLATE/include/board_pins.h" "$TARGET/include/board_pins.h"

case "$VARIANT" in
  --minimal)
    cp "$TEMPLATE/variants/minimal/main.c" "$TARGET/src/main.c"
    ;;
  --full|"")
    cp "$TEMPLATE/include/app.h" "$TARGET/include/app.h"
    cp "$TEMPLATE/src/main.c" "$TEMPLATE/src/board_report.c" \
       "$TEMPLATE/src/i2c_scan.c" "$TARGET/src/"
    ;;
  *)
    echo "error: unknown variant '$VARIANT' (use --full or --minimal)" >&2
    exit 2
    ;;
esac

echo "scaffolded $VARIANT into $TARGET"
echo "  cd $TARGET && pio run -t upload -t monitor"
