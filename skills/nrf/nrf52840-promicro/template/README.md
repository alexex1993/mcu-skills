# ProMicro nRF52840 (V1940) template project

A complete PlatformIO + Adafruit-nRF52-Arduino project for the ProMicro
nRF52840 / SuperMini nRF52840 — a nice!nano v2 compatible clone in the Pro
Micro form factor. Builds clean as-is; nothing is generated and no paths are
embedded, so copying this tree by hand works exactly as well as running the
scaffold script.

```sh
./variants/new-project.sh <target-dir> [--full|--minimal|--ble]
cd <target-dir>
pio run
# double-tap RESET, then:
pio run -t upload -t monitor
```

## Why boards/ is part of the project

**This board does not exist in PlatformIO.** `platform-nordicnrf52` 10.11.0
ships 44 nRF5 boards and none of them is a nice!nano or a Pro Micro clone; the
Adafruit core ships 14 variants and none of them matches this pin map either.
So two things are vendored here and must travel with any project for this
board:

| Path | What it is |
|---|---|
| `boards/promicro_nrf52840.json` | board definition — MCU, SoftDevice, flash/RAM limits, upload protocol, USB VID/PIDs, and `-DCONFIG_NFCT_PINS_AS_GPIOS` (plural — the singular spelling in most clone JSONs is a no-op and leaves P0.09/P0.10 as NFC pins) |
| `boards/variants/promicro_nrf52840/` | the pin map — `variant.h` (names) and `variant.cpp` (`g_ADigitalPinMap`) |

`"variants_dir": "boards/variants"` in the JSON is what points the Adafruit
core at the vendored variant; it is resolved relative to the project root.
Remove that key and the build fails with
`cores/nRF5/Uart.h:27:10: fatal error: variant.h: No such file or directory`.

## Variants

| | Contents | Flash | RAM |
|---|---|---|---|
| `--minimal` | Blink the on-board blue LED (P0.15). No USB stack. | **20,196 B** — 2.5 % of 815,104 B | **3,092 B** — 1.3 % of 237,568 B |
| `--full` (default) | Non-blocking LED heartbeat, USB-CDC status line, 12-bit ADC on A1, die temperature | **55,320 B** — 6.8 % | **8,772 B** — 3.7 % |
| `--ble` | LED heartbeat + BLE Nordic UART Service bridged to USB CDC | **128,676 B** — 15.8 % | **15,588 B** — 6.6 % |

Figures are what `pio run` reports for platform-nordicnrf52 10.11.0 with
framework-arduinoadafruitnrf52 1.10700.0 (Adafruit core 1.7.0). The USB CDC
stack costs ~35 KB flash and ~5.7 KB RAM over the bare blink; Bluefruit adds
another ~73 KB flash and ~6.8 KB RAM on top of that.

**Flash `--minimal` first on an unfamiliar board.** If the LED does not blink
after a successful upload, the problem is your board revision's LED pin, the
bootloader, or the RESET double-tap — not your code.

## Files by subsystem

| File | Subsystem | Needed by |
|---|---|---|
| `platformio.ini` | build config — board, upload protocol, monitor | all |
| `boards/promicro_nrf52840.json` | **board definition** — do not delete | all |
| `boards/variants/promicro_nrf52840/variant.h` | pin names, LED polarity, LF clock choice (`USE_LFRC`) | all |
| `boards/variants/promicro_nrf52840/variant.cpp` | `g_ADigitalPinMap` — Arduino pin → P0.xx/P1.xx | all |
| `include/board.h` | the pin map and flash/RAM map as plain macros, for your own code | all |
| `src/main.cpp` | heartbeat + USB-CDC report + ADC + die temperature | full |
| `variants/minimal/main.cpp` | the minimal blink | minimal |
| `variants/ble/main.cpp` | BLE UART bridge | ble |

To strip a `--full` scaffold back to something of your own, keep `boards/` and
`include/board.h` — that is the reusable half — and replace `src/main.cpp`.

## Uploading

The board never reboots itself into the bootloader from a cold start: **double-
tap RESET** (two fast presses, or bridge RST to GND twice) until the LED starts
a slow fade and a USB drive named `NICENANO` / `PROMICRO` appears. Then:

```sh
pio run -t upload -t monitor
```

PlatformIO packages `firmware.hex` into `firmware.zip` (a Nordic DFU package,
`--sd-req 0x00B6` = SoftDevice S140 6.1.1) and pushes it with
`adafruit-nrfutil dfu serial --singlebank`. A *running* sketch that has USB CDC
up will also enter DFU from the 1200-baud touch PlatformIO does first, so
repeat uploads often need no button at all — but never rely on that during
bring-up.

Alternative: convert `.pio/build/promicro_nrf52840/firmware.hex` to UF2 with
[`uf2conv.py`](https://github.com/microsoft/uf2) (family `0xADA52840`) and drag
it onto the bootloader drive.

## What is verified

All three variants build clean with platform-nordicnrf52 10.11.0 +
framework-arduinoadafruitnrf52 1.10700.0 (verified). The board definition,
`g_ADigitalPinMap` and the LED pin come from a working project for this exact
board (V1940). **Nothing here was measurement-checked with instruments**: the
ADC millivolt conversion and the die-temperature reading are compile-verified
and derived from the nRF52840 datasheet and the core sources, not calibrated.
Battery charging and the RGB/second LED that some clone revisions carry are
**not covered** — this board revision's schematic is not published.

If something misbehaves, say so — that is a gap in this skill, not in your
board.

## Third-party code

None. Everything here is original to this template. It links against the
Adafruit nRF52 Arduino core, Adafruit TinyUSB and Bluefruit52Lib, which ship
with PlatformIO's `nordicnrf52` platform under their own licenses, and against
Nordic's SoftDevice S140, which is already on the board.
