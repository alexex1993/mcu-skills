# LGT8F328P-LQFP32 MiniEVB (Nano-style, 30 pin) — hardware reference

> Sources, all primary:
> - `LGT8FX8P_databook_v1.0.5` — Logic Green LGT8F88P/168P/328P databook, community English
>   translation (294 p.) and the Chinese original. The pin-bundling table, the HDR/PMX/PMCR
>   register definitions and the clock tree below are quoted from it.
> - `LGT328P-LQFP32-Nano.pdf` — nulllab's KiCad schematic of the Nano-style LQFP32 board,
>   rev V1.0, 2021-12-30. The only source for what the *board* wires.
> - `HT42B534-x` v1.30 (Holtek) and `CH9340DS` (WCH) — the two USB-serial bridges that appear
>   in the SOP16 position.
> - The `lgt8fx` Arduino core (`dbuezas/lgt8fx`, `lgt8f/` — `boards.txt`,
>   `variants/lgt8fx8p/`, `variants/standard/pins_arduino.h`, `cores/lgt8f/main.cpp`,
>   `cores/lgt8f/wiring_analog.c`, `libraries/E2PROM/`) and the PlatformIO wrapper
>   `darkautism/pio-lgt8fx` (`boards/LGT8F328P.json`, `builder/frameworks/arduino.py`).
>
> **Verification status.** The template project in this skill builds clean (figures in §8).
> The clock-prescaler trap in §9 was verified by inspecting the macros the toolchain actually
> emits. Everything else here is read off the databook, the schematic and the core source.
> **Nothing in this file was measured on a board by the author of this skill.** Where the
> sources disagree, that is said in place.

---

## 1. Identifying the board

The silkscreen reads **`LGTBF32BP`** — that is Logic Green's own stylisation of
**LGT8F328P**, not a different part. `BTE21-15A` next to it is the board/batch code and
`V1322` the board revision.

The LGT8F328P is **not** an ATmega328P. It is a Logic Green LGT8XM RISC core that executes
the AVR instruction set, in a chip with different peripherals, a different clock system, no
real EEPROM, and a 12-bit ADC. `avr-gcc` compiles for it and `board.build.mcu` is literally
`atmega328p`, which is why ported sketches compile and then behave subtly wrong.

Pick the package before anything else — it decides the core variant:

| Package | Silkscreen / board | PlatformIO `board` | core `build.variant` |
|---|---|---|---|
| **LQFP32** | MiniEVB Nano-style 30-pin, "purple nano", WAVGAT | `LGT8F328P` | `lgt8fx8p` |
| LQFP48 | MiniEVB 48-pin breakout | `lgt8f328p-LQFP48` | `lgt8fx8p48` |
| SSOP20 | "green pseudo Pro Mini" | `LGT8F328P-SSOP20` | `lgt8fx8ps20` |
| LQFP32 | Wemos TTGO XI | `lgt8f328p-wemos-TTGO-XI` | `lgt8fx8p-wemos-TTGO-XI` |

This file is the **LQFP32** board. Count the pins on the chip, not the header.

The USB-serial bridge is **either** a Holtek **HT42B534-1** **or** a WCH **CH9340C** in the
SOP16 position — read the marking. It changes which driver the host needs (§3.1). The nulllab
schematic this file was checked against shows a **CH340G** in that position, so treat the
bridge as board-revision-dependent and everything else as fixed.

---

## 2. Pin map

### 2.1 The whole LQFP32 package

`Ard` is the number `pinMode()`/`digitalWrite()` take. `Hdr` is what the Nano-style header
calls it. Functions are from the databook's QFP48→QFP32 bundling table.

| Pin | Port(s) on the pad | Ard | Hdr | Functions |
|---|---|---|---|---|
| 1 | PD3 | 3 | D3 | INT1, **OC2B** (Timer2 B), PCINT19 |
| 2 | PD4 | 4 | D4 | **DAO — the DAC output**, T0, XCK, PCINT20 |
| 3 | **PE4 ‖ PF4** | 24 (`E4`) | PE4 | OC0A (PE4) · OC1B, ICP3 (PF4) · HDR[4] 80 mA |
| 4 | VCC | — | — | the *only* supply pin on this package |
| 5 | GND | — | — | the *only* ground pin |
| 6 | **PE5 ‖ PF5** | 26 (`E5`) | PE5 | AC1O, CLKO (PE5) · OC1A (PF5) · HDR[5] 80 mA |
| 7 | PB6 | — | — | XTAL1/TOSC1. **No Arduino pin number in the LQFP32 variant** |
| 8 | PB7 | — | — | XTAL2/TOSC2. Same |
| 9 | PD5 | 5 | D5 | **OC0B** (Timer0 B), T1, RXD-alt, HDR[0] 80 mA, PCINT21 |
| 10 | PD6 | 6 | D6 | **OC0A** (Timer0 A), TXD-alt, AIN0, HDR[1] 80 mA, PCINT22 |
| 11 | PD7 | 7 | D7 | ACXN (comparator common negative), PCINT23 |
| 12 | PB0 | 8 | D8 | ICP1, CLKO output option, PCINT0 |
| 13 | PB1 | 9 | D9 | **OC1A** (Timer1 A), PCINT1 |
| 14 | PB2 | 10 | D10 | **OC1B** (Timer1 B), SPI SS, PCINT2 |
| 15 | PB3 | 11 | D11 | **OC2A** (Timer2 A), SPI MOSI, PCINT3 |
| 16 | PB4 | 12 | D12 | SPI MISO, PCINT4 |
| 17 | PB5 | 13 | D13 | SPI SCK, AC1P, **the "L" LED**, PCINT5 |
| 18 | PE0 | 22 (`E0`) | SWC | **SWD clock**, APN4 (diff-amp negative ch. 4) |
| 19 | PE1 | 20 (`A6`) | A6 | ADC6, ACXP (comparator common positive) |
| 20 | PE6 | 25 (`E6`) | AREF | ADC10, **AVREF** — needs `PMX2.E6EN` to be GPIO |
| 21 | PE2 | 23 (`E2`) | SWD | **SWD data** |
| 22 | PE3 | 21 (`A7`) | A7 | ADC7, AC1N |
| 23 | PC0 | 14 (`A0`) | A0 | ADC0, APP0 (diff-amp positive ch. 0), PCINT8 |
| 24 | PC1 | 15 (`A1`) | A1 | ADC1, APP1, PCINT9 |
| 25 | PC2 | 16 (`A2`) | A2 | ADC2, APN0, PCINT10 |
| 26 | PC3 | 17 (`A3`) | A3 | ADC3, APN1, PCINT11 |
| 27 | PC4 | 18 (`A4`) | A4 | ADC4, **SDA**, PCINT12 |
| 28 | PC5 | 19 (`A5`) | A5 | ADC5, **SCL**, PCINT13 |
| 29 | PC6 | 27 (`C6`) | RESET | **RESETN** — needs `PMX2.C6EN` to be GPIO |
| 30 | PD0 | 0 | D0/RX | USART RXD, PCINT16 |
| 31 | **PD1 ‖ PF1** | 1 | D1/TX | USART TXD (PD1) · **OC3A** (PF1) · HDR[2] 80 mA |
| 32 | **PD2 ‖ PF2** | 2 | D2 | INT0, AC0O (PD2) · **OC3B** (PF2) · HDR[3] 80 mA |

### 2.2 Bonded pads — the LQFP32-only hazard

The databook opens the pinout chapter with: *"QFP48L packing leads out all pins. Other
packages … bundle several internal I/O to one pin. Please pay high attention to pin out
configuration."* On this package four pads carry **two port bits each**:

| Pad | Ports | What the second port is for |
|---|---|---|
| pin 3 (`E4`) | PE4 ‖ PF4 | Timer1 OC1B alternate output, Timer3 capture |
| pin 6 (`E5`) | PE5 ‖ PF5 | Timer1 OC1A alternate output |
| pin 31 (`D1`, TX) | PD1 ‖ PF1 | **Timer3 OC3A** — this is how D1 has PWM |
| pin 32 (`D2`) | PD2 ‖ PF2 | **Timer3 OC3B** — this is how D2 has PWM |

Both halves have their own DDRx/PORTx bits and both drivers reach the same bond wire. Setting
`DDRD |= (1<<1)` high and `DDRF |= (1<<1)` low is a driver fight inside the package. The core
knows this for Timer3: `analogWrite(1, v)` clears `DDRD1` before it sets `DDRF1`. Nothing
protects you when you write the registers yourself.

### 2.3 Arduino pin numbers past D19

Beyond the Nano-compatible range the numbering is the core's, and two of the aliases lie:

| Name | Number | Port |
|---|---|---|
| `A6` / `D20` | 20 | PE1 |
| `A7` / `D21` | 21 | PE3 |
| `E0` / `D22` | 22 | PE0 (SWC) |
| `E2` / `D23` | 23 | PE2 (SWD) |
| `E4` / `D24` | 24 | PE4 |
| `E6` | **25** | PE6 (AREF) |
| `E5` | **26** | PE5 |
| `C6` | 27 | PC6 (RESET) |

`variants/standard/pins_arduino.h` defines `D25` as `26` and `D26` as `25`. Use `E4`/`E5`/`E6`
and never the `D24`–`D26` spellings.

Internal ADC channels are numbers in the same space and are meant to be passed to
`analogRead()`: `V5D1`/`VCCM` = 22 (VCC × 1/5), `IVREF` = 27, `V5D4` = 28 (VCC × 4/5),
`AGND` = 29, `DACO` = 30, `PGAO` = 32.

### 2.4 Timers and PWM

| Arduino pin | Port | Timer channel | Notes |
|---|---|---|---|
| D1 | PD1 ‖ PF1 | **TIMER3A** | takes over the UART TX line |
| D2 | PD2 ‖ PF2 | **TIMER3B** | takes over INT0 |
| D3 | PD3 | TIMER2B | |
| **D4** | PD4 | **LGTDAO0** | the 8-bit DAC, not PWM |
| D5 | PD5 | TIMER0B | Timer0 is `millis()`/`micros()`/`delay()` |
| D6 | PD6 | TIMER0A | same |
| D9 | PB1 | TIMER1A | |
| D10 | PB2 | TIMER1B | also SPI SS |
| D11 | PB3 | TIMER2A | also SPI MOSI |

`digitalPinHasPWM(p)` is inherited unchanged from the AVR core and reports only
`3/5/6/9/10/11`. It is wrong on this part in both directions: D1, D2 and D4 also respond to
`analogWrite()`. Do not use that macro to decide anything.

Four timers exist (TMR0/1/2/3), the databook advertises up to 9 PWM channels with three sets
of programmable dead-time control, and Timer3 is 16-bit — none of which a stock 328P has.

### 2.5 Port alternate multiplexing (PMX0/PMX1/PMX2)

Peripherals can be moved off their default pads. All three registers are write-protected:
set the WCE bit (`PMX0[7]`, shared by PMX0 and PMX1; `PMX2[7]` for PMX2) and complete the
write within 6 system clocks.

`PMX0` (0xEE):

| Bit | Name | 0 → | 1 → |
|---|---|---|---|
| 6 | `C1BF4` | OC1B on PB2 (D10) | OC1B on PF4 (pad `E4`) |
| 5 | `C1AF5` | OC1A on PB1 (D9) | OC1A on PF5 (pad `E5`) |
| 4 | `C0BF3` | OC0B on PD5 (D5) | OC0B on PF3 (**not bonded on LQFP32**) |
| 3 | `C0AC0` | with `TCCR0B.C0AS`: 00 = OC0A on PD6, 01 = on PE4, 10 = on PC0, 11 = both |
| 2 | `SSB1` | SPI SS on PB2 | SPI SS on PB1 |
| 1 | `TXD6` | **TX on PD1 (D1)** | **TX on PD6 (D6)** |
| 0 | `RXD5` | **RX on PD0 (D0)** | **RX on PD5 (D5)** |

`PMX1` (0xED): `C3AC` moves OC3A off PF1, `C2BF7` moves OC2B to PF7, `C2AF6` moves OC2A to
PF6 — the last two land on pads that do not exist on LQFP32.

`PMX2` (0xF0): `XIEN` enables external clock input; **`E6EN` turns PE6 from AVREF into a
GPIO**; **`C6EN` turns PC6 from the reset input into a GPIO**; `STSC0`/`STSC1` are the
crystal-oscillator IO enables, set automatically by PMCR.

`TXD6`/`RXD5` are the useful ones: they free D0/D1 from the USB bridge by moving the UART to
D5/D6, which is how you keep a serial console *and* use D0/D1 as GPIO.

### 2.6 High-current drive (HDR, 0xE0)

Default drive is **12 mA** per pin. Six pads can be switched to **80 mA** push-pull:

| HDR bit | LQFP32 pad | Header |
|---|---|---|
| `HDR[0]` | PD5 | D5 |
| `HDR[1]` | PD6 | D6 |
| `HDR[2]` | PD1 ‖ PF1 | D1 / TX |
| `HDR[3]` | PD2 ‖ PF2 | D2 |
| `HDR[4]` | PE4 ‖ PF4 | PE4 |
| `HDR[5]` | PE5 ‖ PF5 | PE5 |

The databook warns twice: do not enable all six, and *"in case of QFP32 packing with only one
power port, it is suggested not to open and drive 4 circuits with large current load at the
same time"*. There is exactly one VCC pin and one GND pin on this package (pins 4 and 5) —
the limit is the bond wires, not the pads.

(The databook's own HDR bit-definition list is visibly copy-pasted wrong — it names PF5 twice
and never mentions PD5/PD6. The table above is the *other* table on the same page, headed
"HDR port / QFP48 / QFP32", which is self-consistent and matches the wolles-elektronikkiste
board notes: TX, D2, D5 and D6 are the four high-current pins reachable on the header.)

---

## 3. Board-level circuits

From the nulllab LQFP32-Nano schematic. Component designators are that schematic's.

### 3.1 USB and the serial bridge

- USB **Micro-B** (`USB1`), 5 V from VBUS.
- The SOP16 bridge (`U2`) is CH340G on the nulllab schematic; production MiniEVB boards carry
  **HT42B534-1** or **CH9340C**. Read the marking.
  - **HT42B534-1** is a USB **CDC** composite device (CDC for data, HID for VID/PID setup).
    It needs no vendor driver on macOS or Linux and enumerates as `/dev/tty.usbmodem*` or
    `/dev/ttyACM*`; Windows 7/8 want an INF file, Windows 10 does not. Its internal 3.3 V
    regulator is specified at 3.0–3.6 V with a 70 mA test current.
  - **CH340G / CH9340C** need the WCH VCP driver and enumerate as `/dev/tty.wchusbserial*`
    or `/dev/ttyUSB*`. A board that "does not appear on any port" is almost always a missing
    WCH driver.
  - The `Y1` 12 MHz crystal on the board belongs to the **bridge**, not the MCU. HT42B534
    integrates its own 12 MHz oscillator, so that footprint may be empty on Holtek boards.
- `R5` 10 kΩ pulls the MCU reset high; `B1` is the reset button; `C8` 100 nF is the DTR
  auto-reset coupling cap.
- Reported for MiniEVB boards: the **TX (D1) line has no pull-up**, which upsets some
  USB-UART bridges during upload. If uploads are unreliable and the board is otherwise fine,
  a 10 kΩ from D1 to VCC is the known fix. *(Community report, not from the schematic.)*

### 3.2 Power

| Rail | Source | Notes |
|---|---|---|
| VBUS | USB Micro-B | 5 V |
| +5V | VBUS through `D1` **MBR0502** Schottky, or `U1` **LT1117-5.0** from VIN | |
| VCC (MCU) | +5V | **≈ 4.6 V when powered from USB** — the diode drop |
| VIN | header | into the LT1117-5.0; needs ≥ ~6.2 V to stay in regulation |
| +3.3V | the USB bridge's internal regulator | *not* an MCU rail; the LQFP32 package has no AVCC/AGND pin and the board has no 3.3 V LDO of its own |

The MCU itself runs **1.8–5.5 V**, 0–32 MHz, −40…+85 °C, ~1 µA in the deepest sleep at 3.3 V.

The ≈4.6 V VCC is the single most consequential board fact: `analogRead()` against the
`DEFAULT` reference is referred to AVCC, so every reading is scaled by ~0.92 against the
5.000 V a Nano user assumes. Use `INTERNAL1V024`/`INTERNAL2V048`/`INTERNAL4V096`, or measure
VCC through the internal `V5D1` channel and scale (recipe 5).

### 3.3 LEDs

Red `D2` is the power indicator, wired to VCC on some revisions and not controllable. The
yellow LEDs are the Nano set: **L on D13/PB5, on = HIGH**, plus TX and RX driven by the USB
bridge. The TX/RX LEDs show *bridge* traffic and say nothing about D0/D1 driven by hand.

### 3.4 ICSP header

`ICSP1` is a 6-pin 2×3 header on the schematic, wired to the SPI pins as on a Nano. It is
**not** how you recover this chip — the LGT8F328P is programmed over **SWD** (PE0/PE2), which
is on the main header as SWC/SWD. See §12.

---

## 4. Clock tree

There is **no crystal on the MCU**. PB6/PB7 (XTAL1/XTAL2) go nowhere on this board and the
core does not even give them Arduino pin numbers. Everything runs off internal RC.

Four sources exist, selected through `PMCR` (0xF2):

| Source | Enable | Notes |
|---|---|---|
| 32 MHz RC (HFRC) | `PMCR[0]` | **the default at power-on**, ±1 % calibrated |
| 32 kHz RC (LFRC) | `PMCR[1]` | WDT clock and low-power modes |
| 400 kHz – 32 MHz external OSC | `PMCR[2]` | needs a crystal — none fitted |
| 32 kHz – 400 kHz external OSC | `PMCR[3]` | none fitted |

`PMCR[6:5]` selects the master: `00` internal RC (default), `01` external high-speed, `10`
internal 32 kHz RC, `11` external low-speed.

**At reset the part runs at 4 MHz**: the 32 MHz RC divided by 8, because `CLKPR` powers up as
`0x03`. The core then reprograms `CLKPR` from `F_DIV = F_OSC / F_CPU` (§9).

Register write protection, and the one place the databook contradicts itself: `PMCR[7]`
(`PMCE`) must be set before any other `PMCR` bit is written, and the follow-up write must land
**within 6 system clocks** per the clock chapter, **within 4** per the register summary. The
core's `sysClock()` writes the two bytes back to back, which satisfies either. `CLKPR[7]`
(`WCE`) guards `CLKPR` the same way with a 4-clock window.

Derived clocks worth knowing:

- **`E2P_clk` is fixed at 32 MHz ÷ 32 = 1 MHz and comes only from the 32 MHz HFRC.** The
  databook: *"If user need to use E2PCTL module to read internal programming FLASH or data
  FLASH memory, it is a must to enable internal 32 MHz oscillator in advance."* Switching to
  an external crystal and then disabling the RC breaks every EEPROM and flash-self-read
  operation.
- WDT runs off the 32 kHz LFRC by default, or 32 MHz ÷ 16 = 2 MHz.
- `CLKPR[5]` (`CKOEN0`) outputs the system clock on PB0 (D8); `CLKPR[6]` (`CKOEN1`) on PE5.

`CLKPS` division factors: 1, 2, 4, 8 (reset default), 16, 32, 64, 128, 256.

---

## 5. Memory map

| | LGT8F328P |
|---|---|
| Flash | 32 KB, ≥ 10,000 write/erase cycles (the English text says 10,000; the Chinese line beside it says 100,000) |
| Usable for the sketch | **29,696 B** — the bootloader keeps 3,072 B |
| SRAM | 2 KB |
| EEPROM | **none as such** — 0/1/2/4/8 KB emulated out of the same flash |
| Interrupt vectors | 2 instruction words each |

Data space is laid out as on an AVR: 32 register-file bytes, then 64 standard I/O
(`IN`/`OUT`), then 160 extended I/O at 0x60–0xFF (`LD`/`ST` only), then up to 2 KB of SRAM.
**Program flash is additionally mapped into data space from 0x4000 to 0xBFFF**, so `LD`/`LDS`
reaches constants without `LPM`.

### 5.1 EEPROM emulation — the part with teeth

There is no separate EEPROM array. The E2PCTL controller emulates one by page-swapping inside
main flash:

- The unit is a **1 KB flash page, and emulation needs two of them.** 1 KB of EEPROM costs
  **2 KB of flash**; 8 KB of EEPROM costs 16 KB — half the chip.
- Of each 1 KB, the last two bytes hold the page-valid flag and cells are 32 bits wide, so
  **1020 bytes are actually usable**, not 1024.
- Selected at runtime by `ECCR`: `lgt_eeprom_init(n)` with n ∈ {0, 1, 2, 4, 8}. The
  `EEPROM` object's constructor calls `lgt_eeprom_init()` — 1 KB — for you.
- **The contents are erased by every sketch upload**, including an update. This is the
  opposite of a real AVR, where the EESAVE fuse preserves EEPROM across programming. Anything
  a device must remember across a firmware update needs another home.
- Flash budgeting: PlatformIO's 29,696 B ceiling does **not** account for the E2PROM pages.
  With the default 1 KB EEPROM, keep the sketch under roughly 27.6 KB. *(Derived from the
  databook's FLASH-vs-E2PROM table plus the 3 KB bootloader; not measured.)*

Databook FLASH/E2PROM table for the 328P:

| E2PROM | Flash left for code |
|---|---|
| 0 KB | 32 KB |
| 1 KB | 30 KB |
| 2 KB | 28 KB |
| 4 KB | 24 KB |
| 8 KB | 16 KB |

(Those are before the bootloader's 3 KB.)

---

## 6. Analog subsystem

What makes this chip worth using instead of a Nano, and what the Arduino API hides.

- **ADC: 12 channels, 12 bit**, successive approximation. `analogRead()` returns **10 bits**
  until you call `analogReadResolution(12)` — the core ships `analog_resbit = 2` for
  Arduino compatibility and silently throws the bottom two bits away.
- Each `analogRead()` on this part performs **two** conversions: the core toggles
  `ADCSRC.SPN` to take a sign-inverted sample, averages the pair, then applies a gain-error
  correction (`pVal -= pVal >> 7`). It is therefore roughly twice as slow as the same call on
  an ATmega328P, and it is not a raw register read.
- **References**: `DEFAULT` = AVCC (≈4.6 V here), `EXTERNAL` = AVREF on PE6,
  `INTERNAL1V024`, `INTERNAL2V048`, `INTERNAL4V096`, all ±1 %.
  - `INTERNAL` is `#define`d to 3 = **`INTERNAL1V024`**. On an ATmega328P `INTERNAL` is
    1.1 V. Ported code that calibrates against 1.1 V is ~7 % out.
  - `INTERNAL2V56` is `#define`d to 2, the **same value as `INTERNAL2V048`**. Ported code
    asking for 2.56 V silently gets 2.048 V — a 25 % error with no diagnostic.
  - Selecting `INTERNAL4V096` also sets `ADCSRD.REFS2`; the core handles this.
- **Internal channels**: `analogRead(V5D1)` reads VCC × 1/5 and `analogRead(V5D4)` reads
  VCC × 4/5 (the core sets `ADCSRD |= 0x06` when it sees those pin numbers). Against
  `INTERNAL1V024` this is how you find the real supply. `IVREF` reads the reference itself.
- **DAC**: 8-bit, output on **PD4 = D4**. `analogWrite(4, v)` writes `DAL0` and produces an
  actual analog voltage; it does not produce PWM and it does not call `pinMode()` on that pin.
  `analogWrite(4, 0)` and `analogWrite(4, 255)` are *not* short-circuited to
  `digitalWrite()` the way they are for PWM pins.
- **Differential amplifier**: one programmable-gain input channel, ×1/8/16/32, positive
  inputs APP0 (PC0/A0) and APP1 (PC1/A1), negative inputs APN0 (PC2/A2), APN1 (PC3/A3),
  APN4 (PE0/SWC). Selected by `ADTMR.DIFS` with gain and routing in `DAPCR`. **The dbuezas
  core exposes no API for this** — it is registers only. Untested here.
- **Two analog comparators** (AC0/AC1) with a shared negative input on PD7 (ACXN) and a
  shared positive on PE1 (A6, ACXP); AC0 output can appear on PD2, AC1 output on PE5. AC
  inputs can be extended through the ADC mux (`ADCSRB.CEM*`).
- Sleep does **not** power analog blocks down. The databook is explicit: disable ADC, DAC,
  comparators and LVD in software before sleeping or they keep drawing current.

---

## 7. Reset, watchdog and the SWD kill switch

Six reset sources: power-on (POR), external (RESETN), watchdog, low-voltage (LVD, threshold
programmable through `VDTCR`, ±10–50 mV hysteresis, reset stretched ≥ 1 ms), software
(`VDTCR[6]`, pulse stretched 16 µs), and OCD (debugger only).

`MCUSR` (0x34) carries the flags — `PORF`, `EXTRF`, `BORF`, `WDRF`, `OCDRF`, `PDRF` — and one
bit that has no counterpart on any AVR:

> **`MCUSR[7]` = `SWDD`.** *"SWD port disable bit. Write logic one to shut down SWD port.
> After shut down of SWD port, debug and ISP operation is not possible."*

Consequences:

- The AVR idiom `MCUSR = 0;` is safe (bit 7 written as 0). `MCUSR |= something` that sets
  bit 7, or a blind `MCUSR = 0xFF` to clear flags, disables SWD.
- To avoid accidents the bit must be **written twice within 4 cycles** to take effect.
- Recovery from a firmware that sets it: **hold RESET low while powering the board up** so
  user code never runs, then SWD works again.
- Once SWD is off, PE0 and PE2 are ordinary GPIO. That is the only reason to do it.

The watchdog can be clocked from the 32 kHz LFRC or 32 MHz ÷ 16, times out up to 8 s, and has
interrupt / reset / interrupt-then-reset modes. **The core already disables it for you**:
`cores/lgt8f/main.cpp` installs `__patch_wdt()` in `.init3` which does `MCUSR = 0;
wdt_disable();` before `main()`. Unlike an Arduino Nano with the old bootloader, a sketch that
leaves the WDT running does not brick the board.

The external reset can be turned off (`PMX2.C6EN`) to reclaim PC6 as GPIO — after which
DTR auto-reset stops working and every upload needs the pin-hold trick above.

---

# Part II — development guide

## 8. Toolchain

```ini
[env:lgt8f328p]
platform = https://github.com/darkautism/pio-lgt8fx.git
board = LGT8F328P
framework = arduino
```

`platform = lgt8f` resolves the same platform from the PlatformIO registry; the git URL is
what the reference project used and what the figures below were built with.

Resolved packages for those figures:

| Package | Version |
|---|---|
| `lgt8f` platform | 1.0.3+sha.dea68b9 |
| `framework-lgt8fx` | 2.0.7 |
| `toolchain-atmelavr` | 1.70300.191015 |
| `tool-avrdude` | 1.60300.200527 |

`board = LGT8F328P` gives `build.variant = lgt8fx8p` (LQFP32), `build.mcu = atmega328p`,
`build.core = lgt8f`, `f_cpu`/`f_osc` = 32 MHz, `clock_source = 1`, `upload.protocol =
arduino`, `upload.speed = 57600`, `maximum_size = 29696`, `maximum_ram_size = 2048`.

Template build results (`pio run`, this platform):

| Variant | Flash | RAM |
|---|---|---|
| `--minimal` (Blink) | **1,106 B** — 3.7 % of 29,696 | **9 B** — 0.4 % of 2,048 |
| `--full` | **4,080 B** — 13.7 % | **204 B** — 10.0 % |

Libraries the core bundles: `E2PROM` (the EEPROM emulation, `#include <EEPROM.h>`), `SPI`,
`Wire`, `SoftwareSerial`, `HID`, `PMU` (sleep/power control), `LarduinoISP`,
`Rtc_Pcf8563`, `VUsbDevice`.

## 9. Clock configuration — the prescaler trap

`builder/frameworks/arduino.py` passes `F_CPU`, `F_OSC` and `CLOCK_SOURCE` as `-D` macros
from `board_build.f_cpu`, `board_build.f_osc` and `board_build.clock_source`. The core then
does, in `lgt8fx8x_clk_src()`:

```c
#define F_DIV (F_OSC / F_CPU)
CLKPR = 0x80;
/* ... CLKPR = 0x00 for F_DIV<=1, 0x01 for <=2, 0x02 for <=4, ... */
```

`F_OSC` describes the **oscillator**; on this board that is always the 32 MHz internal RC.
`F_CPU` describes the core. The prescaler is the ratio. So:

| `board_build.f_cpu` | `board_build.f_osc` | `F_DIV` | Core actually runs at | `F_CPU` claims |
|---|---|---|---|---|
| 32000000L | 32000000L | 1 | 32 MHz | 32 MHz ✅ |
| 16000000L | *unset / 32000000L* | 2 | 16 MHz | 16 MHz ✅ |
| 16000000L | **16000000L** | **1** | **32 MHz** | 16 MHz ❌ |
| 8000000L | 32000000L | 4 | 8 MHz | 8 MHz ✅ |

The third row is the trap, and it is what most copy-pasted "how to run at 16 MHz" snippets
contain. Nothing errors. `delay(1000)` waits 500 ms, `Serial.begin(115200)` produces 230400
baud, and every bit-banged protocol runs at double rate. **Verified**: building the template
with `f_cpu = f_osc = 16000000L` makes the preprocessor evaluate `F_OSC/F_CPU` to 1; building
with `f_cpu = 16000000L` alone gives 2.

`board_build.f_osc` changes only when a real external crystal is fitted, together with
`board_build.clock_source = 2`. Neither applies to this board.

## 10. Peripheral cookbook

Working code is in `recipes.md`; this is the map.

| Want | Do |
|---|---|
| Blink | recipe 3 — `LED_BUILTIN` = D13, on = HIGH |
| Full ADC resolution | `analogReadResolution(12)`; recipe 4 |
| The real supply voltage | `analogReference(INTERNAL1V024)` + `analogRead(V5D1)`; recipe 5 |
| Analog output | `analogWrite(4, v)` — the DAC; recipe 6 |
| PWM on D1/D2 | `analogWrite(1, v)` / `analogWrite(2, v)` — Timer3; recipe 7 |
| Drive an LED string / small motor directly | `HDR` bits; recipe 8 |
| Persistent settings | `EEPROM` + `lgt_eeprom_init(n)`; recipe 9 — and read §5.1 first |
| Serial console while D0/D1 are busy | `PMX0.TXD6`/`RXD5`; recipe 10 |
| Sleep | `PMU` library; recipe 11 |
| Use SWC/SWD as GPIO | `MCUSR` bit 7; recipe 12 — and read §7 first |

## 11. Core-specific gotchas

- `digitalPinHasPWM()` is wrong (§2.4). So is any library that uses it to pick a pin.
- `analogWrite(4, ...)` is a DAC write, so a "PWM fade" ported from a Nano produces a clean
  ramp instead — usually a pleasant surprise, until something downstream expected a square
  wave to drive a MOSFET gate.
- `INTERNAL2V56` silently means 2.048 V, `INTERNAL` means 1.024 V (§6).
- The E2PROM library warns at compile time on non-`__LGT8FX8P__` parts. If you see that
  warning, `build.variant` is wrong.
- `Servo` and `tone()` are not bundled by PlatformIO's core package. Add them via `lib_deps`
  if needed, and check what timer they grab: Timer1 is D9/D10, Timer2 is D3/D11 — the same
  arithmetic as a Nano, plus Timer3 on D1/D2 which no AVR `Servo` knows about.
- Interrupts: only D2 (INT0) and D3 (INT1) are external-interrupt pins, as on a Nano. Pin
  change interrupts cover 40 pins across five `PCMSK` registers.
- SRAM is 2 KB with heap and stack sharing it and no MPU — the Nano arithmetic applies
  unchanged. `F()` every literal, `PROGMEM` every table, watch free RAM.

## 12. Flashing and recovery

**Normal path.** USB Micro-B, DTR auto-reset, `avrdude -c arduino` at 57600:

```sh
pio run -t upload -t monitor
```

The board enumerates per §3.1 (CDC on HT42B534, WCH VCP on CH340G/CH9340C). The PlatformIO
builder always appends `-D` (skip chip erase — the bootloader erases per page). If avrdude
reports a verification mismatch on a board that runs correctly afterwards, add
`upload_flags = -V` to skip read-back.

Upload failure checklist, in order:

1. Missing USB-serial driver (CH340/CH9340 only) — no port at all.
2. Wrong port.
3. Something wired to D0/D1 — disconnect it. Also check nothing enabled Timer3 PWM on D1.
4. Speed: try `upload_speed = 115200` or `19200`; the board definition's 57600 is the common
   case but bootloaders vary.
5. No auto-reset (some bridges, or `PMX2.C6EN` set by a previous sketch): press RESET as the
   upload starts.
6. Missing TX pull-up (§3.1).

**Recovery over SWD.** The LGT8F328P has no ICSP-style AVR programming; it uses its
two-wire SWD port. A second Arduino running the core's `LarduinoISP` example is the standard
programmer:

| Programmer board | → | Target |
|---|---|---|
| D13 | → | SWC (PE0) |
| D12 | → | SWD (PE2) |
| D10 | → | RST |
| VCC, GND | → | VCC, GND |

Put a 10 µF cap between the programmer's RESET and VCC so it does not reset mid-session. Then
`Burn Bootloader` (Arduino IDE) or `Upload using Programmer` restores a board whose
bootloader is gone. Note: a purple LQFP32 board with a **CH9340C** cannot itself be used as
the LarduinoISP programmer.

If a sketch set `MCUSR[7]`, SWD is dead until you **hold RESET low through power-up** so the
sketch never runs (§7).

## 13. Pitfall table

| Symptom | Cause | Fix |
|---|---|---|
| Everything runs at double speed: `delay()` half as long, serial garbled | `board_build.f_osc` set to 16 MHz along with `f_cpu`, so `F_DIV` = 1 | leave `f_osc` at 32000000L; §9 |
| Sketch behaves as if the clock is 4 MHz | `CLOCK_SOURCE` not defined, so the core leaves `CLKPR` at its own default | set `board_build.clock_source = 1` |
| `analogRead()` returns 0–1023 on a "12-bit" ADC | core default `analogReadResolution` is 10 | call `analogReadResolution(12)` |
| ADC readings ~8 % high against a known voltage | `DEFAULT` reference is AVCC ≈ 4.6 V, not 5 V (protection diode) | use an internal reference, or measure VCC via `V5D1`; §6 |
| Ported sketch's 2.56 V reference reads 25 % low | `INTERNAL2V56` == `INTERNAL2V048` on this core | use `INTERNAL4V096` or rescale |
| `analogWrite(4, x)` gives a DC level, not PWM | D4 is the DAC output | use another pin, or embrace it; §2.4 |
| Serial output stops the moment PWM starts | `analogWrite(1, …)` took the TX pad for Timer3 OC3A | move the PWM, or move the UART with `PMX0.TXD6` |
| EEPROM contents gone after a firmware update | emulated EEPROM lives in program flash and is erased by every upload | §5.1 |
| Sketch overflows / corrupts itself near 28 KB | E2PROM pages take flash that PlatformIO's 29,696 B budget ignores | shrink the sketch or `lgt_eeprom_init(0)` |
| EEPROM and flash self-read stop working after a clock change | `E2P_clk` comes only from the 32 MHz RC, which was disabled | keep `PMCR[0]` set; §4 |
| Board no longer enumerates and SWD does not respond | sketch wrote `MCUSR` bit 7 (`SWDD`) | hold RESET low through power-up; §7 |
| Two outputs on the same header pin fight each other | LQFP32 bonds PD1‖PF1, PD2‖PF2, PE4‖PF4, PE5‖PF5 | §2.2 |
| Board browns out when driving a load | four or more 80 mA pads enabled on a package with one VCC pin | §2.6 |
| Upload works, sketch never starts | `PMX2.C6EN` set: reset pin is GPIO, auto-reset gone | hold RESET low through power-up, reflash |
| No serial port on the host at all | CH340/CH9340 driver missing | install the WCH VCP driver |
