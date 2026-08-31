# Сборка шаблона для BE-U1000 на отладочной плате EVU-BA-2.1.
#
#   make BAIKAL_SDK=~/путь/к/SDK_2_1                       # образ под eFlash
#   MEM_REG_ROM=TCMA MEM_REG_RAM=TCMB make BAIKAL_SDK=...  # образ под TCM
#   make clean BAIKAL_SDK=...
#
# Собирает система сборки SDK Байкала: она знает про три ядра, их системы
# команд и карту памяти, и повторять это у себя незачем.
#
# Путь к SDK задаётся переменной окружения, а не зашит в файл: у каждого он
# свой. Нужный каталог — тот, внутри которого лежат `Drivers/`, `BSP/`, `Tools/`.
#
# ⚠ Область памяти задаётся переменными окружения, а не отдельной целью:
# `common.mk` читает их через `?=`, а рекурсивный вызов make ломает служебные
# переменные сборки (`AS` остаётся пустым, и ассемблирование падает с
# «command not found»).
#
# ⚠ Образ не позиционно-независим: собранный под eFlash в TCM не пойдёт и
# наоборот. И **перед сменой области обязателен `make clean`**: `common.mk`
# берёт уже сгенерированный `output/debug/generated.ld` как есть, поэтому
# смена `MEM_REG_ROM` молча не действует — сборка проходит, а `ORIGIN` в
# скрипте остаётся прежним.

BAIKAL_SDK ?=
ifeq ($(BAIKAL_SDK),)
$(error Задайте BAIKAL_SDK — путь к SDK Байкала (каталог с Drivers/, BSP/, Tools/). \
Например: make BAIKAL_SDK=$$HOME/Baikal_U1000/SDK_2_1)
endif

SDK_DIR := $(BAIKAL_SDK)

# Плата. Значение проверяется `common.mk` по списку каталогов в `BSP/`.
BOARD = EVU_BA_2_1

# 0 — никакого автопоиска: исходники перечислены ниже поимённо.
# При автопоиске (значение по умолчанию 1) в сборку попал бы и
# `variants/minimal/`, то есть второй `main()`.
SRC_AUTOSEARCH = 0

INC_DIR += $(CURDIR)/include

# Всё, что лежит в `src/`. Список не перечисляется поимённо намеренно: так
# вариант `--minimal`, у которого файлов меньше, собирается тем же Makefile.
CSRC += $(wildcard $(CURDIR)/src/*.c)

# Модули HAL, которые использует шаблон. Автоподбора у SDK нет: не перечислишь —
# получишь неразрешённые ссылки на этапе компоновки.
CSRC += $(SDK_DIR)/Drivers/HAL/Src/bmcu_cru.c
CSRC += $(SDK_DIR)/Drivers/HAL/Src/bmcu_gpio.c
CSRC += $(SDK_DIR)/Drivers/HAL/Src/bmcu_uart.c
CSRC += $(SDK_DIR)/Drivers/HAL/Src/bmcu_tim.c
CSRC += $(SDK_DIR)/Drivers/HAL/Src/bmcu_pwma.c
CSRC += $(SDK_DIR)/Drivers/HAL/Src/bmcu_adc.c
CSRC += $(SDK_DIR)/Drivers/HAL/Src/bmcu_dma.c

# Заглушки системных вызовов из шаблона SDK. Без них компоновщик не разрешает
# ссылки newlib (`_exit`, `_write`, `_sbrk` и прочие), которые тянет за собой
# стандартная библиотека.
#
# ⚠ Берётся именно один файл, а не каталог: рядом с ним лежит `main.c` самого
# SDK, и он бы состязался с нашим за точку входа.
CSRC += $(SDK_DIR)/Projects/_template/syscalls.c

.DEFAULT_GOAL := all

include $(SDK_DIR)/Tools/build/common.mk

# ⚠ Слежение за заголовками в `common.mk` сломано именами: `DEPS` собирается
# как `app.d`, а компилятор пишет `app.o.d`, и `-include` молча берёт пустоту.
# Без этой строки правка `board.h` не вызывает пересборку — и на плате
# отказывает не то, что менялось.
-include $(wildcard $(OBJ_DIR)/*.o.d)
