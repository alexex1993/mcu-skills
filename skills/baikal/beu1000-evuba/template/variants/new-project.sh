#!/usr/bin/env sh
# Развернуть проект для BE-U1000 на отладочной плате EVU-BA-2.1.
#
#   ./new-project.sh <каталог> [--full | --minimal]
#
#   --full     (по умолчанию) консоль UART0, временная база на TIM0, захват
#              фронта на PWMA1, аппаратно таймированный импульс на PWMA2,
#              аналоговые входы через DMA. Консольное меню по UART0.
#   --minimal  только мигание светодиода LD1. Наименьшее, что доказывает
#              работоспособность тулчейна, образа и прошивки.
#
# Оба варианта собираются как есть:
#   cd <каталог> && make BAIKAL_SDK=$HOME/путь/к/SDK_2_1
#
# Ничего не генерируется и не подставляется: копирование дерева руками даёт
# тот же результат.
set -eu

SRC=$(cd "$(dirname "$0")/.." && pwd)
DEST=${1:-}
MODE=${2:---full}

[ -n "$DEST" ] || { echo "использование: $0 <каталог> [--full|--minimal]" >&2; exit 1; }
[ -e "$DEST" ] && { echo "ошибка: $DEST уже существует" >&2; exit 1; }

mkdir -p "$DEST"
cp -R "$SRC"/. "$DEST"/
rm -rf "$DEST/variants" "$DEST/output"

if [ "$MODE" = "--minimal" ]; then
    rm -f "$DEST"/src/console.c   "$DEST"/include/console.h  \
          "$DEST"/src/timebase.c  "$DEST"/include/timebase.h \
          "$DEST"/src/capture.c   "$DEST"/include/capture.h  \
          "$DEST"/src/evout.c     "$DEST"/include/evout.h    \
          "$DEST"/src/adc_dma.c   "$DEST"/include/adc_dma.h
    cp "$SRC"/variants/minimal/app.c "$DEST"/src/app.c
elif [ "$MODE" != "--full" ]; then
    echo "ошибка: неизвестный режим $MODE" >&2; exit 1
fi

NAME=$(basename "$DEST")
echo "Создан $DEST ($MODE)"
echo "  cd $DEST && make BAIKAL_SDK=\$HOME/путь/к/SDK_2_1"
echo "  прошивка: baikal_uart_flash.py output/debug/$NAME.bin"
