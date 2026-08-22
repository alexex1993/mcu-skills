---
name: rp2040-pico
description: Firmware development for the Raspberry Pi Pico (original, RP2040 — dual Cortex-M0+ @ 133 MHz, 264 KB SRAM, 2 MB QSPI flash) — its 26 exposed GPIO, 4-channel 12-bit ADC, 16-channel PWM, 2× UART/I2C/SPI, PIO state machines, USB 1.1 CDC, VSYS/VBUS/SMPS power wiring, and the PlatformIO + arduino-pico (earlephilhower) setup around them. Use when working on this board, any Pico H or Pico clone, or a bare RP2040: project setup, platformio.ini, the earlephilhower-vs-Mbed core choice, pin mapping, USB CDC serial, analogRead/analogWrite behaviour, Wire/SPI defaults, battery power, EEPROM emulation, BOOTSEL/UF2 flashing, or debugging why something on the board does not work.
---

# Raspberry Pi Pico (RP2040)

Board-specific firmware knowledge. The Pico's failure modes are quiet: a
wrong `platformio.ini` line silently links a different Arduino core, `Serial`
goes to a port that does not exist yet, and an ADC pin doubles as a power
rail — so read the reference files rather than guessing.

- `reference/board-hardware.md` — the complete board reference: 40-pin map
  with alt functions, the four hidden GPIOs, power tree, clocks, memory map,
  relevant errata **plus** a development guide (Part II: §7 toolchain and
  the core-choice table, §8 peripheral cookbook, §9 flashing, §10
  symptom → cause → fix table).
- `reference/recipes.md` — copy-paste code: `platformio.ini`, `board.h`
  excerpts, USB CDC + Serial1, ADC with the VSYS ×3 math, PWM and the slice
  rule, Wire1 remapping, EEPROM with `commit()`, interrupts, core 1,
  BOOTSEL tricks.
- `template/` — a **project that builds clean**, in two variants, plus a
  scaffold script. See `template/README.md`.

## Orientation

| | |
|---|---|
| MCU | RP2040 — 2× ARM Cortex-M0+ @ **133 MHz** (arduino-pico default; SDK default 125 MHz), 40 nm QFN-56 |
| Memory | 264 KB SRAM (6 banks), **no internal flash** — 2 MB W25Q16JV QSPI, XIP from `0x10000000` + 16 KB cache. Max sketch 2,093,056 B (last 4 KB = emulated EEPROM) |
| LED / button | LED = GPIO25, **active-HIGH**, not on the header · BOOTSEL button, sampled **at power-up only** |
| USB | micro-B, FS device/host; `Serial` = **USB CDC**; 1200-baud touch reboots into BOOTSEL |
| ADC | 12-bit 500 ksps: A0=GPIO26, A1=27, A2=28, **A3=GPIO29 = VSYS/3 (internal)** + channel 4 = die temp; `analogRead` is **10-bit by default**; ENOB 8.7 bits |
| PWM | every GPIO, 16 ch (8 slices × 2); `analogWrite` default 8-bit @ 1 kHz; **GPIO 2n & 2n+1 share one slice** |
| Bus defaults | Wire = 4/5 · **Wire1 = 26/27 (= A0/A1!)** · SPI = 17/18/19/16 · Serial1 = 0/1 · Serial2 = 8/9 |
| Power | VBUS 5 V → diode → VSYS **1.8-5.5 V** → RT6150 buck-boost → 3.3 V; 3V3 pin out < 300 mA; 3V3_EN low = board "dead" |
| Debug | 3-pin SWD header (SWCLK/GND/SWDIO); boot ROM is mask ROM — **unbrickable** |
| Toolchain | PlatformIO + `raspberrypi` 1.19.0 + arduino-pico 5.6.0 (`board_build.core = earlephilhower`) |

## Rules that prevent the expensive mistakes

Each of these produces a failure that looks like something else.

1. **`board_build.core = earlephilhower` in `platformio.ini`.** Plain
   `framework = arduino` silently builds against the Arduino **Mbed** core:
   `#include <EEPROM.h>` fails to compile, `Wire1`/`Serial2` don't exist,
   `analogWrite` is 500 Hz with no frequency API — and none of the usual
   Pico tutorial code works. The error reads like a broken dependency, not
   a missing ini line.
2. **`Serial` is USB CDC, not UART0.** A USB-serial adapter wired to
   GPIO0/1 sees nothing from `Serial.print` — those pins are `Serial1`.
   The CDC port exists only once the host opens it: bytes printed before
   that are dropped, and `while (!Serial)` blocks startup until a monitor
   connects. Never gate bring-up on the port being open.
3. **Upload usually needs no button — until it does.** `pio run -t upload`
   touches the CDC port at 1200 baud and a *running* sketch reboots itself
   into BOOTSEL for picotool. If the firmware has crashed or its USB is
   dead, the upload dies with `Cannot find BOOTSEL disk` — hold BOOTSEL
   while replugging USB, then re-run. That recovery is always available;
   the boot ROM is mask ROM and cannot be overwritten.
4. **BOOTSEL only works at power-up.** Pressing it while the board runs
   does nothing (it grounds the flash CS line). To get the `RPI-RP2`
   drive: hold BOOTSEL, plug/replug USB, release. A running sketch can
   re-enter it from code (`reset_usb_boot(0, 0)` — recipe 13).
5. **`analogRead` returns 10-bit values (0-1023) by default** on a 12-bit
   ADC. Symptom: "stuck at 1023 at full scale" or calibration off by 4×.
   `analogReadResolution(12)` before sampling.
6. **A3/GPIO29 is not a free pin — it measures VSYS/3.** Multiply by 3 for
   the input voltage. GPIO23 (SMPS power-save), GPIO24 (VBUS sense), GPIO25
   (LED), GPIO29 are real GPIOs in code but **not on the 40-pin header** —
   wiring plans that use them compile and "work" while nothing appears on
   the breadboard.
7. **GPIO pairs share a PWM slice.** Slice = pin/2: 0&1, 2&3 … 28&29 share
   one counter and therefore one frequency — the last `analogWriteFreq()`
   on either pin retimes both, and a stepper/servo pair on one slice
   glitches in a way that looks like a power problem.
8. **Wire1 defaults to GPIO26/27 — the same pins as A0/A1.** I2C bus
   corruption that starts as soon as analogRead is called is this, not
   electrical noise. `Wire1.setSDA()/setSCL()` to another I2C1 pair
   (2/3, 6/7, 10/11, 14/15, 18/19, 22/23) before `begin()`.
9. **GPIO26-29 are not 5 V tolerant and carry a reverse diode to 3V3.**
   Above ~3.6 V they damage the chip; any voltage on them while the board
   is unpowered back-powers the 3.3 V rail (a Pico that "ghosts" with no
   USB connected is being fed through an analog pin). GPIO0-25 tolerate
   applied voltage while unpowered.
10. **The ADC's accuracy budget is worse than its bit count.** Reference =
    filtered 3.3 V SMPS rail, ~30 mV inherent offset (150 µA through the
    200 Ω filter), ENOB 8.7 bits, DNL spikes at codes 512/1536/2560/3584.
    Naive absolute measurements are ±2-3 %. Average; cancel offset with a
    grounded channel; drive GPIO23 HIGH during sampling to quiet the SMPS
    (recipe 5), or fit an LM4040 3.0 V on ADC_VREF (range drops to 3.0 V).
11. **EEPROM lives in flash and only persists on `commit()`.** `begin(n)`
    maps a shadow, `put()` stages (update semantics), `commit()` burns one
    4 KB sector erase (100 k cycles). A sketch that never commits loses
    everything on reboot — silently, with no error.
12. **Flash writes stall both cores.** Code executes from the same QSPI
    chip (XIP), so `EEPROM.commit()` and filesystem writes pause the whole
    processor mid-stream. Keep them out of timing-critical and ISR-adjacent
    windows; the symptom is a periodic hiccup that looks like a scheduler
    bug.
13. **USB may not enumerate on a busy hub** (erratum E5, B0/B1 silicon —
    B2 fixes it): next to a chatty device on the same transaction
    translator, the port never leaves RESET. LED works, no COM port. Plug
    in directly or move ports before reinstalling drivers.
14. **Power wiring has three traps.** Feed VSYS (pin 39, 1.8-5.5 V) for
    battery/external power — a single Li-ion works. USB *host* mode needs
    5 V supplied to the VBUS pin (pin 40), not just VSYS. And a board that
    looks completely dead with USB connected usually has 3V3_EN (pin 37)
    shorted low — that pin disables the entire SMPS. The 3V3 pin (36) is an
    output (< 300 mA); never back-feed it. RUN (pin 30) shorted to GND is
    the reset button.

## When the task is analog measurement

The ADC is ratiometric to a filtered SMPS rail, not to a reference. What
that means in numbers: full scale is 3.3 V ± the SMPS tolerance (±1-2 %);
there is a built-in ~30 mV offset because the ADC's ~150 µA supply current
flows through the 200 Ω filter resistor (it varies ±20 µA with sampling);
effective resolution is 8.7 bits, with DNL spikes at four codes roughly
every 1024 counts (512, 1536, 2560, 3584 — erratum E11). So a single raw
12-bit reading implies far more accuracy than it has.

Work in that order: average many samples (free, fixes noise + DNL); tie a
spare ADC channel to AGND and subtract its reading (cancels the offset);
drive GPIO23 HIGH while sampling to force the SMPS into PWM mode, LOW after
(cuts ripple, costs light-load efficiency); and only then consider the
LM4040 3.0 V shunt on ADC_VREF — that also shrinks the input range to
3.0 V. For battery monitoring, `analogRead(A3) × 3` is well inside the
ADC's envelope — relative changes are trustworthy even when absolutes
aren't.

## When the task is powering the board

The chain is VBUS → Schottky D1 → VSYS → RT6150 buck-boost → 3V3. The
buck-boost accepts 1.8-5.5 V at VSYS, which makes power the most flexible
part of the board: USB, one Li-ion cell (with protection!), 3×AA, or any
2.3-5.5 V supply with an ORing diode/P-FET into VSYS. The traps: USB host
mode must see 5 V on the **VBUS pin** (VSYS power alone leaves the host
port dead); USB-only setups may bridge VBUS to VSYS to remove the diode
drop; and never feed the 3V3 pin — it's an output, and > 3.3 V there goes
straight into the RP2040 and flash. VSYS current draw: ~10 mA idle in
BOOTSEL, ~1.3 mA sleep, ~0.8 mA dormant, 90+ mA loaded.

## Starting a new project

Do not hand-assemble one. `template/` builds clean; scaffold from it:

```sh
~/.claude/skills/rp2040-pico/template/variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir> && pio run -t upload -t monitor
```

- `--minimal` — classic Blink on GPIO25. **58,172 B** flash / 8,732 B RAM.
  Flash this first on an unfamiliar board: if the LED does not blink after
  upload, the problem is the cable, the BOOTSEL dance or rule 1, not the
  code. (The floor is TinyUSB CDC — a bare blink costs ~58 KB flash here,
  vs ~4 KB flash / ~40 KB RAM on the Mbed core.)
- `--full` (default) — non-blocking heartbeat, USB-CDC report, A0 at 12
  bits, die temperature, VSYS via A3, VBUS sense, EEPROM boot counter.
  61,112 B flash / 9,036 B RAM.

Both build as-is with platform-raspberrypi 1.19.0 + arduino-pico 5.6.0
(verified). Nothing is generated and no paths are embedded, so copying the
tree by hand works identically. `template/README.md` maps files to
subsystems.

When the user already has a project, prefer adding
`board_build.core = earlephilhower` to their `platformio.ini` over
rewriting their code — that one line is usually the whole fix.

## Flashing

Over the micro-USB cable:

```sh
pio run -t upload -t monitor
```

Normally nothing is pressed. PlatformIO opens the CDC port at 1200 baud,
the running sketch reboots into BOOTSEL, picotool flashes and reboots the
board — the LED blinks again within a second of `SUCCESS`. The board
enumerates as a CDC port (`/dev/cu.usbmodem*`, `COMx`); the monitor's baud
rate is ignored.

When the running firmware cannot respond (crashed, USB disabled, power-only
cable), the keystroke sequence is: hold **BOOTSEL** → plug/replug USB →
release → a 128 MB `RPI-RP2` drive appears → either re-run `pio run -t
upload` (picotool picks up the BOOTSEL device), or drag
`.pio/build/pico/firmware.uf2` onto the drive — the board reboots itself
when the copy completes. `pio run -t erase` copies a `flash_nuke.uf2` that
erases all 2 MB (including EEPROM) — the clean-slate recovery. A
power-only USB cable is the most common cause of "no RPI-RP2 drive".

The alternative route is SWD over the 3-pin bottom header with a second
Pico as Picoprobe (`upload_protocol = picoprobe`) — needed only when flash
itself is wedged; the mask-ROM bootloader survives everything.

## Reporting

State honestly what was verified on hardware and what came from the
datasheet. In this skill: both template variants and the recipes marked
compile-verified build clean against platform-raspberrypi 1.19.0 +
arduino-pico 5.6.0; the `--minimal` blink is the user project this skill
was extracted from. **No analog measurement, power draw or timing figure
was measured on hardware by the author** — the ADC accuracy envelope, VSYS
divider, SMPS behaviour and current figures are datasheet-derived
(Pico datasheet §3-4, RP2040 datasheet errata), and core-behaviour claims
(read resolution defaults, Wire1 pins, upload touch) come from the
arduino-pico 5.6.0 sources. Anything over 133 MHz F_CPU a user requests is
overclocking — label it as outside spec.
