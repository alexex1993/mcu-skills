# Reference project — WeAct MiniSTM32H7xx (STM32H750VBT6)

A complete, working PlatformIO + STM32Cube HAL firmware for this board. It builds clean and flashes
over DFU; it is the source every code block in `../reference/recipes.md` was extracted from. Use it
as a starting point, or as the answer to "how is this actually wired up on this board".

## Scaffold a new project

```sh
variants/new-project.sh ~/path/to/NewProject            # full reference firmware
variants/new-project.sh ~/path/to/NewProject --minimal  # LED + button only
cd ~/path/to/NewProject && pio run
```

Flash it: hold **BOOT0**, tap **NRST**, release after ~0.5 s (the board enumerates as `0483:DF11`),
then `pio run -t upload`. The board never resets itself into DFU — that sequence is manual every
time. With the full variant, `pio device monitor` then gives you a USB CDC console on the same cable.

Copying the tree by hand works just as well — there is nothing generated, no absolute paths, and no
project name embedded anywhere.

## What each variant contains

| | `--minimal` | `--full` |
|---|---|---|
| Flash used | 5.7 KB (4.3%) | 40.9 KB (31.2%) |
| RAM used | 48 B | 11.4 KB |
| Clock tree, cache, startup order | ✅ | ✅ |
| LED (PE3) + K1 (PC13) | ✅ | ✅ |
| ST7735 display, backlight PWM | — | ✅ |
| USB CDC console | — | ✅ |
| ADC3 internal temperature sensor | — | ✅ |

Both share the same `SystemClock_Config()`, the same startup order and the same `board.h`.

## Layout

```
platformio.ini              board def size fixes, HSE_VALUE, DFU, monitor - see reference §12
include/board.h             pin names, port handles, Error_Handler  <- the file you keep reusing
include/{lcd,temp_sensor,usb*}.h
src/main.c                  startup, clocks, MX_*_Init, the 120 Hz UI loop
src/stm32h7xx_hal_msp.c     peripheral clock sources + pin AF (SPI4 kernel clock, ADC3 PLL3)
src/stm32h7xx_it.c          exception handlers; SysTick drives HAL_GetTick()
src/lcd.c                   ST7735 bus IO, text rendering, fast strip blit
src/temp_sensor.c           ADC3 + VREFINT scaling + factory calibration
src/usb*.c, usbd*.c         USB device stack glue and the CDC class
lib/ST7735/                 vendored panel driver (WeAct's extension of ST's component driver)
lib/USBDeviceCDC/           vendored ST USB device library
variants/                   scaffolding script and the minimal main.c - removed from generated projects
```

## Stripping the full variant down by hand

If you scaffolded `--full` and want to drop a subsystem, the file sets are self-contained:

- **Display**: delete `src/lcd.c`, `include/lcd.h`, `lib/ST7735/`; remove `MX_SPI4_Init()`,
  `MX_TIM1_Init()`, `LCD_*` calls and the `hspi4`/`htim1` definitions from `main.c`.
- **USB CDC**: delete `src/usb_device.c`, `src/usbd_*.c`, the matching headers and
  `lib/USBDeviceCDC/`; remove `MX_USB_DEVICE_Init()` and `CDC_Transmit_FS()` from `main.c`, and the
  `OTG_FS_IRQHandler` from `src/stm32h7xx_it.c`.
- **Temperature**: delete `src/temp_sensor.c`, `include/temp_sensor.h`; remove `MX_ADC3_Init()` and
  `TempSensor_ReadDeciC()` from `main.c` and `HAL_ADC_MspInit/MspDeInit` from the MSP file.

Keep in every case: `platformio.ini`, `include/board.h`, `src/main.c`,
`src/stm32h7xx_hal_msp.c`, `src/stm32h7xx_it.c`.

## What the full variant does when running

Ramps the backlight in, then runs a 120 Hz frame loop paced off the DWT cycle counter: a
millisecond tick counter and an fps/render-time readout repaint every frame, the die temperature
every 500 ms, and a status line goes out over USB CDC every 5 s. It is a deliberate stress case for
the display path — see `../reference/recipes.md` §9 for why the stock driver cannot keep up with it.

Two settings in it are deliberately outside published specs and work on this particular board:
SPI4 at 30 MHz (`SPI_BAUDRATEPRESCALER_4`; the ST7735S spec implies 15 MHz) and FRMCTR1 raised to
~130 Hz. Both are one-line reverts if a panel misbehaves.
