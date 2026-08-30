# Beetle (ATmega32U4) template project

A complete PlatformIO + Arduino-core project for the CJMCU / DFRobot Beetle
(Mini Arduino Leonardo, ATmega32U4 @ 16 MHz, 21 × 28 mm). Builds clean as-is;
nothing is generated and no paths are embedded, so copying this tree by hand
works exactly as well as running the scaffold script.

```sh
./variants/new-project.sh <target-dir> [--full|--minimal|--hid]
cd <target-dir>
pio run -t upload -t monitor
```

## Variants

| | Contents | Flash | RAM |
|---|---|---|---|
| `--full` (default) | Non-blocking LED heartbeat, USB-CDC report guarded by `if (Serial)`, free-RAM watch, ADC sample on A0, EEPROM boot counter | **5,214 B** — 18.2 % of the 28,672 B Caterina leaves | **164 B** — 6.4 % of 2,560 B |
| `--minimal` | Blink on the onboard D13 LED. Nothing else. | **3,956 B** — 13.8 % | **149 B** — 5.8 % |
| `--hid` | USB keyboard with an arm pin and a 5 s boot grace window; adds `lib_deps = arduino-libraries/Keyboard` | **6,206 B** — 21.6 % | **231 B** — 9.0 % |

Figures are what `pio run` reports for platform-atmelavr 5.3.0 with
`board = leonardo`.

The number worth staring at is the minimal one: **a bare Blink costs 3,956 B
here against 924 B on an ATmega328P Nano.** The ~3 KB difference is the USB
stack, and it is not optional — the ATmega32U4 core enumerates a CDC port
from `main()` whether or not the sketch mentions `Serial`. Budget the flash
from 28,672 B minus ~4 KB, not from 32 KB.

`--minimal` is the one to flash first on an unfamiliar Beetle: it touches
nothing but one GPIO, so if it does not blink the fault is in the toolchain,
the port, the bootloader window or a clone without a D13 LED — not in your
code.

## Files by subsystem

| File | Subsystem | Needed by |
|---|---|---|
| `platformio.ini` | build config — `board = leonardo`, why `upload_port` must stay unset | all |
| `include/board.h` | **the pin map** — the ten pads, their timers, their ADC channels, and the sixteen I/O lines that go nowhere | all |
| `src/main.cpp` | heartbeat + CDC report + ADC + EEPROM + free-RAM watch | full |
| `variants/minimal/main.cpp` | the toolchain-proving blink | minimal |
| `variants/hid/main.cpp` | USB keyboard, with the guards that keep the board reprogrammable | hid |

To strip a `--full` scaffold back to something of your own, keep
`include/board.h` — that is the reusable half — and replace `src/main.cpp`.

## Uploading

`pio run -t upload -t monitor`. PlatformIO opens the sketch's CDC port at
1200 baud, which makes the ATmega32U4 reset into the Caterina bootloader;
the bootloader enumerates as a **second, differently-named serial port**, and
`avrdude` speaks `avr109` to that one. All of this happens automatically as
long as `upload_port` is left unset.

If the upload fails with *"no device found"* or times out waiting for a port:
short **ICSP pin 5 (RESET) to pin 6 (GND)** on the six dots on the back — the
Beetle has no reset button — and start the upload within eight seconds. One
touch is enough. See the skill's flashing section for the full recovery
ladder.

## What is verified

All three variants were **built** clean against platform-atmelavr 5.3.0 /
framework-arduino-avr, and the size figures above are that build's own
report. The *chip-side* pin map in `include/board.h` — which port, timer, ADC channel
and interrupt sits behind each Arduino pin number — is read from the Arduino
AVR core's `variants/leonardo/pins_arduino.h`, the same file the compiler
uses, and is exact.

Which ten of those pins the Beetle brings out to pads is **transcribed from
DFRobot's own pad↔pin table** (Beetle SKU:DFR0282 documentation), not
inferred. What is not certain is whether a differently-sized clone relabels
anything: DFRobot's board is 20 × 22 mm and some clones are larger.
`reference/board-hardware.md` §2.4 has a short sketch that walks every pad so
you can confirm against your own silkscreen in ten minutes.

Not run on hardware by the author of this skill: the blink itself, HID
enumeration, and the physical layout of any particular clone. The board facts
come from DFRobot's documentation and the bootloader's source rather than from
a board on a bench — SKILL.md §Reporting draws that line precisely. If
something misbehaves, that is a gap in this skill — say so.

## Third-party code

None. Everything here is original to this template. The Arduino AVR core it
links against ships with PlatformIO's `atmelavr` platform; the `--hid`
variant additionally pulls `arduino-libraries/Keyboard` (LGPL-2.1) from the
PlatformIO registry at build time — it is not vendored here.
