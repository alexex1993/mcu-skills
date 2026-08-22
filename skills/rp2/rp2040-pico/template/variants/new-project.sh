#!/usr/bin/env bash
# Scaffold a Raspberry Pi Pico (RP2040) project from this template.
#
#   ./new-project.sh <target-dir> [--full|--minimal]
#
#   --full     (default) LED heartbeat + USB-CDC report + ADC (A0, die temp,
#              VSYS) + VBUS sense + flash-backed EEPROM boot counter
#   --minimal  classic Blink on GPIO25, nothing else
#
# Nothing is generated and no paths are embedded: copying template/ by hand
# and deleting the files you do not want gives exactly the same result.
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
cp -R "$TEMPLATE/src" "$TEMPLATE/include" "$TARGET/"
cp "$TEMPLATE/platformio.ini" "$TEMPLATE/.gitignore" "$TARGET/"

if [ "$VARIANT" = minimal ]; then
  cp "$TEMPLATE/variants/minimal/main.cpp" "$TARGET/src/main.cpp"
fi

echo "scaffolded $VARIANT project in $TARGET"
echo "next:  cd $TARGET && pio run -t upload -t monitor"
