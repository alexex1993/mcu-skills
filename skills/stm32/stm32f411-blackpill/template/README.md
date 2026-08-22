# Reference project — WeAct "Black Pill" V3.x (STM32F411CEU6)

A complete, working PlatformIO + STM32Cube HAL firmware for this board. It builds clean with `-Wall`
and flashes over the board's own USB-C through the ROM DFU bootloader; it is the source every code
block in `../reference/recipes.md` was extracted from.

## Scaffold a new project

```sh
variants/new-project.sh ~/path/to/NewProject            # full reference firmware
variants/new-project.sh ~/path/to/NewProject --minimal  # LED + K1 only
cd ~/path/to/NewProject && pio run
```

Flash it: hold **BOOT0**, tap **NRST**, release → the board enumerates as `0483:DF11` → `pio run -t
upload`. PlatformIO passes `:leave` to dfu-util, so the board restarts into the new firmware by
itself; only *entering* DFU is manual. With the full variant, `pio device monitor` then gives you a
USB CDC console on that same cable — this board has no USB-serial chip, so that console *is* the
serial port.

Copying the tree by hand works just as well — nothing is generated, no absolute paths, no project
name embedded anywhere.

## What each variant contains

| | `--minimal` | `--full` |
|---|---|---|
| Flash used | 3476 B (0.7%) | 19808 B (3.8%) |
| RAM used | 44 B | 4840 B |
| Clock tree (96/48/96 MHz), startup order | ✅ | ✅ |
| LED (PC13, active low) + K1 (PA0, active low) | ✅ | ✅ |
| RTC on the 32.768 kHz LSE crystal, VBAT-backed | — | ✅ |
| USB CDC console with echo | — | ✅ |
| Internal temperature sensor + VREFINT, factory-calibrated | — | ✅ |

Both share the same `SystemClock_Config()`, the same `SysTick_Handler()` and the same `board.h`.

## Layout

```
platformio.ini              board, HSE_VALUE, DFU, monitor - see reference §12
include/board.h             pin names with polarity, Error_Handler  <- the file you keep reusing
include/{temp_sensor,usb_device,usbd_*}.h
src/main.c                  startup order, clocks, RTC, the main loop, SysTick_Handler
src/temp_sensor.c           ADC1 ch18 + VREFINT, scaled with TS_CAL1/TS_CAL2
src/usb_device.c            core + CDC class + descriptors, assembled
src/usbd_cdc_if.c           the CDC read/write side, DTR tracking, echo
src/usbd_conf.c             PCD <-> USBD glue, MSP, FIFO split, OTG_FS_IRQHandler
src/usbd_desc.c             descriptors; serial number built from the die UID
lib/USBDevice/              vendored ST USB Device Library core + CDC class
variants/                   scaffold script and the minimal main.c - removed from generated projects
```

There is no `stm32f4xx_hal_msp.c` and no `stm32f4xx_it.c`: on this board the only MSP work is USB's,
which lives in `usbd_conf.c` next to the peripheral it serves, and the only interrupt handlers needed
are `SysTick_Handler` (in `main.c`) and `OTG_FS_IRQHandler` (in `usbd_conf.c`). Add a proper
`stm32f4xx_it.c` as soon as a second peripheral needs one.

## Vendored code

`lib/USBDevice/` is ST's USB Device Library (`usbd_core`, `usbd_ctlreq`, `usbd_ioreq`, `usbd_cdc`),
copyright STMicroelectronics, under the terms in the headers of those files. It has to be vendored:
`framework-stm32cubef4` ships the HAL and CMSIS but **not** the USB middleware, so there is nothing
to `#include` otherwise. The files are unmodified.

## Stripping the full variant down by hand

- **USB CDC**: delete `src/usb_device.c`, `src/usbd_*.c`, the matching headers and `lib/USBDevice/`;
  remove `MX_USB_DEVICE_Init()`, `cdc_printf()` and the `CDC_PortOpen` block from `main.c`.
- **Temperature**: delete `src/temp_sensor.c`, `include/temp_sensor.h`; remove `TempSensor_Init()`
  and the `SENSE_PERIOD_MS` block from `main.c`.
- **RTC**: remove `RTC_ClockSource_Init()`, `RTC_Init()`, `print_time()` and `hrtc` from `main.c`.
  The LSE crystal stays unused; PC14/PC15 remain unusable as GPIO while it is fitted.

Keep in every case: `platformio.ini`, `include/board.h`, and `main.c`'s `SystemClock_Config()`,
`LED_GPIO_Init()`, `Error_Handler()` and `SysTick_Handler()`.

## What the full variant does when running

Blinks PC13 at 5 Hz, prints the RTC time with milliseconds every 200 ms, the die temperature and
measured VDDA every second, and a line on every K1 press. Typed input is echoed. The banner reprints
whenever the host opens the port, so starting a monitor mid-run still shows the clock tree and the
die's unique ID.

## Verification status

Hardware-verified on a WeAct V3.x board: clock tree, LSE-backed RTC (including surviving a reset
without reseeding), USB CDC enumeration and echo, DFU flashing, PC13 polarity.

Build-verified only: the temperature sensor and VREFINT module, and K1 on PA0 — the latter is the
documented WeAct V3.x wiring, not something read off a schematic in this repo. If K1 reads stuck,
check `reference/board-hardware.md` §5.1 before assuming the code is wrong.
