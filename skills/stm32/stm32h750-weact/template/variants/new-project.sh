#!/usr/bin/env sh
# Scaffold a PlatformIO project for the WeAct MiniSTM32H7xx (STM32H750VBT6).
#
#   ./new-project.sh <target-dir> [--full | --minimal]
#
#   --full     (default) the complete reference firmware: ST7735 UI, USB CDC
#              console, internal temperature sensor. Builds to ~40 KB of the
#              128 KB flash.
#   --minimal  LED blink + K1 only. Same clock tree and startup order, nothing
#              else -- the smallest thing that proves toolchain, clocks and
#              flashing all work.
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
    rm -f  "$DEST"/src/lcd.c "$DEST"/src/temp_sensor.c \
           "$DEST"/src/usb_device.c "$DEST"/src/usbd_cdc_if.c \
           "$DEST"/src/usbd_conf.c "$DEST"/src/usbd_desc.c
    rm -f  "$DEST"/include/lcd.h "$DEST"/include/temp_sensor.h \
           "$DEST"/include/usb_device.h "$DEST"/include/usbd_cdc_if.h \
           "$DEST"/include/usbd_conf.h "$DEST"/include/usbd_desc.h
    rm -rf "$DEST"/lib/ST7735 "$DEST"/lib/USBDeviceCDC
    cp "$SRC"/variants/minimal/main.c          "$DEST"/src/main.c
    cp "$SRC"/variants/minimal/stm32h7xx_it.c  "$DEST"/src/stm32h7xx_it.c
elif [ "$MODE" != "--full" ]; then
    echo "error: unknown mode $MODE" >&2; exit 1
fi

echo "Created $DEST ($MODE)"
echo "  cd $DEST && pio run"
echo "  flash: hold BOOT0, tap NRST, release after ~0.5 s, then: pio run -t upload"
