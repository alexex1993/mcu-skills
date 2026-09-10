#!/usr/bin/env bash
# Scaffold a PlatformIO + Arduino project for the Waveshare ESP32-S3-RLCD-4.2.
#
#   new-project.sh <target-dir> [--full|--minimal]
#
# --full (default)  board self-test: report + PSRAM check, I2C scan, SHTC3,
#                   PCF85063A RTC, battery, dashboard on the reflective panel
# --minimal         USB CDC console heartbeat + PSRAM check, nothing else
#
# Nothing is generated and no paths are embedded — copying template/ by hand
# and deleting what you do not want works identically.
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
for f in platformio.ini .gitignore; do
  cp "$TEMPLATE/$f" "$TARGET/$f"
done
cp "$TEMPLATE/include/board_pins.h" "$TARGET/include/board_pins.h"

case "$VARIANT" in
  --minimal)
    cp "$TEMPLATE/variants/minimal/main.cpp" "$TARGET/src/main.cpp"
    ;;
  --full|"")
    cp "$TEMPLATE/include/app.h" "$TARGET/include/app.h"
    cp "$TEMPLATE/src/main.cpp" "$TEMPLATE/src/board_report.cpp" \
       "$TEMPLATE/src/display.cpp" "$TEMPLATE/src/sensors.cpp" "$TARGET/src/"
    ;;
  *)
    echo "error: unknown variant '$VARIANT' (use --full or --minimal)" >&2
    exit 2
    ;;
esac

echo "scaffolded $VARIANT into $TARGET"
echo "  cd $TARGET && pio run -t upload -t monitor"
