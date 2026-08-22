# Raspberry Pi Pico template project

A complete PlatformIO + Arduino project for the Raspberry Pi Pico (RP2040).
Builds clean as-is; nothing is generated and no paths are embedded, so
copying this tree by hand works exactly as well as running the scaffold
script.

```sh
./variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir>
pio run -t upload -t monitor
```

## Which Arduino core

`board_build.core = earlephilhower` is set deliberately. On
platform-raspberrypi, plain `framework = arduino` gives you the Arduino
**Mbed** core — a different core with no `EEPROM.h`, no `Wire1`, no
`Serial2`, 500 Hz `analogWrite` and no `analogWriteFreq()`, on which most
Pico tutorial code does not compile. Keep the line unless you specifically
want the Mbed core.

## Variants

| | Contents | Flash | RAM |
|---|---|---|---|
| `--full` (default) | Non-blocking LED heartbeat, USB-CDC report, ADC on A0 at native 12 bits, die temperature, VSYS via the internal A3 divider, VBUS sense, flash-backed EEPROM boot counter | **61,112 B** — 2.9 % of the 2,093,056 B available | **9,036 B** — 3.4 % of 256 KB |
| `--minimal` | The classic Blink on GPIO25. Nothing else. | **58,172 B** — 2.8 % | **8,732 B** — 3.3 % |

Figures are what `pio run` reports for platform-raspberrypi 1.19.0 with
arduino-pico 5.6.0 (earlephilhower). The floor is the TinyUSB CDC stack:
even a bare blink costs ~58 KB flash and ~8.7 KB RAM. For comparison, the
same blink on the Mbed core (`board_build.core` omitted) is ~4 KB flash but
~40 KB RAM. **Flash `--minimal` first on an unfamiliar board**: if the LED
does not blink after `pio run -t upload`, the problem is the cable, the
BOOTSEL dance or the core choice, not the code.

## Files by subsystem

| File | Subsystem | Needed by |
|---|---|---|
| `platformio.ini` | build config — board, Arduino core selection, upload behaviour | both |
| `include/board.h` | **the pin map** — every board pin, divider and trap in one place | both |
| `src/main.cpp` | heartbeat + USB-CDC report + ADC + VSYS + VBUS + EEPROM counter | full |
| `variants/minimal/main.cpp` | the minimal Blink | minimal |

To strip a `--full` scaffold back to something of your own, keep
`include/board.h` — that is the reusable half — and replace `src/main.cpp`.

## Uploading

`pio run -t upload -t monitor` flashes over the USB cable with no button
press: PlatformIO touches the USB CDC port at 1200 baud and a running
arduino-pico sketch reboots itself into BOOTSEL mode for picotool, which
reboots the board when done. If the running firmware has crashed or its
USB is dead, the upload prints `Cannot find BOOTSEL disk` — hold **BOOTSEL**
while (re)plugging the USB cable, release, then re-run the upload. The
monitor attaches to the CDC port (`/dev/cu.usbmodem*` on macOS, a COM port
on Windows); the baud rate is ignored.

## What is verified on hardware

Both variants build clean with platform-raspberrypi 1.19.0 + arduino-pico
5.6.0 (verified on the author's machine). The `--minimal` blink is the
project this skill was extracted from, near-verbatim. **Nothing here was
measurement-checked on hardware by the skill author**: the ADC math, the
VSYS divider factor of 3 and the EEPROM counter are datasheet-derived and
compile-verified — treat the skill's ADC accuracy section as the bounding
envelope, not a calibration.

If something misbehaves, say so — that is a gap in this skill, not in your
board.

## Third-party code

None. Everything here is original to this template; the arduino-pico core
and TinyUSB it links against ship with PlatformIO's `raspberrypi` platform
under their own licenses.
