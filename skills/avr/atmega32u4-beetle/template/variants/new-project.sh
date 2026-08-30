#!/usr/bin/env bash
# Scaffold a Beetle (ATmega32U4 / Mini Arduino Leonardo) project from this
# template.
#
#   ./new-project.sh <target-dir> [--full|--minimal|--hid]
#
#   --full     (default) LED heartbeat + USB-CDC report + ADC + EEPROM + free RAM
#   --minimal  blink the onboard D13 LED, nothing else — flash this first
#   --hid      USB keyboard with an arm pin and a boot grace window
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
    --hid)     VARIANT=hid ;;
    -h|--help) sed -n '2,11p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*)        echo "unknown option: $arg" >&2; exit 2 ;;
    *)         TARGET="$arg" ;;
  esac
done

[ -n "$TARGET" ] || { echo "usage: $0 <target-dir> [--full|--minimal|--hid]" >&2; exit 2; }
[ -e "$TARGET" ] && { echo "$TARGET already exists" >&2; exit 1; }

mkdir -p "$TARGET"
cp -R "$TEMPLATE/src" "$TEMPLATE/include" "$TARGET/"
cp "$TEMPLATE/platformio.ini" "$TEMPLATE/.gitignore" "$TARGET/"

case "$VARIANT" in
  minimal)
    cp "$TEMPLATE/variants/minimal/main.cpp" "$TARGET/src/main.cpp"
    ;;
  hid)
    cp "$TEMPLATE/variants/hid/main.cpp" "$TARGET/src/main.cpp"
    # Keyboard/Mouse are not bundled with PlatformIO's Arduino AVR core.
    printf '\nlib_deps =\n    arduino-libraries/Keyboard\n' >> "$TARGET/platformio.ini"
    ;;
esac

echo "scaffolded $VARIANT project in $TARGET"
echo "next:  cd $TARGET && pio run -t upload -t monitor"
