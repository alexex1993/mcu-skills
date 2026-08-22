---
name: stm32f411-blackpill
description: Firmware development for the WeAct Studio "Black Pill" (STM32F411CEU6) and its clones such as the DFRobot DFR0864 — the 25 MHz HSE and 32.768 kHz LSE crystals, PC13 LED, K1 button, USB-C wired straight to USB_OTG_FS, ROM DFU flashing, the SPI-flash footprint, and the STM32Cube HAL + PlatformIO setup around them. Use when working on this board or any STM32F411CE/F401CC Black Pill: project setup, platformio.ini, clock and PLL configuration for 96 MHz plus 48 MHz USB, pin mapping on the 48-pin package, USB CDC console, RTC on the LSE crystal, the internal temperature sensor, ADC accuracy, DFU flashing and recovery, or debugging why something on the board does not work.
---

# WeAct "Black Pill" — STM32F411CEU6

Board-specific firmware knowledge. The two reference files hold the detail — read the one you need
rather than guessing, because most of the failure modes below are silent (the board runs, just
wrongly).

- `reference/board-hardware.md` — complete hardware reference: pin map for the 48-pin package with
  every alternate function, power tree, memory map, boot modes, **plus** a full development guide
  (Part II: §12 PlatformIO, §13 bring-up order, §14 peripheral cookbook, §15 F4/M4 gotchas,
  §16 flashing and recovery, §17 pitfall table).
- `reference/recipes.md` — copy-paste-ready code: `platformio.ini`, `board.h`, the clock tree, RTC on
  the LSE crystal, the calibrated temperature sensor, the complete USB CDC stack.
- `template/` — a **complete working project** that builds and flashes, plus a scaffolding script.
  It is where every recipe was extracted from; see `template/README.md`.

## Orientation

| | |
|---|---|
| MCU | STM32F411CEU6, Cortex-M4F, **512 KB flash, 128 KB SRAM**, UFQFPN48, 100 MHz max |
| Crystals | HSE 25 MHz on PH0/PH1, LSE 32.768 kHz on PC14/PC15 — **both fitted from the factory** |
| Working clock tree | PLL M25/N192/P2/**Q4** → SYSCLK 96 MHz, HCLK 96, PCLK1 48, PCLK2 96, **USB 48** |
| LED / button | PC13 **lit = LOW** (anode on 3V3) / K1 on PA0 **pressed = LOW**, needs a pull-up |
| USB | USB-C straight to USB_OTG_FS on PA11/PA12. **No USB-serial chip on the board** |
| Flashing | ROM DFU over that same USB-C (`0483:DF11`), entered by hand with BOOT0 + NRST |
| Debug | SWD on the 4-pin header: 3V3, PA13 (SWDIO), PA14 (SWCLK), GND |
| ADC | ADC1 only, 10 external channels (PA0–PA7, PB0, PB1). **No DAC.** VDDA = VREF+ = the 3V3 LDO |
| Storage | unpopulated SOIC-8 pad underneath for an SPI flash on SPI1 (PA4–PA7) |

## Rules that prevent the expensive mistakes

Check these before writing code — each one produces a failure that looks like something else.

1. **Define `SysTick_Handler()` yourself.** `framework-stm32cube` ships no `stm32f4xx_it.c`, and the
   startup file's weak handler is an infinite loop. Without
   `void SysTick_Handler(void) { HAL_IncTick(); }` the board boots and then hangs in the first
   `HAL_Delay()` — which reads like a clock problem and is not. This is the single most common way a
   first project on this board fails.
2. **96 MHz, not 100.** USB_OTG_FS needs exactly 48 MHz off PLLQ, which divides the same VCO as PLLP.
   No integer pair gives 100 MHz and 48 MHz together. `PLLM 25 / PLLN 192 / PLLP 2 / PLLQ 4`. Set
   SYSCLK to 100 and USB silently never enumerates.
3. **`FLASH_LATENCY_3`** at 96 MHz, and **`PWR_REGULATOR_VOLTAGE_SCALE1`** (after
   `__HAL_RCC_PWR_CLK_ENABLE()`) because Scale 2 caps at 84 MHz. Too few wait states → hard fault on
   the first fetch past the clock switch.
4. **`-DHSE_VALUE=25000000`** in `build_flags`. CubeF4's default template happens to be 25 MHz
   already, so it works by luck — until someone drops in a CubeMX-generated `stm32f4xx_hal_conf.h`
   written for an 8 MHz board and every baud rate and timer period shifts by 3.125× with no error.
5. **PC13 is lit by a LOW level** and sits behind the backup-domain power switch: 3 mA maximum,
   `GPIO_SPEED_FREQ_LOW`, and **never as a current source**. Same limit applies to PC14/PC15, which
   are the LSE crystal here anyway.
6. **K1 on PA0 needs `GPIO_PULLUP`** — not every revision fits an external one, and PA0 is one of
   only two pins on this package that are **not 5 V tolerant** (the other is PB5).
7. **Backup domain before RTC**: `__HAL_RCC_PWR_CLK_ENABLE()` then `HAL_PWR_EnableBkUpAccess()`, or
   the LSE configuration reports success and the clock never starts. Expect the LSE itself to take
   ~2 s to stabilise on a cold start — that pause is normal, not a hang.
8. **`HAL_RTC_GetTime()` before `HAL_RTC_GetDate()`, always.** The F4's calendar sits in shadow
   registers that unlock only after both are read. Reversed, the date freezes — a bug that first
   shows up at midnight.
9. **The temperature sensor is ADC channel 18 on the F411**, not 16, and it shares that channel with
   the VBAT divider. Use `ADC_CHANNEL_TEMPSENSOR` and never call `HAL_ADCEx_EnableVBAT()`. F401
   examples — which is most Black Pill material online — use channel 16 and return a plausible wrong
   number.
10. **Any internal ADC channel needs `ADC_SAMPLETIME_480CYCLES`** (≥ 10 µs per the datasheet). The
    3-cycle default reads noise. And use the factory calibration at `0x1FFF7A2A`/`2C`/`2E` rather
    than the datasheet's typical 2.5 mV/°C — VDDA here is just an LDO output, not a reference.
11. **The USB device library is not in the framework.** `framework-stm32cubef4` has no
    `STM32_USB_Device_Library`; vendor `usbd_core/ctlreq/ioreq/cdc` into `lib/`. And point
    `USBD_malloc` at a static buffer — ST's default is `malloc()` against a 0x200-byte heap, which
    fails silently and the device never appears.

## When the task is "how do I print something"

This board has **no USB-serial chip**. That surprises people coming from an Arduino Nano or an ESP32
devkit, and it shapes every debugging session. Options, cheapest first:

| Route | Cost | Notes |
|---|---|---|
| **USB CDC** | ~14 KB flash, PA11/PA12, the USB peripheral | same cable as power and flashing; what `template/` does |
| USART1 on PA9/PA10 | two pins + an external USB-UART dongle | also the ROM bootloader's UART, so it doubles as recovery |
| SWO on PB3 | one pin + a trace-capable SWD probe | no extra cable |
| Semihosting | none | very slow, halts on every write, needs the debugger attached |

USB CDC is the default answer, and it is why rule 2 exists.

## Starting a new project

Do not hand-assemble one — the USB stack alone is five files that have to be written, not installed.
`template/` is a verified-working project; scaffold from it:

```sh
~/.claude/skills/stm32f411-blackpill/template/variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir> && pio run
```

- `--full` (default) — LSE-backed RTC, USB CDC console with echo, calibrated temperature sensor, LED
  and K1. **19808 B of 512 KB flash, 4840 B of 128 KB RAM.**
- `--minimal` — LED + K1 only, same clock tree and startup order. **3476 B flash, 44 B RAM.** Flash
  this first on a new board to prove toolchain, crystal and the DFU route before adding anything.

Both build clean with `-Wall` (verified). Nothing is generated and no paths are embedded, so copying
the tree by hand works equally well. `template/README.md` maps files to subsystems, so a `--full`
scaffold can be stripped back cleanly.

When the user already has a project, prefer bringing it in line with the template's `platformio.ini`
and `board.h` over rewriting their code.

## Flashing

DFU over the USB-C cable, no probe: hold **BOOT0**, tap **NRST**, release → the board enumerates as
`0483:DF11` → `pio run -t upload`. PlatformIO passes `:leave` to dfu-util so the board restarts into
the new firmware by itself; only *entering* DFU is manual, every time. SWD on the 4-pin header when
you need breakpoints (`debug_tool = stlink` — the board definition defaults to `blackmagic`).

Nothing you flash can brick this board: the ROM bootloader runs before user code, so BOOT0 + NRST
always gets you back, including after firmware that reconfigures PA13/PA14 and locks out SWD. If no
DFU device appears at all, suspect BOOT1/PB2 being held high by attached hardware — that selects an
SRAM boot instead. `reference/board-hardware.md` §16.3 has the full recovery ladder.

## Confirm the board first

WeAct sells the same PCB with an **STM32F401CCU6** (256 KB flash, 64 KB RAM, 84 MHz max, temperature
sensor on ADC channel **16**). The silkscreen next to the MCU says which. Most Black Pill tutorials
online are written for the F401, so code lifted from them compiles on an F411 and misreads the
temperature sensor. Everything in this skill is F411-specific unless it says otherwise; the clock
tree in rule 2 works unchanged on an F401.

## Reporting

State honestly what was verified on hardware versus derived from the datasheet. In this skill:
the clock tree, LSE-backed RTC, USB CDC, DFU flashing and PC13 polarity were confirmed on a WeAct
V3.x board; the temperature-sensor module is build-verified only; and K1 on PA0, the SPI-flash
footprint pins and the header order come from the published WeAct pinout rather than from a
schematic. Say so rather than presenting all of it as equally certain.
