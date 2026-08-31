---
name: lgt8f328p-minievb
description: Firmware development for the LGT8F328P-LQFP32 MiniEVB — the Nano-style 30-pin "purple nano" board whose silkscreen reads LGTBF32BP (also sold as BTE21-15A, WAVGAT, LGT8F328P Nano V3.0), built on the Logic Green / Prodesign LGT8F328P, an AVR-instruction-compatible LGT8XM part with a 32 MHz internal RC and no crystal, a 12-bit ADC, an 8-bit DAC on D4, four timers, 80 mA drive pins, SWD instead of ICSP, and EEPROM emulated in program flash. Use when working on this board, any LGT8F328P/LGT8F328D/LGT8FX8P board, or a sketch being ported from an Arduino Nano or ATmega328P: PlatformIO setup with the lgt8fx core, platformio.ini, f_cpu/f_osc/clock_source and why timings come out 2x wrong, pin mapping and the LQFP32 bonded pads, analogRead resolution and reference errors, analogWrite hitting the DAC, EEPROM vanishing after upload, 80 mA outputs, upload failures over CH340/CH9340/HT42B534, SWD recovery, or debugging why something on the board does not work.
---

# LGT8F328P-LQFP32 MiniEVB ("LGTBF32BP")

A board that looks exactly like an Arduino Nano, takes the same sketches, and is not one. The
LGT8F328P is a Logic Green LGT8XM core that executes the AVR instruction set inside a chip
with a different clock system, a 12-bit ADC, a DAC, four timers, no real EEPROM and no
crystal. `board.build.mcu` is literally `atmega328p`, so **everything compiles and most things
silently behave differently**. Almost every failure on this board is a Nano assumption that
was not true.

Read the reference files rather than reasoning from ATmega328P knowledge.

- `reference/board-hardware.md` — the complete board reference: the LQFP32 pin map with the
  four bonded pads, Arduino pin numbering past D19, timer/PWM map, PMX0/1/2 alternate
  routing, the HDR high-current table, power tree, clock tree, memory map and the EEPROM
  emulation, **plus** a development guide (§8 toolchain and measured build sizes, §9 the
  clock-prescaler trap, §10 peripheral cookbook, §11 core gotchas, §12 flashing and SWD
  recovery, §13 symptom → cause → fix table).
- `reference/recipes.md` — copy-paste code: `platformio.ini`, `board.h`, blink, the 12-bit
  ADC, measuring the real VCC, the DAC on D4, Timer3 PWM on D1/D2, 80 mA drive, emulated
  EEPROM, moving the UART to D5/D6, sleep, and disabling SWD.
- `template/` — a **project that builds clean**, in two variants, plus a scaffold script. See
  `template/README.md`.

## Confirm the board first

The silkscreen `LGTBF32BP` is Logic Green's stylisation of `LGT8F328P`, not a different part.
What actually has to be decided is the **package**, because it picks the core variant:

| Package | Board | PlatformIO `board` | `build.variant` |
|---|---|---|---|
| **LQFP32** | **Nano-style 30-pin MiniEVB, "purple nano", WAVGAT — this skill** | `LGT8F328P` | `lgt8fx8p` |
| LQFP48 | MiniEVB 48-pin breakout | `lgt8f328p-LQFP48` | `lgt8fx8p48` |
| SSOP20 | "green pseudo Pro Mini" | `LGT8F328P-SSOP20` | `lgt8fx8ps20` |
| LQFP32 | Wemos TTGO XI | `lgt8f328p-wemos-TTGO-XI` | `lgt8fx8p-wemos-TTGO-XI` |

Count the pins on the chip, not on the header. Getting this wrong gives a build that flashes
and runs with a pin map that is wrong past D19.

Also read the SOP16 USB bridge marking — **HT42B534-1** (USB CDC, no driver needed on
macOS/Linux, enumerates as `usbmodem`/`ttyACM`) or **CH9340C / CH340G** (needs the WCH VCP
driver, enumerates as `wchusbserial`/`ttyUSB`). It decides why the board does or does not
appear as a port.

## Orientation

| | |
|---|---|
| MCU | LGT8F328P (Logic Green / Prodesign), 8-bit **LGT8XM** RISC core, AVR-instruction-compatible, 1.8–5.5 V, 0–32 MHz |
| Clock | **No crystal on the MCU.** Internal 32 MHz RC ±1 %; `CLKPR` boots at ÷8 so the part starts at 4 MHz, and the core reprograms it from `F_OSC / F_CPU`. PB6/PB7 (XTAL) are unused and have no Arduino pin number |
| Memory | 32 KB flash (**29,696 B usable** — 3 KB bootloader), **2 KB SRAM**, **no EEPROM array** — 0/1/2/4/8 KB emulated out of the same flash at 2× cost |
| LED | **L** = D13 / PB5, on = **HIGH**. Red power LED is not controllable |
| ADC | **12 channels, 12-bit**; `analogRead()` returns 10 bits until `analogReadResolution(12)`. Refs: AVCC, AVREF (PE6), **1.024 / 2.048 / 4.096 V** ±1 % |
| DAC | **8-bit, on PD4 = D4.** `analogWrite(4, v)` is an analog voltage, not PWM |
| PWM | D3 (T2B), D5 (T0B), D6 (T0A), D9 (T1A), D10 (T1B), D11 (T2A) **and D1, D2 (Timer3)**. `digitalPinHasPWM()` is wrong on this part |
| UART | D0/D1 — shared with the USB bridge. Movable to D5/D6 via `PMX0.TXD6`/`RXD5` |
| I2C / SPI | A4=SDA, A5=SCL · D10=SS, D11=MOSI, D12=MISO, D13=SCK |
| Extra pins | `E0` (22, SWC), `E2` (23, SWD), `E4` (24), `E6` (**25**, AREF), `E5` (**26**), `C6` (27, RESET) |
| High current | 80 mA on D5, D6, D1/TX, D2, PE4, PE5 via `HDR`; 12 mA otherwise |
| Power | **VCC ≈ 4.6 V on USB** — a protection diode sits in the 5 V line. One VCC pin and one GND pin on the whole package |
| Debug | **SWD on PE0 (SWC) / PE2 (SWD)** — not ICSP. Disableable by one register bit, permanently until a power-up reset trick |
| Toolchain | PlatformIO + `darkautism/pio-lgt8fx` (platform `lgt8f` 1.0.3) + `framework-lgt8fx` 2.0.7 (the dbuezas `lgt8fx` core), `board = LGT8F328P` |

## Rules that prevent the expensive mistakes

Each of these produces a failure that points somewhere else.

1. **Never set `board_build.f_osc` to match `board_build.f_cpu`.** `F_OSC` describes the
   oscillator (always 32000000L here — the internal RC); `F_CPU` describes the core; the core
   programs `CLKPR` from `F_DIV = F_OSC / F_CPU`. Setting both to 16 MHz makes `F_DIV = 1`,
   the RC stays undivided, and the part runs at 32 MHz while every `delay()`, `micros()` and
   baud rate is computed for 16 MHz. **Everything is 2× off and nothing reports an error.**
   To run at 16 MHz, change `f_cpu` alone. Most "how to run at 16 MHz" snippets on the web
   contain this bug.
2. **Set `board_build.clock_source = 1` explicitly.** Without a `CLOCK_SOURCE` define the
   core skips `lgt8fx8x_clk_src()` and leaves its own default prescaler, so `F_CPU` and the
   real clock diverge. `1` = internal RC, `2` = external crystal — and this board has no
   crystal on the MCU.
3. **`analogReadResolution(12)` or you are running a 10-bit ADC.** The core ships
   `analog_resbit = 2` and throws the bottom two bits away for Arduino compatibility. The
   symptom is a chip that "isn't any better than a Nano".
4. **`analogWrite(4, v)` is the DAC, not PWM.** D4 = PD4 = DAC0 outputs a real analog
   voltage. It also does not call `pinMode()` and does not short-circuit `0`/`255` to
   `digitalWrite()`. A ported LED-fade works; a MOSFET gate or an opto driven from D4 does
   not, and the scope shows DC instead of edges.
5. **`analogWrite(1, …)` kills `Serial`.** The LQFP32 bonds PF1 onto the D1 pad, so Timer3's
   OC3A comes out on the UART TX pin; `analogWrite(2, …)` likewise takes INT0. Use another
   pin, or move the UART with `PMX0.TXD6`/`RXD5` first.
6. **`INTERNAL` is 1.024 V, and `INTERNAL2V56` is silently 2.048 V.** Both are `#define`d in
   this core to the LGT references — `INTERNAL2V56` has the *same value* as
   `INTERNAL2V048`. Ported code that calibrated against 1.1 V is ~7 % out; code asking for
   2.56 V is 25 % out. Neither errors.
7. **`DEFAULT` means AVCC ≈ 4.6 V, not 5.000 V.** A protection diode in the board's 5 V line
   drops it. Every `DEFAULT`-referenced reading is scaled by ~0.92. Use an internal
   reference, or read the real supply through the chip's own `V5D1` channel (recipe 5).
8. **Four header pads carry two port bits each.** D1 = PD1‖PF1, D2 = PD2‖PF2, `E4` =
   PE4‖PF4, `E5` = PE5‖PF5. Both halves have their own `DDRx`/`PORTx` and both drivers reach
   the same bond wire — writing them against each other is a short inside the package. The
   core handles this for `analogWrite()`; nothing protects hand-written register code.
9. **The emulated EEPROM is erased by every upload, including an update.** It lives in
   program flash, page-swapped by the E2PCTL controller. 1 KB of EEPROM also costs 2 KB of
   flash that PlatformIO's 29,696 B budget does not know about, and only 1020 of each 1024
   bytes are usable. Anything that must survive a firmware update needs another home.
10. **Never write `MCUSR` with bit 7 set.** `MCUSR[7]` is `SWDD` — it disables the SWD port,
    and with it debugging and ISP recovery, permanently until you hold RESET low through a
    power-up so the sketch cannot run. `MCUSR = 0;` is safe. `MCUSR = 0xFF;` to "clear the
    reset flags" is a brick.
11. **Keep the 32 MHz RC enabled (`PMCR[0]`) even if you switch the master clock.** `E2P_clk`
    is 32 MHz ÷ 32 and comes only from that oscillator; without it every EEPROM operation and
    every flash self-read stops working, with no error.
12. **`digitalPinHasPWM()` is wrong here** — inherited unchanged from the AVR core, it
    reports 3/5/6/9/10/11 and misses D1, D2 and D4. Do not let a library use it to pick a pin.
13. **`PMX0`, `PMX1`, `PMX2`, `PMCR`, `CLKPR` and `ECCR` are write-protected.** Set the guard
    bit (`0x80`) first, then complete the real write within 4–6 system clocks. A single
    ordinary assignment does nothing at all — the register reads back unchanged and the
    feature simply never turns on.
14. **PE6 is AREF and PC6 is RESET until you say otherwise.** `PMX2.E6EN` and `PMX2.C6EN`
    turn them into GPIO; taking PC6 also takes away DTR auto-reset, so every subsequent
    upload needs a manual reset.
15. **One VCC pin, one GND pin, six 80 mA pads.** The databook says do not drive four
    high-current loads at once on this package. The bond wires are the limit, not the pads.
16. **SRAM is still 2 KB with no MPU.** `F()` every literal, `PROGMEM` every table, watch
    free RAM — the Nano arithmetic is unchanged, and 32 MHz does not buy you memory.

The watchdog rule from the Nano does **not** apply: the core installs `__patch_wdt()` in
`.init3` (`MCUSR = 0; wdt_disable();`) before `main()`, so a sketch that leaves the WDT
running cannot boot-loop the board.

## When the task is porting an ATmega328P sketch

This is the common case, and it compiles on the first try, which is the problem. Work through
in this order:

1. **Clock** — rules 1 and 2. If timing is out by exactly 2× or 8×, stop here.
2. **Analog reads** — rules 3, 6, 7. A ported sketch reads 10 bits against a 4.6 V reference
   it believes is 5.000 V. Both errors are silent and they do not cancel.
3. **`analogWrite` targets** — rules 4 and 5. Check D1, D2, D4 specifically.
4. **EEPROM** — rule 9. `EEPROM.put()`/`get()` work, the data does not survive the next
   upload.
5. **Pin numbers past D19** — a Nano has none; here `A6`/`A7` are 20/21, then `E0`, `E2`,
   `E4`, `E6`, `E5`, `C6` are 22, 23, 24, **25**, **26**, 27. The core's `D25`/`D26`
   aliases are swapped relative to their own numbers; use the `E*` names.
6. **Libraries that touch registers** — anything writing `TCCR*`, `ADMUX`, `MCUSR` or
   assuming `digitalPinHasPWM()` needs reading. Timer0 is still `millis()`; Timer3 is new
   and lands on D1/D2.

What the port *gains*, and is worth using instead of working around: 32 MHz, 12-bit ADC with
calibrated internal references, a DAC, a programmable-gain differential amplifier, two
comparators, a fourth timer, and 80 mA outputs.

## When the task is analog

The reason to pick this chip over a Nano, and where the Arduino API hides the most.

- The ADC is 12-bit but each `analogRead()` costs **two** conversions: the core takes a
  sign-inverted sample via `ADCSRC.SPN`, averages the pair, then applies a gain correction
  (`pVal -= pVal >> 7`). Budget roughly twice an ATmega328P's conversion time, and do not
  expect a raw register read.
- Discard the first sample after any mux or reference change.
- The internal references are ±1 % and calibrated in the factory (`VCAL1`/`VCAL2`/`VCAL3`);
  `analogReference()` loads the right one. They are far better than a Nano's 1.1 V and are
  the right default here — `DEFAULT` (AVCC) on this board means "referred to a diode drop".
- `V5D1` (VCC × 1/5), `V5D4` (VCC × 4/5) and `IVREF` are internal ADC channels you pass
  straight to `analogRead()`. Measuring your own supply is two lines (recipe 5).
- The differential amplifier (×1/8/16/32, `ADTMR.DIFS` + `DAPCR`) and the two comparators
  have **no core API** — registers only, and untested in this skill.
- Sleep does not power the analog blocks down. Disable ADC, DAC, comparators and LVD by hand
  before sleeping or the µA figures are fiction.

## Starting a new project

Do not hand-assemble one. `template/` builds clean; scaffold from it:

```sh
~/.claude/skills/lgt8f328p-minievb/template/variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir> && pio run
```

- `--minimal` — Blink on `LED_BUILTIN` (D13). **1,106 B** flash, **9 B** RAM. **Flash this
  first on an unfamiliar board**: if it does not blink, the problem is the platform, the port
  or the bootloader; if it blinks at half or double rate, it is rule 1.
- `--full` (default) — non-blocking heartbeat, serial report of `F_CPU`/`F_OSC`/clock source,
  12-bit ADC read, real-VCC measurement through `V5D1`, a DAC ramp on D4, an EEPROM boot
  counter and a free-RAM watch. **4,080 B** flash, **204 B** RAM.

Both figures are what `pio run` reported with platform `lgt8f` 1.0.3+sha.dea68b9 /
`framework-lgt8fx` 2.0.7. Nothing is generated and no paths are embedded, so copying the tree
by hand works identically. `template/README.md` maps files to subsystems.

When the user already has a project, prefer bringing their `platformio.ini` and
`include/board.h` in line with the template over rewriting their code — the clock settings in
rules 1 and 2 are usually the whole bug.

## Flashing

Over the USB Micro-B cable, nothing to press — DTR auto-reset opens the bootloader:

```sh
pio run -t upload -t monitor
```

`avrdude -c arduino` at **57600** (this board definition's default; some bootloaders want
115200 or 19200). The builder always appends `-D`, which skips the chip erase because the
bootloader erases per page.

Upload failure checklist, in order:

1. **No port at all** → missing WCH VCP driver (CH340G/CH9340C only; HT42B534-1 is CDC and
   needs none).
2. Wrong port.
3. Something wired to D0/D1 — disconnect it. Also check nothing enabled Timer3 PWM on D1.
4. Try `upload_speed = 115200`, then `19200`.
5. No auto-reset — press RESET as the upload starts. If a previous sketch set `PMX2.C6EN`,
   this is now permanent.
6. `avrdude` verification mismatch on a board that otherwise runs → add `upload_flags = -V`.
7. Missing TX pull-up: MiniEVB boards are reported to leave D1 without one, which upsets some
   bridges during upload; 10 kΩ from D1 to VCC is the known fix.

**Recovery is SWD, not ICSP.** The ICSP header on the board is a Nano leftover — this chip is
programmed through PE0 (SWC) and PE2 (SWD), which are on the main header. A second Arduino
running the core's `LarduinoISP` example is the standard programmer: D13→SWC, D12→SWD,
D10→RST, plus VCC/GND, and a 10 µF cap between the programmer's RESET and VCC. Then `Burn
Bootloader` or `Upload using Programmer`. A purple LQFP32 board with a CH9340C cannot itself
serve as the programmer.

If a sketch disabled SWD (rule 10), **hold RESET low while powering the board up** so the
sketch never runs, then program over SWD.

## Reporting

Be explicit about what is verified. In this skill:

- **Built and measured**: the template, both variants, against platform `lgt8f`
  1.0.3+sha.dea68b9 / `framework-lgt8fx` 2.0.7 / `toolchain-atmelavr` 1.70300.191015. The
  flash and RAM figures above are what the build reported. Every recipe in
  `reference/recipes.md` compiles against this core.
- **Verified at the toolchain level**: rule 1 — building with `f_cpu = f_osc = 16000000L`
  makes the preprocessor evaluate `F_OSC / F_CPU` to 1, while `f_cpu = 16000000L` alone gives
  2.
- **Derived from primary documents**: the pin map, bonded pads, HDR/PMX/PMCR/MCUSR behaviour,
  clock tree, memory map and EEPROM emulation come from the LGT8FX8P databook v1.0.5, the
  nulllab LQFP32-Nano schematic and the `lgt8fx` core source. Board-level facts (the ≈4.6 V
  VCC, the missing TX pull-up, the LT1117-5.0 on VIN) come from the schematic and community
  reports and are marked as such in `reference/board-hardware.md` §3.
- **Not verified on hardware by the author of this skill.** Nothing here was checked with a
  meter, a scope or a load. Recipes 7, 8, 10, 11 and 12 are marked **⚠︎ compile-checked
  only** in `reference/recipes.md`. Say so when a user's measurement disagrees — that is a
  gap in this skill, not in their board.
- **Untested and API-less**: the differential amplifier and the analog comparators. The
  dbuezas core exposes no functions for either; the databook register names are in
  `reference/board-hardware.md` §6.
- The databook contradicts itself in two places noted in the reference: the `PMCE` write
  window (6 clocks in the clock chapter, 4 in the register summary) and flash endurance
  (10,000 in the English text, 100,000 in the Chinese line beside it).
