---
name: stm32f401rc-blackpill
description: Firmware development for the 40-pin "Black Pill"-format board with an STM32F401RCT6 (STM32F401RC, LQFP64, 256 KB flash, 64 KB RAM, 84 MHz) that ships with the WeAct Studio HID Bootloader (USB 0483:572A, flash 0x08000000-0x08003FFF, app at 0x08004000) — its PC13 LED, KEY button on PA0, 25 MHz HSE, USB-C on USB_OTG_FS, and the PlatformIO + STM32Cube HAL setup around them, including a USB CDC virtual COM port for printf debugging. Use when working on this board, an unbranded F401RCT6 Black Pill clone, or any STM32F401/F411 board that enumerates as "WeAct Studio HID Bootloader": project setup, platformio.ini, linking at 0x08004000, flashing through the HID bootloader or ROM DFU, the app starting behind a bootloader (VTOR, SysTick, clock hand-off), clock/PLL configuration for 84 MHz plus 48 MHz USB, printf/serial console over USB (no /dev/cu or COM port), a blink that never starts or hangs in HAL_Delay, entering or recovering the bootloader, or debugging why something on the board does not work.
---

# STM32F401RCT6 "Black Pill" with the WeAct HID bootloader

Board-specific firmware knowledge. The board's defining feature is not the chip but what sits in the
first 16 KB of its flash: WeAct's HID bootloader. Almost every failure on this board comes from code
that assumes it owns 0x08000000 and a freshly reset chip — neither is true here. Read the reference
files rather than guessing.

- `reference/board-hardware.md` — the hardware (MCU, pins, header, clocks, memory map with the
  bootloader), a full description of the bootloader's behaviour and protocol, PlatformIO
  configuration, flashing and recovery routes, and a symptom → cause → fix table (§9).
- `reference/recipes.md` — copy-paste code taken from `template/`: `platformio.ini`, the linker-script
  change, the hand-off `Reset_Handler`, the 84/48 MHz clock tree, the USB CDC console, the flasher,
  bootloader reinstall.
- `template/` — a **working PlatformIO project**: blink + USB CDC `printf` console, flashed over USB
  through the HID bootloader without touching a button, plus a scaffold script that can also produce
  a bare blink. See `template/README.md`.

## Orientation

| | |
|---|---|
| MCU | STM32F401RCT6 — Cortex-M4F, **84 MHz max, 256 KB flash, 64 KB SRAM**, LQFP64. DEVID **0x423** (same die as the F401CC) |
| Board | 40-pin (2 × 20) Black Pill footprint, **unbranded clone**, USB-C, NRST + BOOT0 + KEY buttons |
| Bootloader | WeAct Studio HID Bootloader **V1.2** in sector 0, enumerates as HID `0483:572A`, driverless |
| Flash map | `0x08000000` bootloader (16 KB, UID lock at `0x08003A28`) · **`0x08004000` app** (240 KB) |
| Crystals | HSE **25 MHz** (proven: the bootloader's PLLM=25 USB enumerates); LSE 32.768 kHz expected, untested |
| Working clock tree | PLL M25/N336/**P4**/Q7 → SYSCLK 84, HCLK 84, PCLK1 42, PCLK2 84, **USB 48**, `FLASH_LATENCY_2`, VOS Scale 2 |
| LED | **PC13**, toggling verified; LOW = lit per WeAct convention (polarity not measured) |
| KEY | **PA0**, pressed = LOW, needs `GPIO_PULLUP` — verified |
| USB | USB-C → USB_OTG_FS PA11/PA12. **No USB-serial chip** — a COM port exists only while firmware with a USB stack runs |
| Debug | SWD on PA13/PA14 (4-pin header on most Black Pill boards) |

## Rules that prevent the expensive mistakes

1. **Link for `0x08004000`** whenever the HID bootloader stays on the chip:
   `board_build.ldscript = ld/STM32F401RCTX_APP_0x08004000.ld` (ST's script with
   `FLASH ORIGIN = 0x8004000, LENGTH = 240K`). The bootloader writes whatever it receives to 0x08004000
   regardless of how it was linked; a 0x08000000-linked image is accepted, written, and then jumps
   into the middle of the bootloader. `scripts/weact_hid_flash.py` refuses such an image.
2. **Replace `Reset_Handler`**, do not just set VTOR in `main()`. The bootloader jumps to the app
   with **VTOR still 0x08000000** — its own vector table — and interrupts unmasked. After
   `HAL_Init()` every SysTick lands in the bootloader's handler, which increments *its* `uwTick`
   (some random word of your RAM), so `HAL_GetTick()` never moves and the first `HAL_Delay()` hangs.
   V1.1 bootloaders also jump with SysTick already running, which corrupts `.data`/`.bss` during
   start-up. `src/boot_handoff.c` masks IRQs, stops SysTick, sets `SCB->VTOR = g_pfnVectors`, and
   only then copies `.data` / zeroes `.bss`. ST's startup declares `Reset_Handler` weak, so this
   simply overrides it.
3. **`-DUSER_VECT_TAB_ADDRESS -DVECT_TAB_OFFSET=0x4000` does not work** with `framework = stm32cube`
   — it makes things worse. The framework's `system_stm32f4xx.c` `#define`s `VECT_TAB_OFFSET
   0x00000000U` unconditionally, so the file wins (one "redefined" warning), and `SystemInit()` then
   actively writes `VTOR = 0x08000000` — verified in the disassembly. Rule 2 is the fix, and its
   VTOR write comes after `SystemInit()` for exactly this reason.
4. **`HAL_RCC_DeInit()` first in `SystemClock_Config()`.** V1.2 jumps before touching the clocks
   (the app starts on HSI), but V1.1 — still found on boards — hands over with HSE + PLL already
   driving SYSCLK, and `HAL_RCC_OscConfig()` returns `HAL_ERROR` for any PLL setting that differs
   from the running one (M25/N336/P4/Q7). One line makes the clock code independent of which
   bootloader, or none, ran before it.
5. **Never `upload_protocol = hid`.** PlatformIO's `hid` is Serasidis' `hid-flash`, which looks for
   `1209:BEBA` and never finds this bootloader (`0483:572A`). The `coreboard_f401rc` board definition
   even lists `hid` — another reason the template uses `genericSTM32F401RC`. Use
   `upload_protocol = custom` + `extra_scripts = scripts/hid_upload.py` as in the template.
6. **ROM DFU and ST-Link write 0x08000000 and erase the HID bootloader**, and **never restore it
   from a dump of another board**: the bootloader keeps this chip's UID at 0x08003A28 and, on a
   mismatch, copies itself to RAM and **mass-erases the entire flash** (V1.2 disassembly). Restore
   with WeAct's official V1.2 `.hex`, whose UID slot is blank — `reference/recipes.md` §7.
7. **Define `SysTick_Handler()`** (`HAL_IncTick()`). `framework-stm32cube` ships no
   `stm32f4xx_it.c`; the weak default is an infinite loop and the board hangs in the first
   `HAL_Delay()` — which, behind this bootloader, looks exactly like rule 2.
8. **84 MHz, Scale 2, two wait states, PCLK1 ≤ 42 MHz.** This is an F401, not an F411: 96/100 MHz
   configurations copied from F411 Black Pill code overclock it, and `PWR_REGULATOR_VOLTAGE_SCALE1`
   is not an F401 range (VOS=11 is reserved and behaves as Scale 2).
9. **`-DHSE_VALUE=25000000`** in `build_flags`. The F4 HAL template defaults to 25 MHz by luck; a
   CubeMX `stm32f4xx_hal_conf.h` from an 8 MHz board silently skews every baud rate and timer by
   3.125×.
10. **KEY on PA0 needs `GPIO_PULLUP`**, and holding it through a reset is the bootloader entry. A
    design that pulls PA0 low (a sensor on PA0, a pressed key at power-up) boots into the bootloader
    instead of your app.
11. **The temperature sensor is ADC_IN16 on the F401** (ADC_IN18 on the F411). Use
    `ADC_CHANNEL_TEMPSENSOR`, which the HAL maps per part, and `ADC_SAMPLETIME_480CYCLES`.
12. **Pins that exist on this chip are not necessarily on the header.** The LQFP64 die has PC0–PC12
    and PD2, but a 40-pin Black Pill layout only routes the 48-pin chip's set. PB11 does not exist on
    the F401 LQFP64 at all (VCAP_1). Read the silkscreen before picking a pin.
13. **No serial port unless the firmware makes one.** The USB-C goes straight to the MCU, so
    `/dev/cu.usbmodem*` / `ttyACM*` / `COMx` appears only while firmware with USB CDC runs. The HID
    bootloader, ROM DFU and a firmware without USB show none — not a driver problem. For debug
    output use the template's CDC console (`printf`), or SWD.

## When the task is "flash it"

`pio run -t upload`. With the template firmware running, that is all: the flasher
(`scripts/weact_hid_flash.py`) opens the board's CDC port at 1200 baud, the firmware reboots into
the bootloader, and the upload prints the bootloader's reply (`V1.2 0x423 ROM: 256KB`), writes 1 KB
pages and reboots into the new app — verified, ~2.6 s, no buttons.

When nothing is listening on CDC (blank chip, the minimal blink, crashed or foreign firmware), the
flasher asks for the manual entry and waits 30 s:

1. Hold **KEY**, tap **NRST**, keep KEY held — PC13 flickers fast while KEY is down. Release.
   PC13 "breathes"; the board enumerates as `0483:572A` "WeAct Studio HID Bootloader".
2. The waiting upload continues by itself.

The bootloader also stays resident by itself when 0x08004000 holds no valid vector table (blank or
erased app) — that is why a brand-new board shows up as HID with no button presses.
`python scripts/weact_hid_flash.py --info` asks the bootloader for version, DEVID and flash size
without writing anything: the quickest way to identify the die on an unknown board.

First upload installs the `hidapi` package into PlatformIO's Python (`scripts/hid_upload.py`). On
Linux hidapi needs a udev rule for `0483:572A` — `reference/board-hardware.md` §8.3.

## Starting a new project

Do not hand-assemble one — the linker script, the `Reset_Handler` and the uploader have to agree.
Scaffold from `template/`:

```sh
~/.claude/skills/stm32f401rc-blackpill/template/variants/new-project.sh <target-dir>            # full
~/.claude/skills/stm32f401rc-blackpill/template/variants/new-project.sh <target-dir> --minimal  # blink only
cd <target-dir> && pio run && pio run -t upload
```

| variant | what it is | flash / RAM |
|---|---|---|
| `--full` (default) | blink + **USB CDC console**: `printf` to the USB port, banner, 1 s heartbeat, KEY events, echo, commands; reboots into the bootloader on a 1200-baud open, so re-flashing needs no buttons | 20408 B / 4424 B |
| `--minimal` | PC13 at 2 Hz, 10 Hz while KEY is held; no USB — every upload needs KEY + NRST | 3896 B / 44 B |

Both use 84 MHz from the crystal and build with zero warnings under `-Wall` (PlatformIO `ststm32`
19.7.1, `framework-stm32cubef4` 1.28.1). Default to `--full`: the console is the board's only
debug output without a probe. Three environments share one source tree:

| env | links at | flashes via | bootloader afterwards |
|---|---|---|---|
| `hid` (default) | 0x08004000 | USB, HID bootloader, `custom` uploader | kept |
| `dfu` | 0x08000000 | ROM DFU: hold BOOT0, tap NRST → `0483:DF11` | **erased** |
| `stlink` | 0x08000000 | SWD probe | **erased** |

The same `boot_handoff.c` is correct in all three; it is harmless when the chip boots straight in.
When the user already has a project, bring its `platformio.ini`, linker script and
`Reset_Handler`/`SystemClock_Config()` in line with the template rather than rewriting their code.

## Debug output over USB

In the full template `printf` goes to the USB CDC port (`0483:5740` "Black Pill F401RC VCP",
`/dev/cu.usbmodem<UID>` on macOS). Read it with `pio device monitor`. Rules that keep it working —
details in `reference/board-hardware.md` §8.4, code in `reference/recipes.md` §5:

- end lines with `\r\n`; stdout is line-buffered, so a partial line appears only after `fflush(stdout)`;
- `printf` only from the main loop / thread context, **never from an interrupt handler**;
- nothing is sent while no terminal has the port open (DTR low) — output from before the port was
  opened is lost, which is why the template prints its banner on every open;
- never open the port at 1200 baud unless you want the bootloader;
- adding peripherals keeps the USB working only if `SystemClock_Config()` keeps PLLQ = 48 MHz and
  nothing else claims PA11/PA12 or `OTG_FS_IRQHandler`.

The banner states the reset cause (power-on, NRST pin, software, IWDG/WWDG) — check it first when
the board restarts unexpectedly.

## Flashing

Routes, in order of preference:

- **HID bootloader** — above. Nothing to install but Python's `hidapi`; no driver on any OS.
  Entry: 1200-baud touch of the template firmware, or KEY + NRST.
- **ROM DFU** (`pio run -e dfu -t upload`) — hold BOOT0, tap NRST, release → `0483:DF11`. Always
  available because it is in ROM, so it is the recovery route for anything — at the cost of the HID
  bootloader (rule 6). Without BOOT0: in the HID bootloader, hold KEY > 0.65 s and release — it
  jumps to the ROM bootloader (from the V1.2 code; not tried on hardware).
- **SWD** (`pio run -e stlink -t upload`) — writes from 0x08000000 like DFU. For breakpoints
  *behind* the bootloader, `pio debug -e hid` loads the `.elf` at its own addresses (sector 1 up) and
  leaves sector 0 alone; the reset then runs the bootloader, which jumps to the app (untested here —
  no probe was attached).

Recovery: nothing flashed through the HID route can touch sector 0, so KEY + NRST always gets back
to the bootloader. If it does not, the bootloader is gone (DFU/SWD write, or its UID self-erase):
BOOT0 + NRST into ROM DFU, then reinstall it from WeAct's `.hex` (`reference/recipes.md` §7).
Right after such a reinstall the first boot records the UID and then pulses PC13 briefly every
0.5 s forever — tap NRST once; that is not a fault.

## Reporting

State honestly what was verified on hardware versus derived. On this skill's board (unbranded
STM32F401RCT6 Black Pill, bootloader V1.2) verified: DEVID 0x423 / 256 KB, KEY + NRST entry (fast
flicker, then "breathing"), HID flashing with the template's flasher, the app starting behind the
bootloader, the 84 MHz clock from the 25 MHz HSE, PC13 blinking, KEY on PA0 active-low, BOOT0 +
NRST enumerating as ROM DFU `0483:DF11` ("STM32  BOOTLOADER"; nothing written through it), and the
full template's USB CDC console on macOS (banner, heartbeat, echo, `i`/`b` commands) with
button-free re-flashing through the 1200-baud touch and DR19. One open observation: with an early
test firmware, an app started after a DR19 entry was twice reset back into the bootloader ~11 s
later; it did not recur with the template (`board-hardware.md` §6.1). From the V1.2 disassembly
only: the UID self-erase, the long-press jump to ROM DFU. Not tested: Linux/Windows hosts for the
console, `[env:dfu]`/`[env:stlink]` uploads. LED polarity, LSE crystal, power circuit and
**header order** are not verified on this clone — check the silkscreen, and say so rather than
presenting the WeAct layout as fact.
