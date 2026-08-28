---
name: rp2350a-weact
description: Firmware development for the WeAct Studio RP2350A Core Board (RP2350A — dual Cortex-M33 @ 150 MHz, 520 KB SRAM, 4 MB or 16 MB QSPI flash) in both of its revisions, RP2350A_V10 and RP2350A_V20 — the 40-pin Pico-2-shaped header, 3-or-4-channel 12-bit ADC, PWM, PIO, USB-C CDC, the V1.0 LDO vs V2.0 buck-boost power wiring, and the PlatformIO + arduino-pico (earlephilhower) setup around them. Use when working on this board, a WeAct RP2350 core board of unknown revision, or any Raspberry Pi Pico 2 / RP2350A clone: project setup, platformio.ini, board = rpipico2 vs weact_rp2350b, telling V1.0 and V2.0 apart, pin mapping, LED and KEY pins, powering it from a battery, 4 MB vs 16 MB flash, erratum RP2350-E9 pull-down trouble, BOOT/RESET UF2 flashing, or debugging why something on the board does not work.
---

# WeAct Studio RP2350A Core Board (RP2350A)

A Pico-2-shaped RP2350A core board that ships in **two incompatible
revisions under one name**. V2.0 is a pin-for-pin Pico 2 replacement; V1.0
brings out every GPIO instead, and pays for it by moving the power pin, the
ADC reference and three GPIO functions. Firmware built for the wrong one
compiles cleanly and then drives the wrong pins — so establish the revision
before writing any code, and read the reference files rather than assuming
Pico 2 behaviour.

- `reference/board-hardware.md` — the complete board reference: how to tell
  the revisions apart, the 40-pin map for both, the off-header GPIO, power
  trees, clocks, memory map, vendor-material inventory **plus** a development
  guide (Part II: §8 toolchain, §10 peripheral cookbook, §11 RP2350 gotchas
  including erratum E9 and the PWM slice map, §12 flashing, §13 a
  symptom → cause → fix table).
- `reference/recipes.md` — copy-paste code: `platformio.ini` with all four
  envs, the revision guard, reading the real flash size out of the chip,
  LEDs, the KEY button, V2.0 power sensing, ADC, `Wire1` remapping, PWM,
  EEPROM, LittleFS, core 1.
- `template/` — a **project that builds clean** for both revisions and both
  flash sizes, in two variants, plus a scaffold script. See
  `template/README.md`.

## Orientation

| | |
|---|---|
| MCU | RP2350A, QFN-60 — 2 × Cortex-M33 @ **150 MHz** (or 2 × Hazard3 RISC-V, not reachable from this toolchain), 30 GPIO |
| Memory | 520 KB SRAM, 8 KB OTP, **no internal flash** — 4 MB **or** 16 MB QSPI, XIP from `0x10000000`. Max sketch 4,190,208 B / 16,773,120 B (last 4 KB = emulated EEPROM) |
| Clock | 12 MHz crystal → 150 MHz. No 32.768 kHz crystal fitted |
| LED | **V1.0**: GP25 green + GP24 blue · **V2.0**: GP25 green only. All **active-HIGH** through 5.1 kΩ — deliberately dim, and not on the header |
| Button | RESET (→ RUN, not readable) and BOOT (→ QSPI_SS, readable at runtime as `if (BOOTSEL)`) on both · **V1.0 only**: `23@KEY` on GP23, **active-LOW**, 5.1 kΩ pull-up |
| USB | USB-C, FS device/host. `Serial` = **USB CDC**; 1200-baud touch reboots into BOOTSEL; enumerates `2E8A:000F`, BOOTSEL drive is named **`RP2350`** (not `RPI-RP2`) |
| ADC | 12-bit: A0=GP26, A1=GP27, A2=GP28, **A3=GP29 — free input on V1.0, VSYS sense on V2.0** + channel 4 = die temp. `analogRead` is **10-bit by default**. ADC_VREF on header pin 35 (V2.0) or test point T1 only (V1.0) |
| PWM | `analogWrite` on any GPIO, 8-bit @ 1 kHz. 12 slices exist but only **slices 0–7 are reachable**: 4 GPIOs per slice, and GP*n* / GP*n+16* share a channel |
| Bus defaults | Wire = 4/5 · **Wire1 = 26/27 (= A0/A1!)** · SPI = 16/19/18/17 · SPI1 = 12/15/14/13 · Serial1 = 0/1 · Serial2 = 8/9 |
| Power | **V1.0**: pin 39 `5V` → LDO, **3.6–6.5 V** → 3.3 V @ 800 mA · **V2.0**: pin 39 `VSYS` → buck-boost, **1.8–5.5 V**. Pin 40 is silkscreened `VIN` on both and is **USB VBUS**, not an input |
| Debug | 4 pads at the far end: `3V3` / `DIO` / `CLK` / `GND`. Boot ROM is mask ROM — **unbrickable** |
| Size | 51.00 × 21.00 mm, rows 17.78 mm apart — same footprint as a Pico 2 |
| Toolchain | PlatformIO + `raspberrypi` **1.20.0** + arduino-pico **6.0.0** (`board = rpipico2`, `board_build.core = earlephilhower`) |

## Rules that prevent the expensive mistakes

Each of these produces a failure that looks like something else.

1. **Establish the revision before writing code.** V1.0 and V2.0 differ on
   GP23, GP24, GP29 and on what pin 39 accepts. Ask the user, or have them
   look at the top row of pins: `VREF` in the ADC-reference slot and one LED
   means V2.0; `29` in that slot, two LEDs and a third button marked
   `23@KEY` means V1.0. Then build with **`-DBOARD_REV=10` or `=20`** — the
   template's `board.h` refuses to compile without it, on purpose.
2. **`board = rpipico2`, never `weact_rp2350b`.** There is no PlatformIO
   board definition for this board. `weact_rp2350b` exists in arduino-pico
   and is the **RP2350B** core board — QFN-80, 48 GPIO, 8 ADC channels, a
   different pinout. It builds and then everything above GP29 is fiction.
   `rpipico2` is exactly right for V2.0 and the best available base for V1.0.
3. **`board_build.core = earlephilhower` in `platformio.ini`.** Plain
   `framework = arduino` selects the Arduino **Mbed** core, which does not
   support RP2350 at all. The failure reads as a broken toolchain, not a
   missing ini line.
4. **The pin silkscreened `VIN` is USB VBUS, on both revisions.** The supply
   input is the pin *next* to it, and the two boards do not accept the same
   voltage there: V1.0's `5V` is an LDO input wanting **3.6–6.5 V**, V2.0's
   `VSYS` is a buck-boost input wanting **1.8–5.5 V**. 6.5 V into a V2.0 is
   over the absolute maximum; a 3.7 V Li-ion into a V1.0 browns out as the
   cell discharges. Only V2.0 runs a single cell properly.
5. **`Serial` is USB CDC, not UART0.** A USB-serial adapter on GP0/GP1 sees
   nothing from `Serial.print` — those pins are `Serial1`. The CDC port
   exists only once the host opens it: earlier bytes are dropped, and
   `while (!Serial)` blocks startup until a monitor connects. Never gate
   bring-up on the port being open.
6. **`analogRead` returns 10-bit values (0–1023) by default** on a 12-bit
   ADC. Symptom: "pinned at 1023 at full scale", or a calibration that is off
   by exactly 4×. `analogReadResolution(12)` before sampling.
7. **GP29 is not the same pin on the two boards.** On V1.0 it is ADC3 on
   header pin 35, a free analog input. On V2.0 it is the VSYS sense (test pad
   only) and header pin 35 is `ADC_VREF` instead. Code that reads "the fourth
   ADC channel" measures the supply on one board and your sensor on the
   other.
8. **The V2.0 VSYS sense is nominally ÷3, but through a FET.** R23/R24 100 kΩ
   into R25 100 kΩ works out to VSYS/3 — the same ×3 math as a Pico — except
   there is a pass FET with its gate at 3V3 in the path, whose resistance
   varies with VSYS. Use it for trends and thresholds; calibrate against a
   meter before quoting an absolute battery voltage.
9. **`INPUT_PULLDOWN` is not usable on RP2350** (erratum RP2350-E9). An input
   whose pad sits between logic levels leaks up to ~120 µA, which overwhelms
   the internal pull-down: the pin latches at ~2 V and reads HIGH forever.
   Wire buttons to GND with `INPUT_PULLUP`, or fit an **external pull-down of
   8.2 kΩ or less**. The same leakage costs ~120 µA per floating input — on a
   battery build that is the whole sleep budget. V1.0's `23@KEY` is
   active-LOW with a 5.1 kΩ pull-up and is unaffected.
10. **PWM aliasing is four-wide, not two.** Slice = `(gpio >> 1) & 7`, so
    slices 0–7 each serve four pins (0/1/16/17, 2/3/18/19, …) and only
    GP14/GP15 are unaliased. Pins on one slice share the frequency; pins 16
    apart share the *channel*, so `analogWrite(2, x)` and `analogWrite(18, y)`
    write the same duty register and the last one wins for both. A
    servo/stepper pair that glitches like a power problem is usually this.
11. **Nothing on the board says whether it has 4 MB or 16 MB of flash.** The
    build believes `board_upload.maximum_size`. Claiming 16 MB on a 4 MB
    board puts the EEPROM sector and any filesystem past the end of the chip.
    Read the JEDEC ID at boot (recipe 3 — the template prints both the real
    size and the one the build assumed), then pick the matching env.
    `picotool info -d` in BOOTSEL is a cross-check, but on a third-party board
    its flash-size field comes from unprogrammed OTP and may say nothing.
    `board_build.f_flash` is an ESP-ism and does nothing on this platform.
12. **Wire1 defaults to GP26/GP27 — the same pins as A0/A1.** I2C corruption
    that starts the moment `analogRead` is called is this, not noise.
    `Wire1.setSDA()/setSCL()` to another i2c1 pair (2/3, 6/7, 10/11, 14/15,
    18/19) before `begin()`.
13. **The LEDs run through 5.1 kΩ.** ~0.25 mA: they are visible in shade and
    almost invisible in daylight. Do not debug a blink sketch that is working.
14. **EEPROM lives in flash and only persists on `commit()`,** and flash
    writes stall *both* cores because code runs from the same QSPI chip via
    XIP. A sketch that never commits loses everything on reboot, silently;
    one that commits inside a timing-critical loop hiccups in a way that looks
    like a scheduler bug.
15. **`flash_do_cmd()` must run during startup, interrupts off.** It stops XIP
    to talk to the flash chip. Called from a running application — or with
    core 1 executing — it hangs the board. That is why the template reads the
    JEDEC ID on the first line of `setup()`.
16. **RP2350 UF2 files are not RP2040 UF2 files.** Different family ID, and
    the BOOTSEL drive is named `RP2350`. An RP2040 image dropped on it is
    rejected rather than run, and a `.uf2` from a Pico project will not boot
    here.
17. **GP26–GP29 are not 5 V tolerant and carry a reverse diode to 3V3.**
    Above ~3.6 V they damage the chip, and any voltage on them while the board
    is unpowered back-powers the whole board through the pin.

## When the task is powering it from something other than USB

This is where the two revisions actually diverge, so answer the revision
question first.

**V2.0** behaves like a Pico 2: VSYS (pin 39) takes 1.8–5.5 V into a
buck-boost, so a single Li-ion cell, 3 × AA, or any 2–5 V supply works
directly and stays working as the cell sags. GP24 tells you whether USB is
present and GP29 gives you approximately VSYS/3, so battery monitoring and
USB/battery switchover need no extra parts. If USB is the only source you may
short VBUS to VSYS to remove the Schottky drop. USB *host* mode needs 5 V fed
to the `VIN` pin (pin 40), not to VSYS.

**V1.0** has an LDO instead: pin 39 (`5V`) wants **3.6–6.5 V** and drops out
below ~3.6 V, so a 1S Li-ion works only down to ~3.6 V — most of the cell is
unusable, and the brown-out looks like a firmware crash. There is no VSYS or
VBUS sense on any GPIO, so battery monitoring means wiring a divider of your
own into one of the four ADC inputs (GP29/ADC3 is free on this revision, which
is convenient). And because it is linear, input current equals output current:
6.5 V in at 300 mA out dissipates about a watt in a SOT-23-5 package.

On both: `3V3` (pin 36) is an output — never back-feed it — and `EN` (pin 37)
pulled low kills the rail and makes the board look bricked with USB attached.

## Starting a new project

Do not hand-assemble one. `template/` builds clean for both revisions;
scaffold from it:

```sh
~/.claude/skills/rp2350a-weact/template/variants/new-project.sh <dir> \
    [--full|--minimal] [--v20|--v10] [--16mb]
cd <dir> && pio run -t upload -t monitor
```

- `--minimal` — blink on GP25, which is a user LED on both revisions.
  **32,916 B** flash / 8,504 B RAM. Flash this first on an unfamiliar board:
  if it does not blink after upload, the problem is the cable, the BOOT dance
  or the env choice, not the code. (The floor is TinyUSB CDC; a bare blink
  costs ~33 KB here.)
- `--full` (default) — non-blocking heartbeat, USB-CDC report, the real flash
  size read from the JEDEC ID, A0 at 12 bits, die temperature, EEPROM boot
  counter, plus the per-revision extras: VSYS + VBUS sense and the SMPS mode
  pin on V2.0, the second LED and the KEY button on V1.0. **37,676 B** flash
  (V2.0) / 9,824 B RAM.

All four envs — `weact_rp2350a_v20`, `_v20_16mb`, `_v10`, `_v10_16mb` —
build as-is with platform-raspberrypi 1.20.0 + arduino-pico 6.0.0 (verified).
Nothing is generated and no paths are embedded, so copying the tree by hand
works identically; `template/README.md` maps files to subsystems.

When the user already has a project, prefer fixing their `platformio.ini`
(`board = rpipico2`, `board_build.core = earlephilhower`, the right
`board_upload.maximum_size`) and adding the `BOARD_REV` guard over rewriting
their code — that is usually the whole fix.

## Flashing

Over the USB-C cable, nothing pressed:

```sh
pio run -t upload -t monitor
```

PlatformIO opens the CDC port at 1200 baud, the running sketch reboots itself
into BOOTSEL, picotool flashes and reboots the board; the LED is blinking
again within a second of `SUCCESS`. The board enumerates as a CDC port
(`/dev/cu.usbmodem*`, `COMx`) at VID:PID `2E8A:000F`; the monitor's baud rate
is ignored.

When the firmware cannot respond — crashed, USB disabled, power-only cable, or
a blank board — the sequence is: hold **BOOT** (the button beside the USB
connector) → plug or replug USB → release → a drive named **`RP2350`**
appears → either re-run `pio run -t upload` (picotool picks up the BOOTSEL
device) or drag `.pio/build/<env>/firmware.uf2` onto the drive. Without
unplugging: hold BOOT, tap RESET, release BOOT. BOOT is sampled at power-up
only — pressing it on a running board does nothing.

That BOOT route always works — the boot ROM is mask ROM and nothing you flash
can disable it, so the board cannot be bricked. Note that `pio run -t erase`
copies PlatformIO's bundled `flash_nuke.uf2`, which carries the **RP2040**
and absolute UF2 family IDs and no RP2350 family: do not count on it here
(untested on this board), and the picotool 2.0.0 that ships with the platform
has no `erase` subcommand. For a genuine clean slate, flash a `flash_nuke`
built for RP2350 from pico-examples; for the ordinary case, flashing a
known-good sketch over the top is enough. SWD on the four end pads
(`3V3`/`DIO`/`CLK`/`GND`, with a Debug Probe or
`upload_protocol = cmsis-dap`) is for live debugging, not for recovery.

## Reporting

State honestly what was verified on hardware and what came from the schematic.
In this skill: **only RP2350A_V20 was run on hardware** — the `--minimal`
blink on GP25 and the USB-C upload path. All four template envs and every
recipe marked as such build clean against platform-raspberrypi 1.20.0 +
arduino-pico 6.0.0. Everything specific to **V1.0 — the GP24/GP25 LEDs, the
GP23 KEY, the free ADC3 on GP29, the LDO's 3.6–6.5 V range — is transcribed
from `V10_SCH.pdf` and has not been run.** No voltage, current, ADC or timing
figure anywhere in this skill was measured: the ADC reference network, the
VSYS ÷3 factor, the LED currents and the regulator ratings are
schematic- and datasheet-derived. Core-behaviour claims (read-resolution
default, `Wire1` pins, the PWM slice formula, the 1200-baud touch) come from
the arduino-pico 6.0.0 and pico-sdk sources. Anything above 150 MHz that a
user asks for is overclocking — label it as outside spec.
