# Arduino Nano template project

A complete PlatformIO + Arduino-core project for the Arduino Nano (A000005,
ATmega328P). Builds clean as-is; nothing is generated and no paths are
embedded, so copying this tree by hand works exactly as well as running the
scaffold script.

```sh
./variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir>
pio run -t upload -t monitor
```

## Variants

| | Contents | Flash | RAM |
|---|---|---|---|
| `--full` (default) | Non-blocking LED heartbeat, serial report with `F()`-macro strings, free-RAM watch, ADC sample on A0, EEPROM boot counter with update semantics | **2,874 B** — 9.4 % of the 30,720 B the Optiboot bootloader leaves | **201 B** — 9.8 % of 2,048 B |
| `--minimal` | The classic Blink on LED_BUILTIN (D13). Nothing else. | **924 B** — 3.0 % | **9 B** — 0.4 % |

Figures are what `pio run` reports for platform-atmelavr 5.3.0. `--minimal`
is the one to flash first on an unfamiliar board: it was verified on real
hardware, and if it does not blink the problem is the toolchain, the COM port
or the bootloader era — not your code.

## Files by subsystem

| File | Subsystem | Needed by |
|---|---|---|
| `platformio.ini` | build config — board choice (Optiboot vs old bootloader), upload speed | both |
| `include/board.h` | **the pin map** — every board pin, timer and trap in one place | both |
| `src/main.cpp` | heartbeat + serial report + ADC + EEPROM + free-RAM watch | full |
| `variants/minimal/main.cpp` | the hardware-verified Blink | minimal |

To strip a `--full` scaffold back to something of your own, keep
`include/board.h` — that is the reusable half — and replace `src/main.cpp`.

## Uploading

`pio run -t upload -t monitor` flashes over the USB (Mini-B) cable through
the bootloader; the DTR auto-reset opens it, nothing to press. If upload
fails with `not in sync` / `stk500_recv`, uncomment `upload_speed = 57600`
in `platformio.ini` — the board has the old ATmegaBOOT bootloader (pre-2018
genuine boards and most clones). The board enumerates as an FTDI VCP COM
port on genuine boards, a CH340 COM port on most clones.

## What is verified on hardware

The Blink in `variants/minimal/main.cpp` ran on an Arduino Nano A000005
(v3.x, Optiboot bootloader) — the pin map it relies on (LED_BUILTIN = D13,
HIGH = on) is hardware-confirmed.

The `--full` variant's serial report, ADC read, EEPROM counter and free-RAM
watch are built from datasheet-derived behavior of the Arduino AVR core and
compile clean, but were not run on hardware by the author of this skill. The
`wdt_disable()` guard in `setup()` exists for old-bootloader boards and is
harmless on Optiboot ones.

If something misbehaves, say so — that is a gap in this skill, not in your
board.

## Third-party code

None. Everything here is original to this template; the Arduino core it
links against ships with PlatformIO's `atmelavr` platform.
