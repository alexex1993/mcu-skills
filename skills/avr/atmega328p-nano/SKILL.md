---
name: atmega328p-nano
description: Firmware development for the Arduino Nano (A000005, ATmega328P @ 16 MHz, 5 V) — its 30 KB usable flash, 2 KB SRAM, 1 KB EEPROM, 8-channel 10-bit ADC, six 8-bit PWM pins across three timers, UART shared with the USB-serial chip, I2C on A4/A5, SPI on D10–D13, sleep modes, and the Arduino AVR core + PlatformIO setup around them. Use when working on this board or any ATmega328P Nano clone (FTDI or CH340 USB): project setup, platformio.ini, old-vs-new bootloader uploads, pin mapping, A6/A7 restrictions, timer/Servo/tone conflicts, the 2 KB SRAM budget, analog references, powering from VIN, ICSP recovery, or debugging why something on the board does not work.
---

# Arduino Nano (ATmega328P)

Board-specific firmware knowledge. The Nano is simple enough that every
failure mode is silent — the sketch compiles, uploads, runs, and does
something subtly wrong — so read the reference file rather than guessing.

- `reference/board-hardware.md` — the complete board reference: corrected pin
  map (the official manual's own tables contain errors), power tree, clock,
  memory map, connector/LED inventory **plus** a development guide (Part II:
  §8 toolchain, §10 peripheral cookbook, §11 gotchas, §12 flashing, §13
  symptom → cause → fix table).
- `reference/recipes.md` — copy-paste code: `platformio.ini`, `board.h`,
  non-blocking timing, serial + free RAM, EEPROM with update semantics, ADC
  and references, interrupts, the timer/PWM map, Servo vs `tone()`,
  power-down sleep, PROGMEM tables.
- `template/` — a **project that builds clean**, in two variants, plus a
  scaffold script. See `template/README.md`.

## Orientation

| | |
|---|---|
| MCU | ATmega328P, 8-bit AVR @ **16 MHz** (crystal — fixed, no PLL), Harvard architecture |
| Memory | 32 KB flash (30,720 B usable — 2 KB bootloader reserve), **2 KB SRAM**, 1 KB EEPROM. No DMA, no MPU |
| LED / button | **L** = D13 (`LED_BUILTIN`), on = **HIGH** · **RESET** button, **active-low** |
| ADC | 8 × 10-bit, 0–5 V, ~100 µs per `analogRead()`; A6/A7 are **analog-input only** |
| PWM | 6 × 8-bit: D3, D11 (~490 Hz, Timer2), D9, D10 (~490 Hz, Timer1), D5, D6 (~980 Hz, Timer0) |
| UART | D0/D1 — **shared with the USB-serial chip**, not free pins |
| I2C / SPI | A4=SDA, A5=SCL · D10=SS, D11=MOSI, D12=MISO, D13=SCK (+ICSP header) |
| Power | 5 V logic. USB Mini-B, VIN 7–12 V via LDO, +5V pin (bypasses regulator). 3V3 pin **50 mA max** |
| Debug | No SWD/JTAG — only the 6-pin **ICSP** header (pin 1 at the outer edge; some diagrams rotate it 180°) |
| Toolchain | PlatformIO 6.1 + `atmelavr` 5.3.0 + Arduino AVR core, `board = nanoatmega328new` (Optiboot) / `nanoatmega328` (old bootloader) |

## Rules that prevent the expensive mistakes

Each of these produces a failure that looks like something else.

1. **`upload_speed = 57600` on old-bootloader boards.** Genuine Nanos sold
   before Jan 2018 and most clones run the old ATmegaBOOT at 57600 baud, not
   Optiboot's 115200. The wrong choice fails uploads with `not in sync` /
   `stk500_recv` — which reads like a driver or cable problem, not a
   firmware-era problem. In the Arduino IDE the equivalent is
   `Tools → Processor → ATmega328P (Old Bootloader)`.
2. **The 2 KB SRAM is the wall, not the 30 KB flash.** Every `Serial.print`
   string literal, `const` array and `String` copy lands in SRAM by default.
   The failure is a heap/stack collision that corrupts variables or crashes
   after minutes-to-days — nothing prints "out of memory". `F()` on every
   literal, `PROGMEM` for tables, static buffers instead of `String`, and
   watch free RAM (recipe 4).
3. **`digitalWrite(A6)` / `digitalRead(A7)` compile and silently misbehave.**
   A6/A7 feed the ADC mux only — there is no port register behind them. They
   are analog-input-only; any digital use needs the signal moved to a real
   pin. A0–A5 are full digital pins (aliases 14–19).
4. **D0/D1 are wired to the FTDI/CH340 chip, not to free GPIO.** Using them
   while `Serial` is open produces garbage in both directions, and anything
   external wired to them blocks uploads. The TX/RX LEDs show USB-serial
   traffic only — they say nothing about D1 driven manually.
5. **Never write `TCCR0A`/`TCCR0B`.** Timer0 is `millis()`, `micros()` and
   `delay()` **and** the ~980 Hz PWM on D5/D6. "Just changing the PWM
   frequency" on those pins makes `delay()` lie and baud-rate-timed software
   (SoftwareSerial, DHT, NeoPixel) glitch, with no error anywhere.
6. **Every PWM pin is rented from a library.** `Servo` takes Timer1 — D9/D10
   `analogWrite()` dead while attached. `tone()` takes Timer2 — D3/D11 dead
   while playing. Servo + tone coexist (different timers) but neither
   survives touching Timer0. Also: PlatformIO's AVR core bundles only
   EEPROM/HID/SoftwareSerial/SPI/Wire — `#include <Servo.h>` is a compile
   error until `lib_deps = arduino-libraries/Servo` is added (the Arduino
   IDE bundles it).
7. **D13 is the LED and the SPI clock.** The LED + series resistor load the
   line, which matters when D13 is an input or SCK: a flaky SPI bus that
   blinks L with the clock is this, not your wiring.
8. **All GPIO are 5 V push-pull.** Into a 3.3 V-only peripheral that is
   damage, not a logic level. The 3V3 pin is an output limited to 50 mA —
   powering a sensor is fine, a radio module is not.
9. **`analogReference(EXTERNAL)` before any voltage touches AREF**, and AREF
   ≤ 5 V always. Driving AREF while an internal reference is selected burns
   the pin.
10. **The +5V pin bypasses the regulator.** Feeding it from anything above
    5.0 V — or from a "5 V" supply that drifts — goes straight into the MCU.
    Unregulated input belongs on VIN, which needs ≥ 7 V to clear the LDO
    dropout (7–12 V recommended).
11. **`wdt_disable()` first in `setup()`** (and clear `MCUSR`). A sketch that
    leaves the watchdog enabled boot-loops boards with the old ATmegaBOOT
    bootloader — it does not clear the WDT on entry — and the loop survives
    re-uploading; only ICSP recovery or Optiboot fixes it.
12. **Unconnected inputs are not zero — they float.** Pull-ups (20–50 kΩ)
    are off by default: `INPUT_PULLUP` or an external resistor, or the pin
    reads random noise.
13. **USB + VIN together = auto power selection, highest voltage wins.** A
    9 V VIN while debugging over USB back-feeds the USB port. Power
    actuators from VIN, the debug session from USB, grounds common — or
    accept VIN < 5 V.

## When the task is memory

2,048 B covers globals, heap and stack together, and there is no MPU: the
heap grows up, the stack grows down, and their collision is whatever random
corruption results. The arithmetic that matters:

- A `Serial.println("temperature sensor ready")` costs 26 B of SRAM forever;
   in `F()` it costs 0. A 256-entry `uint16_t` table is an eighth of the
   chip — `PROGMEM` it (recipe 11).
- The Arduino core + a Blink already use ~600 B flash and single-digit bytes
   of RAM; `Serial` adds ~1.5 KB flash. The template's full variant —
   heartbeat + serial report + ADC + EEPROM counter — totals 2,874 B flash,
   201 B RAM. Budget accordingly: a 1 KB receive buffer is half the machine.
- `String` fragments the small heap; static `char[]` buffers do not. On this
   part, prefer `snprintf()` into a fixed buffer.
- Instrument, don't estimate: the `free_ram()` probe in recipe 4 costs 20 B
   flash. Below ~200 B free, stop adding features and restructure.

## When the task is timing

The three timers are the whole scheduling story, and all of them are spoken
for:

| Timer | Owned by | PWM pins | Free to reconfigure? |
|---|---|---|---|
| Timer0 | `millis()`/`micros()`/`delay()` | D5, D6 (~980 Hz) | **never** |
| Timer1 | `Servo` library | D9, D10 (~490 Hz) | if Servo is not used (recipe 8 has the fast-PWM setup) |
| Timer2 | `tone()` | D3, D11 (~490 Hz) | if tone is not used |

Consequences worth internalizing: an ISR or `noInterrupts()` section longer
than ~100 µs starts eating UART characters and skewing `millis()`-based
intervals; `delayMicroseconds()` is cycle-counted and stays exact; and
anything bit-banged (DHT, one-wire, SoftwareSerial) degrades when interrupts
are blocked — including by long `Servo` pulses. The non-blocking
`millis()` pattern (recipe 3) is the default shape of every loop on this
board.

## Starting a new project

Do not hand-assemble one. `template/` builds clean; scaffold from it:

```sh
~/.claude/skills/atmega328p-nano/template/variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir> && pio run
```

- `--minimal` — the classic Blink on the "L" LED, hardware-verified. 924 B
  flash, 9 B RAM. **Flash this first on an unfamiliar board**: if it does
  not blink, the problem is the toolchain, the COM port or the bootloader
  era (rule 1), not the code.
- `--full` (default) — non-blocking heartbeat, serial report with `F()`
  strings, free-RAM watch, ADC sample on A0, EEPROM boot counter. 2,874 B
  flash, 201 B RAM.

Both build as-is with platform-atmelavr 5.3.0 (verified). Nothing is
generated and no paths are embedded, so copying the tree by hand works
identically. `template/README.md` maps files to subsystems.

When the user already has a project, prefer bringing it in line with the
template's `platformio.ini` and `include/board.h` over rewriting their code.

## Flashing

Over the Mini-B USB cable, nothing to press — the DTR auto-reset opens the
bootloader (Optiboot listens ~1 s at 115200, old ATmegaBOOT ~1–2 s at
57600):

```sh
pio run -t upload -t monitor
```

The board enumerates as a COM port: FTDI VCP on genuine boards, CH340 on
most clones (needs the WCH driver — its absence is why a clone "does not
appear on any port").

Upload failure checklist, in order: wrong bootloader era (rule 1), wrong
COM port / missing CH340 driver, something wired to D0/D1 (disconnect), and
only then assume the sketch. A sketch that crashes instantly or blocks
interrupts can be replaced by timing a RESET press during upload.

The recovery route is the 6-pin ICSP header with a second Arduino running
`11.ArduinoISP`, a USBasp, or `avrdude` — it bypasses the bootloader
entirely, so **no bad firmware permanently bricks this board**. `Burn
Bootloader` over ICSP also restores the fuse set (LOW=0xFF, HIGH=0xDA,
EXT=0x05) — the fix for clones with mis-set clock fuses and for WDT
boot-loops. ICSP is also how to upload bootloader-less and reclaim the
reserved 2 KB.

## Reporting

Say what is verified on hardware and what is derived. In this skill the
**Blink** (the template's `--minimal` variant) ran on an Arduino Nano
A000005 v3.x with the Optiboot bootloader, and the pin map it relies on is
the corrected physical pinout of §2 — the official A000005 manual's own pin
tables contain errors (they omit RESET/GND and AREF, mislabel 3V3, and
invent "Serial Wire Debug" pins), so quote this skill's tables, not
Arduino's. Everything marked **⚠︎ compile-checked only** in
`reference/recipes.md` — ADC references, interrupts, Timer1 PWM, Servo/
tone, sleep, PROGMEM — compiles against the Arduino AVR core but was not
run on hardware by the author. The VIN range is reported inconsistently by
the sources (7–15 V, 7–12 V, 6–20 V); this skill states 7–12 V recommended
as the conservative intersection.
