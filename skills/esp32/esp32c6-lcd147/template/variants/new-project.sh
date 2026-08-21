#!/usr/bin/env bash
# Scaffold a Waveshare ESP32-C6-LCD-1.47 project from this template.
#
#   ./new-project.sh <target-dir> [--full|--minimal]
#
#   --full     (default) LCD effect carousel + RGB LED + USB console
#   --minimal  RGB LED + console heartbeat only, no display, no SPI bus
#
# Nothing is generated and no paths are embedded: copying template/ by hand and
# deleting the files you do not want gives exactly the same result.
set -euo pipefail

TEMPLATE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET=""
VARIANT=full

for arg in "$@"; do
  case "$arg" in
    --full)    VARIANT=full ;;
    --minimal) VARIANT=minimal ;;
    -h|--help) sed -n '2,9p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*)        echo "unknown option: $arg" >&2; exit 2 ;;
    *)         TARGET="$arg" ;;
  esac
done

[ -n "$TARGET" ] || { echo "usage: $0 <target-dir> [--full|--minimal]" >&2; exit 2; }
[ -e "$TARGET" ] && { echo "$TARGET already exists" >&2; exit 1; }

mkdir -p "$TARGET"
cp -R "$TEMPLATE/src" "$TEMPLATE/include" "$TEMPLATE/scripts" "$TARGET/"
cp "$TEMPLATE/platformio.ini" "$TEMPLATE/CMakeLists.txt" \
   "$TEMPLATE/sdkconfig.defaults" "$TEMPLATE/.gitignore" "$TARGET/"

if [ "$VARIANT" = minimal ]; then
  rm -f "$TARGET/src/main.c" "$TARGET/src/gfx.c" "$TARGET/src/effects.c" \
        "$TARGET/include/gfx.h" "$TARGET/include/effects.h" \
        "$TARGET/include/font12x24.h"
  rm -rf "$TARGET/scripts"
  cp "$TEMPLATE/variants/minimal/main.c" "$TARGET/src/main.c"
fi

echo "scaffolded $VARIANT project in $TARGET"
echo "next:  cd $TARGET && pio run -t upload -t monitor"
