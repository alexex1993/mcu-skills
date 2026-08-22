#!/usr/bin/env sh
# Scaffold a PlatformIO project for the WeAct "Black Pill" (STM32F411CEU6).
#
#   ./new-project.sh <target-dir> [--full | --minimal]
#
#   --full     (default) the complete reference firmware: LSE-backed RTC, USB
#              CDC console with echo, calibrated internal temperature sensor,
#              LED and K1. ~18 KB of the 512 KB flash.
#   --minimal  LED blink + K1 only. Same clock tree, same startup order, no USB.
#              The smallest thing that proves toolchain, crystal and DFU work.
#
# Both variants build as-is:  cd <target-dir> && pio run
set -eu

SRC=$(cd "$(dirname "$0")/.." && pwd)
DEST=${1:-}
MODE=${2:---full}

[ -n "$DEST" ] || { echo "usage: $0 <target-dir> [--full|--minimal]" >&2; exit 1; }
[ -e "$DEST" ] && { echo "error: $DEST already exists" >&2; exit 1; }

mkdir -p "$DEST"
cp -R "$SRC"/. "$DEST"/
rm -rf "$DEST/variants"

if [ "$MODE" = "--minimal" ]; then
    rm -f  "$DEST"/src/temp_sensor.c \
           "$DEST"/src/usb_device.c "$DEST"/src/usbd_cdc_if.c \
           "$DEST"/src/usbd_conf.c "$DEST"/src/usbd_desc.c
    rm -f  "$DEST"/include/temp_sensor.h \
           "$DEST"/include/usb_device.h "$DEST"/include/usbd_cdc_if.h \
           "$DEST"/include/usbd_conf.h "$DEST"/include/usbd_desc.h
    rm -rf "$DEST"/lib/USBDevice
    cp "$SRC"/variants/minimal/main.c "$DEST"/src/main.c
elif [ "$MODE" != "--full" ]; then
    echo "error: unknown mode $MODE" >&2; exit 1
fi

echo "Created $DEST ($MODE)"
echo "  cd $DEST && pio run"
echo "  flash: hold BOOT0, tap NRST, release, then: pio run -t upload"
