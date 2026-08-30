#!/usr/bin/env bash
# Scaffold a PlatformIO + Arduino project for the 40-pin ESP32-S3-WROOM CAM board.
#
#   new-project.sh <target-dir> [--full|--minimal]
#
# --full (default)  board self-test: report + PSRAM check, microSD over SDMMC,
#                   camera, BOOT button takes a JPEG onto the card
# --minimal         LED + WS2812 blink, console heartbeat, PSRAM check
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
for f in platformio.ini partitions.csv .gitignore; do
  cp "$TEMPLATE/$f" "$TARGET/$f"
done
cp "$TEMPLATE/include/board.h" "$TARGET/include/board.h"

case "$VARIANT" in
  --minimal)
    cp "$TEMPLATE/variants/minimal/main.cpp" "$TARGET/src/main.cpp"
    ;;
  --full|"")
    cp "$TEMPLATE/include/app.h" "$TARGET/include/app.h"
    cp "$TEMPLATE/src/main.cpp" "$TEMPLATE/src/board_report.cpp" \
       "$TEMPLATE/src/camera.cpp" "$TEMPLATE/src/sdcard.cpp" "$TARGET/src/"
    ;;
  *)
    echo "error: unknown variant '$VARIANT' (use --full or --minimal)" >&2
    exit 2
    ;;
esac

echo "scaffolded $VARIANT into $TARGET"
echo "  cd $TARGET && pio run -t upload -t monitor"
