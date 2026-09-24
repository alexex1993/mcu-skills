#!/usr/bin/env bash
# Scaffold a PlatformIO + ESP-IDF project for the Guition JC4880P443C_I_W.
#
#   new-project.sh <target-dir> [--full|--minimal]
#
# --full (default)  board self-test: chip + silicon revision, measured clock,
#                   flash, memory, benchmarks, die temperature, I2C scan,
#                   ES8311 id, ST7701 display, GT911 touch drawing, backlight
#                   on the BOOT button, battery voltage
# --minimal         toolchain + silicon revision + USB console + PSRAM check,
#                   and the backlight blinking on GPIO23 — no display driver,
#                   no component-manager dependencies
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
    cp "$TEMPLATE/include/app.h" "$TEMPLATE/include/font5x7.h" "$TARGET/include/"
    cp "$TEMPLATE/src/idf_component.yml" "$TARGET/src/"
    for f in main.c display.c touch.c i2c_bus.c battery.c board_report.c; do
      cp "$TEMPLATE/src/$f" "$TARGET/src/$f"
    done
    ;;
  *)
    echo "error: unknown variant '$VARIANT' (use --full or --minimal)" >&2
    exit 2
    ;;
esac

echo "scaffolded $VARIANT into $TARGET"
echo "  cd $TARGET && pio run -t upload -t monitor"
