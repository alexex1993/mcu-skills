# LGT8F328P-LQFP32 MiniEVB template project

A complete PlatformIO + `lgt8fx`-core project for the Nano-style 30-pin LGT8F328P board whose
silkscreen reads **`LGTBF32BP`**. Builds clean as-is; nothing is generated and no paths are
embedded, so copying this tree by hand works exactly as well as running the scaffold script.

```sh
./variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir>
pio run -t upload -t monitor
```

## Variants

| | Contents | Flash | RAM |
|---|---|---|---|
| `--full` (default) | Non-blocking LED heartbeat, serial report of `F_CPU` / `F_OSC` / clock source, 12-bit `analogRead` on A0, real VCC measured through the internal `V5D1` channel, an 8-bit DAC ramp on D4, an EEPROM boot counter, free-RAM watch | **4,080 B** — 13.7 % of the 29,696 B the bootloader leaves | **204 B** — 10.0 % of 2,048 B |
| `--minimal` | Blink on `LED_BUILTIN` (D13). Nothing else. | **1,106 B** — 3.7 % | **9 B** — 0.4 % |

Figures are what `pio run` reports for platform `lgt8f` 1.0.3+sha.dea68b9,
`framework-lgt8fx` 2.0.7, `toolchain-atmelavr` 1.70300.191015.

`--minimal` is the one to flash first on an unfamiliar board: if it does not blink, the
problem is the platform, the port or the bootloader — not your code. If it blinks at half or
double the expected rate, the clock is misconfigured; see the `f_osc` comment in
`platformio.ini`.

## Files by subsystem

| File | Subsystem | Needed by |
|---|---|---|
| `platformio.ini` | build config — board/variant choice, **the clock settings**, upload speed | both |
| `include/board.h` | **the pin map** — every board pin, the bonded pads, the timer behind each PWM pin, the DAC, the internal ADC channels | both |
| `src/main.cpp` | heartbeat + clock report + ADC + VCC + DAC + EEPROM + free-RAM watch | full |
| `variants/minimal/main.cpp` | Blink | minimal |

To strip a `--full` scaffold back to something of your own, keep `include/board.h` — that is
the reusable half — and replace `src/main.cpp`.

## Uploading

`pio run -t upload -t monitor` flashes over the USB Micro-B cable through the bootloader;
DTR auto-reset opens it, nothing to press. Default speed is 57600; some bootloaders want
115200 or 19200.

The board enumerates as a CDC port (`usbmodem` / `ttyACM`) if the SOP16 bridge is a Holtek
**HT42B534-1**, or as a WCH VCP port (`wchusbserial` / `ttyUSB`, driver required) if it is a
**CH340G / CH9340C**. Read the marking on the chip.

If `avrdude` reports a verification mismatch on a board that runs correctly, uncomment
`upload_flags = -V` in `platformio.ini`.

Recovery is over **SWD** (PE0/PE2 on the header), not the ICSP footprint — see the skill's
flashing section.

## What is verified

The two variants above **build clean** with the package versions listed, and those are the
measured sizes. **Neither was run on hardware by the author of this skill**, and nothing here
was checked with a meter or a scope. The pin map in `include/board.h` is derived from the
LGT8FX8P databook v1.0.5, the nulllab LQFP32-Nano schematic and the `lgt8fx` core's own pin
tables.

If something misbehaves, say so — that is a gap in this skill, not in your board.

## Third-party code

None. Everything here is original to this template; the `lgt8fx` core it links against ships
with the `darkautism/pio-lgt8fx` PlatformIO platform (core: `dbuezas/lgt8fx`, LGPL-2.1 for
the Arduino-derived parts).
