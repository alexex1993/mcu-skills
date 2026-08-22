#!/usr/bin/env bash
# Scaffold a PlatformIO + ESP-IDF project for this board.
#
#   new-project.sh <target-dir> [--full|--minimal]
#
# --full (default)  board self-test: chip/reset/strapping report, ADC1 with
#                   calibration, Wi-Fi scan, LEDC heartbeat, BOOT button
# --minimal         LED blink + console heartbeat only
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

mkdir -p "$TARGET"
for f in platformio.ini sdkconfig.defaults partitions.csv CMakeLists.txt .gitignore; do
  cp "$TEMPLATE/$f" "$TARGET/$f"
done
mkdir -p "$TARGET/src" "$TARGET/include"
cp "$TEMPLATE/include/board.h" "$TARGET/include/board.h"

case "$VARIANT" in
  --minimal)
    cp "$TEMPLATE/variants/minimal/main.c" "$TARGET/src/main.c"
    cat > "$TARGET/src/CMakeLists.txt" <<'EOF'
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "." "../include"
    REQUIRES driver
)
EOF
    ;;
  --full|"")
    cp "$TEMPLATE/include/app.h" "$TARGET/include/app.h"
    cp "$TEMPLATE/src/main.c" "$TEMPLATE/src/board_report.c" \
       "$TEMPLATE/src/analog.c" "$TEMPLATE/src/wifi_scan.c" \
       "$TEMPLATE/src/CMakeLists.txt" "$TARGET/src/"
    ;;
  *)
    echo "error: unknown variant '$VARIANT' (use --full or --minimal)" >&2
    exit 2
    ;;
esac

echo "scaffolded $VARIANT into $TARGET"
echo "  cd $TARGET && pio run -t upload -t monitor"
