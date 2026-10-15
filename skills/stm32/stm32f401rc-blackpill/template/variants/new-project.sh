#!/usr/bin/env sh
# Scaffold a PlatformIO project for the STM32F401RCT6 Black Pill
# (WeAct HID bootloader on board).
#
#   ./new-project.sh <target-dir> [--full | --minimal]
#
#   --full     (default) blink + USB CDC debug console: printf() to
#              /dev/cu.usbmodem*, banner with clocks/UID/reset cause,
#              heartbeat, KEY events, 'i'/'r' commands.
#   --minimal  blink only (PC13 2 Hz, 10 Hz while KEY is held), no USB.
#
# Both: 84 MHz from the 25 MHz crystal, linked at 0x08004000 behind the
# bootloader. Build as-is:  cd <target-dir> && pio run
set -eu

SRC=$(cd "$(dirname "$0")/.." && pwd)
DEST=${1:-}
MODE=${2:---full}

[ -n "$DEST" ] || { echo "usage: $0 <target-dir> [--full|--minimal]" >&2; exit 1; }
[ -e "$DEST" ] && { echo "error: $DEST already exists" >&2; exit 1; }

mkdir -p "$DEST"
cp -R "$SRC"/. "$DEST"/
rm -rf "$DEST/variants" "$DEST/.pio" "$DEST/scripts/__pycache__"

if [ "$MODE" = "--minimal" ]; then
    rm -f "$DEST"/src/usb_device.c "$DEST"/src/usbd_cdc_if.c \
          "$DEST"/src/usbd_conf.c "$DEST"/src/usbd_desc.c \
          "$DEST"/include/usb_device.h "$DEST"/include/usbd_cdc_if.h \
          "$DEST"/include/usbd_conf.h "$DEST"/include/usbd_desc.h
    cp "$SRC"/variants/minimal/main.c "$DEST"/src/main.c
elif [ "$MODE" != "--full" ]; then
    echo "error: unknown mode $MODE" >&2; exit 1
fi

echo "Created $DEST ($MODE)"
echo "  cd $DEST && pio run"
echo "  flash: hold KEY, tap NRST, release KEY (PC13 flickers, then breathes), then: pio run -t upload"
