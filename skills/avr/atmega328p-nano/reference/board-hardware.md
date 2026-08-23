# Arduino Nano (A000005) hardware reference

> Sources:
> - `A000005-datasheet.pdf` — Arduino Nano User Manual, SKU A000005, rev 4 (12/06/2025, modified 17/08/2026), Arduino S.r.l.
> - `Arduino_nano_specs.pdf` — "Arduino Nano: pinout, features, datasheet, IDE and simulation", Richard Electronics blog (24 Oct 2024).
> - `Arduino_nano_specs2.pdf` — "Arduino Nano Board Guide (Pinout, Specifications, Comparison)", Makerguides (published 2020-09-29, updated 2026-02-17).
> - Online: https://docs.arduino.cc/hardware/nano
>
> Board revision this was verified on: Arduino Nano v3.x / official A000005 (ATmega328P, Mini-B USB, post-2018 units ship with the new Optiboot bootloader; see §7 and §12). The Blink sketch that became `template/variants/minimal/main.cpp` ran on this board.
>
> Where the official manual and this file disagree, this file wins — the manual's pin tables contain errors, listed in §7.

---

## 1. Overview

The Arduino Nano is a breadboard-friendly, open-source development board built around the 8-bit **Microchip/Atmel ATmega328P** AVR microcontroller running at **16 MHz** (up to 16 MIPS). It offers roughly the same functionality as the Arduino Uno / Duemilanove but in a minimal 18 × 45 mm footprint with a Mini-B USB port instead of a full-size USB-B connector and no DC barrel jack.

| Property | Value |
|---|---|
| SKU | A000005 |
| Microcontroller | ATmega328P (8-bit AVR, Harvard architecture) |
| Clock speed | 16 MHz (up to 16 MIPS) |
| Logic level | 5 V |
| Flash memory | 32 KB, of which 2 KB reserved for the bootloader (≈30 KB for the sketch) |
| SRAM | 2 KB |
| EEPROM | 1 KB |
| General purpose registers | 32 × 8 bit |
| Digital I/O pins | 20 usable as digital (D0–D13 + A0–A5; 22 nominal incl. A6/A7 which are analog-input-only) |
| Analog inputs | 8 (A0–A7, 10-bit ADC) |
| PWM outputs | 6 (D3, D5, D6, D9, D10, D11, 8-bit) |
| External interrupts | 2 hardware (D2/INT0, D3/INT1) |
| UART | 1 (D0/D1, routed to USB via FTDI USB-to-TTL chip) |
| I2C (TWI) | 1 (A4/SDA, A5/SCL) |
| SPI | 1 (D10/SS, D11/MOSI, D12/MISO, D13/SCK; mirrored on ICSP) |
| Real Time Counter | On MCU, with separate oscillator capability (unusable on this board, see §5) |
| Operating voltage | 5 V |
| Input voltage (VIN) | 7–15 V unregulated per official manual (7–12 V recommended per Richard Electronics; 6–20 V absolute limits per Makerguides) |
| DC current per I/O pin | 40 mA absolute (20 mA recommended) |
| DC current on 3.3 V pin | 50 mA max |
| Power consumption | ~19 mA typical (board, active); official manual lists values as "TBC" |
| Sleep modes | Idle, ADC Noise Reduction, Power-save, Power-down, Standby, Extended Standby |
| USB | Mini-B (the manual and one blog say "Mini-B"; Makerguides also calls it Mini-B — note some sources mistakenly write "Micro-B") |
| ICSP header | Yes (6-pin) |
| LEDs | 4: PWR (ON), L (D13), TX, RX (driven by the FTDI chip) |
| PCB size | 18 × 45 mm |
| Weight | 7 g |
| Power jack | None (power via USB, VIN pin, or +5V pin) |
| Operating temperature | −40 °C … +85 °C (conservative board limits) |
| Certifications | CE (2014/53/EU), RoHS 2/3, REACH, FCC Part 15, IC RSS |

**History / revisions**

- 2008: Original Nano released (designed by Gianluca Martino / Gravitech), based on ATmega168P (16 KB flash).
- 2010: Switch to ATmega328P (32 KB flash) — same MCU as the Uno.
- 2012–2017: Nano R3 era — improved compatibility, I2C pins in the pinout, ATmega328 performance.
- Since Jan 2018: genuine boards ship with a new (Optiboot) bootloader; older boards and most clones use the "Old Bootloader" (see §12).
- The original Nano has components on **both** sides of the PCB; newer Nano family boards are top-side only.
- Board is sold with or without pre-soldered headers (headers included in box), with 2.54 mm through-holes **and castellated pads**, so it can be soldered directly onto a carrier PCB.

**Target areas** (per official manual): Maker, Security, Environmental, Robotics and Control Systems.

**Nano family comparison** (context; all share the 18 × 45 mm form factor — only the classic Nano, column 2, is covered by this skill):

| Property | Nano (this board) | Nano Every | Nano 33 IoT | Nano 33 BLE | Nano 33 BLE Sense |
|---|---|---|---|---|---|
| Microcontroller | ATmega328P | ATmega4809 | SAMD21 Cortex-M0+ | nRF52840 | nRF52840 |
| Operating voltage | 5 V | 5 V | 3.3 V | 3.3 V | 3.3 V |
| Input voltage (VIN) | 6–20 V | 7–21 V | 5–21 V | 5–21 V | 5–21 V |
| Clock | 16 MHz | 20 MHz | 48 MHz | 64 MHz | 64 MHz |
| Flash | 32 KB | 48 KB | 256 KB | 1 MB | 1 MB |
| RAM | 2 KB | 6 KB | 32 KB | 256 KB | 256 KB |
| Current per pin | 40 mA | 40 mA | 7 mA | 15 mA | 15 mA |
| PWM pins | 6 | 5 | 11 | all | all |
| Radio | — | — | WiFi + BT (NINA-W102) | BT (NINA B306) | BT (NINA B306) |
| Sensors | — | — | LSM6DS3 6-axis IMU | LSM9DS1 9-axis IMU | IMU + mic, gesture, light, pressure, temp/humidity |
| USB | Mini-B | Micro | Micro | Micro | Micro |

Related products listed by Arduino: Nano 33 BLE (ABX00030), Nano 33 IoT (ABX00027), Micro (A000093).

## 2. Pin map

Two 1×15 headers, 2.54 mm (0.1") pitch, 30 pins total. Pin numbering below follows the standard physical count (USB at the top): pins 1–16 digital side, pins 17–30 analog side.

> Note: the official manual's pin tables contain errors (they omit the RESET/GND pair at pins 3–4 and the AREF pin, duplicate "12" for GND/VIN, describe pin 1 "+3V3" as "5V USB Power", and list PB0–PB5 as "Serial Wire Debug"). The table below is the corrected, physical pinout.

### 2.1 Digital side (pins 1–16)

| # | Silkscreen | MCU pin | Arduino API | Functions | Polarity / level | Shared / conflicts |
|---|---|---|---|---|---|---|
| 1 | D1/TX | PD1 (TXD) | `1` | GPIO, UART TX | Active-high logic; UART idles HIGH | **Shared with USB serial** — unusable while `Serial` is in use |
| 2 | D0/RX | PD0 (RXD) | `0` | GPIO, UART RX | idles HIGH | **Shared with USB serial** |
| 3 | RESET | RESET | — | MCU reset, active-low, 10 kΩ pull-up, reset button, auto-reset via DTR | **Active LOW** | Also appears at pin 28 |
| 4 | GND | — | — | Ground | — | — |
| 5 | D2 | PD2 (INT0) | `2` | GPIO, external interrupt INT0 | — | `attachInterrupt` capable |
| 6 | D3 (~) | PD3 (INT1/OC2B) | `3` | GPIO, external interrupt INT1, PWM 8-bit (Timer2, ~490 Hz) | — | INT1 + PWM on one pin |
| 7 | D4 | PD4 | `4` | GPIO | — | — |
| 8 | D5 (~) | PD5 (OC0B) | `5` | GPIO, PWM 8-bit (Timer0, ~980 Hz) | — | Timer0 = `millis()`/`delay()` timer |
| 9 | D6 (~) | PD6 (OC0A) | `6` | GPIO, PWM 8-bit (Timer0, ~980 Hz) | — | Timer0 = `millis()`/`delay()` timer |
| 10 | D7 | PD7 | `7` | GPIO | — | — |
| 11 | D8 | PB0 (ICP1/CLKO) | `8` | GPIO | — | — |
| 12 | D9 (~) | PB1 (OC1A) | `9` | GPIO, PWM 8-bit (Timer1, ~490 Hz) | — | Timer1 used by `Servo` library |
| 13 | D10 (~) | PB2 (OC1B/SS) | `10` | GPIO, PWM 8-bit (Timer1), SPI slave-select | — | PWM + SPI SS conflict; Timer1/Servo |
| 14 | D11 (~) | PB3 (OC2A/MOSI) | `11` | GPIO, PWM 8-bit (Timer2), SPI MOSI | — | PWM + SPI conflict; Timer2 used by `tone()` |
| 15 | D12 | PB4 (MISO) | `12` | GPIO, SPI MISO | — | SPI |
| 16 | D13 | PB5 (SCK) | `13` | GPIO, SPI SCK, **LED_BUILTIN ("L")** | LED on = HIGH | SPI + onboard LED; LED + series resistor load can affect SPI as input |

### 2.2 Analog side (pins 17–30)

| # | Silkscreen | MCU pin | Arduino API | Functions | Polarity / level | Shared / conflicts |
|---|---|---|---|---|---|---|
| 17 | 3V3 | regulator output | — | 3.3 V rail from onboard regulator, **50 mA max** | — | Not GPIO; official manual mislabels it "5V USB Power" |
| 18 | AREF | AREF | — | External ADC reference input | Must **not exceed 5 V** | Conflicts with internal references if driven while `analogReference()` ≠ EXTERNAL |
| 19 | A0 | PC0 (ADC0) | `A0` / `14` | Analog input / digital GPIO | — | — |
| 20 | A1 | PC1 (ADC1) | `A1` / `15` | Analog input / digital GPIO | — | — |
| 21 | A2 | PC2 (ADC2) | `A2` / `16` | Analog input / digital GPIO | — | — |
| 22 | A3 | PC3 (ADC3) | `A3` / `17` | Analog input / digital GPIO | — | — |
| 23 | A4 | PC4 (ADC4/SDA) | `A4` / `18` | Analog input / digital GPIO / **I2C SDA** | — | I2C (Wire) conflicts with analog/digital use |
| 24 | A5 | PC5 (ADC5/SCL) | `A5` / `19` | Analog input / digital GPIO / **I2C SCL** | — | I2C (Wire) |
| 25 | A6 | ADC6 | `A6` / `20` | **Analog input only** — no digital port behind it | — | Cannot be `pinMode`/`digitalWrite`/`digitalRead` |
| 26 | A7 | ADC7 | `A7` / `21` | **Analog input only** | — | Same restriction as A6 |
| 27 | 5V | +5V rail | — | Regulated 5 V output rail / regulated 5 V supply input | — | Feeding this pin **bypasses the regulator** — never exceed 5 V |
| 28 | RESET | RESET | — | MCU reset, active-low, parallel to pin 3 | **Active LOW** | Duplicate of pin 3 |
| 29 | GND | — | — | Ground | — | — |
| 30 | VIN | LDO input | — | Unregulated input to the 5 V regulator (7–15 V per manual) | — | Auto power-selection with USB (highest voltage source wins) |

**Electrical characteristics common to all GPIO (D0–D13, A0–A5):**

- 5 V push-pull outputs; **not 3.3 V-tolerant-safe for 3.3 V-only peripherals without level shifting** (outputs drive 5 V).
- 40 mA absolute max per pin, 20 mA recommended.
- Internal pull-up resistors of **20–50 kΩ**, disconnected by default — enable with `pinMode(pin, INPUT_PULLUP)`.
- A0–A5 work as full digital pins via the `A0`…`A5` or `14`…`19` aliases; **A6/A7 are the exception** (analog-input-only, unlike the Uno which lacks them entirely).

**Port-register mapping** (for direct AVR port manipulation): D0–D7 → `PORTD`, D8–D13 → `PORTB` (PB0–PB5), A0–A5 → `PORTC` (PC0–PC5). PB6/PB7 are consumed by the 16 MHz crystal, PC6 by RESET; ADC6/ADC7 (A6/A7) have no port register.

## 3. Connectors and headers

### 3.1 USB — Mini-B

- Used for programming (via bootloader) and power; also drives the TX/RX LEDs through the FTDI USB-to-TTL serial chip (FT23x family on genuine boards).
- Requires a Mini-B USB cable (the manual sometimes writes "Micro-B" — physically it is Mini-B on the classic Nano).
- Auto-resets the board on serial-port open (DTR → capacitor → RESET), which triggers the bootloader.

### 3.2 ICSP header (6-pin, 2×3, 2.54 mm)

Located on the bottom of the board.

| Pin | Name | Function |
|---|---|---|
| 1 | MISO | SPI master-in-slave-out (mirrors D12) |
| 2 | +5V | Supply voltage |
| 3 | SCK | SPI clock (mirrors D13) |
| 4 | MOSI | SPI master-out-slave-in (mirrors D11) |
| 5 | RESET | Reset, **active LOW** |
| 6 | GND | Ground |

- SS is **not** present on this header (it is bit-banged on D10 as usual for AVR SPI-master operation).
- Used to program the MCU with ArduinoISP or a dedicated programmer — this **bypasses the bootloader** (see §12).
- Orientation gotcha: pin 1 is marked on the board; community reports that some published diagrams rotate the header 180°. Verify the pin-1 dot (outer edge) before connecting a programmer.

### 3.3 LEDs

| LED | Meaning |
|---|---|
| ON (PWR) | Board is powered |
| L (LED_BUILTIN) | Connected to D13 — HIGH = on |
| TX | Flashes on USB-serial transmit (FTDI side; **not** for arbitrary D1 traffic) |
| RX | Flashes on USB-serial receive |

### 3.4 Mechanics

- 18 × 45 mm, 7 g, 30 pins in two 15-pin rows at 2.54 mm pitch — spans a standard solderless breadboard center.
- Through-hole pads + castellated edge pads allow direct soldering onto a carrier PCB.
- Components on both top and bottom sides on the original Nano.
- Reset button on the top side.

## 4. Power tree

The board can be powered from three sources; if several are connected, the **highest-voltage source is automatically selected**:

```
Mini-B USB (5 V)  ──┐
                     ├── auto-select ──> +5V rail ──> ATmega328P VCC, +5V pin (27)
VIN pin (7–15 V) ────┘        │                          │
   └──> 5 V LDO ──────────────┘                          └──> 3.3 V regulator ──> 3V3 pin (17), 50 mA max
```

- **Mini-B USB**: primary method; 5 V from the host/adapter (also used for programming).
- **VIN (pin 30)**: 7–15 V unregulated external supply (official manual). Richard Electronics states 7–12 V recommended; Makerguides gives 6–20 V as the absolute input range. Regulated down to 5 V by an LDO.
- **+5V pin (pin 27)**: a regulated 5 V source can be injected here. **This bypasses the voltage regulators** and is not recommended — the voltage must be stable and must never exceed 5 V.
- **3V3 (pin 17)**: output only, from a dedicated onboard 3.3 V regulator, limited to **50 mA**. Suitable for low-voltage sensors; not sufficient for power-hungry modules.
- The MCU itself runs at 5 V (VCC = 5 V); there is no configurable "supply mode" — all GPIO are 5 V logic.
- Typical active consumption: ~19 mA (whole board). Official manual power table: "TBC" (to be confirmed) for both USB VCC and VIN inputs.
- Conservative thermal limits for the whole board: −40 °C to +85 °C. (A separate FCC-test note limits the EUT to −20…+80 °C.)

## 5. Clock sources

- **Main clock: 16 MHz quartz crystal** on XTAL1/XTAL2 (MCU pins PB6/PB7) with onboard load capacitors. This is why PB6/PB7 are not available as GPIO. The crystal times all serial protocols (UART/I2C/SPI) and instruction execution.
- No PLL, no internal-RC calibration needed in software; `F_CPU = 16000000UL` is fixed by the Arduino core.
- **Real Time Counter with separate oscillator** is an ATmega328P feature, but on this package the TOSC1/TOSC2 function is multiplexed with XTAL1/XTAL2 — since those carry the 16 MHz crystal, the asynchronous RTC/Timer2 operation is **not usable** on the Nano.
- `millis()`/`micros()`/`delay()` are derived from Timer0 (prescaler 64 → 1.024 ms tick with software correction); `delayMicroseconds()` is cycle-counted.
- Fuse set PlatformIO actually burns for this board (`nanoatmega328new.json` / `nanoatmega328.json`, identical for both): **lfuse=0xFF, hfuse=0xDA, efuse=0xFD**, lock=0x0F. `avrdude -U efuse:r:-:h` on a genuine or correctly-fused clone reads back **0xFD**, not the 0x05 sometimes quoted elsewhere — only the low 3 bits (BODLEVEL, brown-out at 2.7 V) are implemented on the ATmega328P's extended fuse byte, per `avrdude.conf`'s bit template for `efuse` (`x x x x x i i i`); the top 5 bits are unimplemented and always read back as 1. 0x05 and 0xFD are electrically the *same* fuse (BODLEVEL=101); a verify step that expects 0x05 will report a false mismatch against a correctly-programmed chip.

## 6. Memory map

AVR Harvard architecture: program flash, data SRAM and EEPROM are in **separate address spaces**. There is **no external memory bus, no XIP and no DMA controller** — everything is CPU-driven.

| Region | Size | Address range (ATmega328P) | Notes |
|---|---|---|---|
| Flash (program) | 32 KB (16 K × 16-bit words) | 0x0000–0x7FFF (bytes) | Application: 0x0000–0x77FF (**30,720 bytes** — matches `upload.maximum_size` in both board JSONs exactly). Boot section: 0x7800–0x7FFF (2 KB reserved) |
| Boot section | 2 KB reserved by the hfuse (BOOTSZ=00 from hfuse=0xDA) | 0x7800–0x7FFF | Bootloader; entered after reset, times out to the application |
| SRAM (data) | 2 KB | 0x0100–0x08FF in data space | Registers R0–R31 at 0x00–0x1F; I/O at 0x20–0x5F (0x00–0x3F in `IN`/`OUT` address space); extended I/O 0x60–0xFF; stack starts at 0x08FF and grows downward — usable heap+stack+globals ≈ 2 KB total |
| EEPROM | 1 KB | 0x000–0x3FF (not memory-mapped) | Accessed via `EEPROM` library or EEAR/EEDR/EECR registers |

Practical consequences:

- "DMA-reachable" = everything the CPU can address (there is no DMA); SPI/I2C/UART transfers are interrupt- or polling-driven and consume CPU time.
- No XIP: code executes only from internal flash; constants live in flash and must be read via `pgm_read_*` / `F()` macro (the `F()` macro is essential to avoid copying string literals into SRAM).
- All SRAM (globals, heap, stack) is reachable by all peripherals through the CPU.

## 7. Vendor SDK and examples

- **Arduino AVR core** (bundled with the Arduino IDE, package `Arduino AVR Boards`): board definition `arduino:avr:nano`, variants for ATmega328P / ATmega328P (Old Bootloader) / ATmega168P. In PlatformIO: `platform = atmelavr`, `board = nanoatmega328new` (Optiboot) or `nanoatmega328` (old bootloader) — **not** `nanoatmega328p`, which is not a registered board id and fails `pio run` with "Unknown board ID" (see §8).
- **Tools supported by the board** (per official manual): Arduino Desktop IDE (https://www.arduino.cc/en/software), Arduino CLI, Arduino Cloud Editor (https://create.arduino.cc/editor, guide: https://docs.arduino.cc/arduino-cloud/guides/editor/).
- **Examples**: IDE "File → Examples" menu and Arduino Documentation (https://docs.arduino.cc/hardware/nano). Key built-ins for this board: `01.Basics → Blink` (the template's minimal variant is exactly this), `Communication`, `Analog`, `Digital`, and `11.ArduinoISP` for reflashing via ICSP.
- **Online resources**: Project Hub (Nano projects: `part_id=11332`), Library Reference (https://www.arduino.cc/reference/en/libraries/), store (https://store.arduino.cc/).
- **Simulation tools** (per Richard Electronics): Proteus, SimulIDE, Tinkercad Circuits, Fritzing (Fritzing part files exist for the Nano).
- **Open-source hardware**: official schematics and Fritzing models are published (Nano Schematics.pdf linked from Makerguides).

**What is wrong with the official material** (errata relevant to this board):

- The A000005 manual's connector tables omit the RESET/GND pair at digital pins 3–4 and the AREF pin, duplicate row number "12", and describe pin 1 (3V3) as "5V USB Power".
- The "ATmega328" pin table lists PB0–PB5 as "Serial Wire Debug" — the ATmega328P has **no SWD**; those pins are D8–D13 (SPI + PWM + LED).
- A6/A7 are described as "/GPIO" — they are analog-input-only.
- Power consumption figures are "TBC"; VIN range conflicts across sources (7–15 V vs 7–12 V vs 6–20 V) as noted in §4.
- The manual mixes "Mini-B" and "Micro-B" USB naming.

---

# Part II — development guide

## 8. Toolchain and project configuration

**Arduino IDE (desktop)**

1. Install Arduino IDE from https://www.arduino.cc/en/software (the `Arduino AVR Boards` core is included).
2. Connect the board with a **Mini-B USB** cable; the FTDI VCP driver installs automatically (genuine boards). Clones with a CH340 USB chip need the WCH driver.
3. `Tools → Board → Arduino AVR Boards → Arduino Nano`.
4. `Tools → Processor`:
   - **ATmega328P** — genuine boards purchased after January 2018 (new Optiboot bootloader).
   - **ATmega328P (Old Bootloader)** — older genuine boards and most clones (upload speed drops from 115200 to 57600 baud; use this if uploads fail).
   - ATmega168P — for the original 2008-era Nano.
5. `Tools → Port` → the board's COM port (unplug/replug to identify it).
6. Write/load the sketch and click **Upload** — the IDE opens the serial port, auto-reset fires, and the bootloader receives the image via UART.

**PlatformIO** (what `template/` uses): `platform = atmelavr`, `board = nanoatmega328new`, `framework = arduino`, `monitor_speed = 115200`. For old-bootloader boards, switch to `board = nanoatmega328` — its `upload.speed` is 57600 by board definition, so `upload_speed` does not need to be set by hand (see §12). Build with `pio run`, upload with `pio run -t upload -t monitor`.

**Alternatives**: Arduino CLI (`arduino-cli compile --fqbn arduino:avr:nano` / `arduino-cli upload -p COMx --fqbn arduino:avr:nano`; add `--board-options processor=atmega328pold` for old-bootloader boards), Arduino Cloud Editor (browser, always up to date, sketches in the cloud, needs the plugin only).

**Simulation before hardware**: Proteus, SimulIDE, Tinkercad Circuits, Fritzing.

## 9. Clock configuration

- The clock is **fixed by hardware** (16 MHz crystal + fuses); there is no runtime clock-tree configuration. All timing APIs assume `F_CPU = 16 MHz`.
- `delay()`, `delayMicroseconds()`, `millis()`, `micros()` are calibrated for 16 MHz — changing the clock (e.g., burning different fuses for 8 MHz internal RC) silently breaks UART timing and all Arduino timing unless the board definition is changed too.
- PWM base frequencies are timer-dependent: D5/D6 ≈ 980 Hz (Timer0 — do not reconfigure without losing `millis()`), D3/D9/D10/D11 ≈ 490 Hz (Timer1/Timer2 — safe to reconfigure if you accept losing `Servo`/`tone()` respectively).
- Baud-rate-sensitive software (SoftwareSerial, one-wire, DHT, NeoPixel) relies on 16 MHz cycle timing — it degrades if interrupts are blocked or the clock is changed.

## 10. Peripheral cookbook

```cpp
// GPIO + internal pull-up (20-50 kOhm)
pinMode(3, OUTPUT);
pinMode(4, INPUT_PULLUP);

// Digital use of analog pins (A0-A5 only — NOT A6/A7)
pinMode(A0, OUTPUT);
digitalWrite(A0, HIGH);

// External interrupts (only D2, D3)
attachInterrupt(digitalPinToInterrupt(2), handler, FALLING); // RISING/FALLING/CHANGE/LOW

// ADC: 10-bit, 0-5V, ~100 us per analogRead()
int v = analogRead(A0);            // works on A0..A7
analogReference(EXTERNAL);         // use AREF pin (must be <= 5V; set BEFORE applying ext. voltage)
// References: DEFAULT = 5V (AVcc), INTERNAL = 1.1V, EXTERNAL = AREF pin

// PWM (8-bit) on D3, D5, D6, D9, D10, D11
analogWrite(9, 128);

// UART (D0/D1, shared with USB)
Serial.begin(115200);

// I2C (A4=SDA, A5=SCL)
Wire.begin();

// SPI (D10=SS, D11=MOSI, D12=MISO, D13=SCK)
SPI.begin();

// EEPROM
#include <EEPROM.h>
EEPROM.put(0, myStruct);

// Servo (Timer1, D9/D10 PWM is lost while in use)
#include <Servo.h>

// tone() uses Timer2 (conflicts with D3/D11 PWM)
tone(8, 440);

// Sleep modes: Idle, ADC Noise Reduction, Power-save, Power-down, Standby, Extended Standby
#include <avr/sleep.h>
set_sleep_mode(SLEEP_MODE_PWR_DOWN);
sleep_mode();

// Watchdog
#include <avr/wdt.h>
wdt_enable(WDTO_8S);
```

Timer-to-resource map (for conflict planning): Timer0 → `millis()`/`delay()` + PWM D5/D6; Timer1 → PWM D9/D10 + `Servo`; Timer2 → PWM D3/D11 + `tone()`.

## 11. Core-specific gotchas

- **2 KB SRAM is the hard wall.** String handling, large buffers and recursion crash mysteriously (heap meets stack). Use `F()` for constants, `pgm_read_byte` for flash tables, and watch free RAM. There is no MPU to catch the overflow.
- **Harvard architecture**: constants do not "just work" as pointers — flash data needs `PROGMEM`; conversely, self-programming (writing flash from the sketch) requires boot-section code (not available to a normal app other than via the bootloader protocol).
- **No cache/DMA concerns** (there is neither), but every byte moved by SPI/I2C costs CPU cycles; long ISR or `noInterrupts()` sections break UART timing and `millis()` accuracy.
- **A6/A7 have no digital port** — `pinMode`/`digitalRead` on them silently misbehave. They only feed the ADC mux.
- **D0/D1 are wired to the FTDI chip**: using them as GPIO while `Serial` is active causes garbage on both sides; the TX/RX LEDs show USB traffic only.
- **D13 drives the onboard LED** (through a series resistor on classic boards) — as an input or SPI line the LED load can pull the line.
- **5 V everywhere**: outputs drive 5 V into a 3.3 V-only peripheral without level shifting = damage. The 3V3 pin supplies at most 50 mA.
- **AREF must not exceed 5 V**, and never drive AREF externally while an internal reference is selected.
- **Bootloader era matters** (see §12): wrong `Tools → Processor` choice is the single most common upload failure. Old-bootloader units can boot-loop if the app leaves the watchdog enabled (the old ATmegaBOOT does not disable it on entry; Optiboot does).
- **Auto power selection**: with USB and VIN both attached, the higher voltage source wins — the USB port may then be back-fed; avoid powering VIN while debugging over USB unless VIN < 5 V.
- **Datasheet pin-table errata** (see §7) — always cross-check the pinout in §2 of this file against silkscreen, not the A000005 tables.
- **ICSP orientation**: verify pin 1 (outer edge) — some published diagrams are rotated 180°.
- **Temperature range discrepancy**: recommended −40…+85 °C vs the FCC note's −20…+80 °C; stay inside the tighter band for certified operation.

## 12. Flashing and recovery

**Normal path — UART bootloader**

- Reset → bootloader runs (~1–2 s window on old ATmegaBOOT, shorter on Optiboot) → listens on D0/D1 at 57600 (old) / 115200 (new) baud → receives the Intel HEX image and self-programs flash → jumps to the application.
- Auto-reset via the FTDI DTR line makes manual button pressing unnecessary; pressing RESET at the right moment is the manual fallback.

**Upload failures — checklist**

1. Wrong `Tools → Processor` (old vs new bootloader) — "not in sync" errors: toggle between ATmega328P and ATmega328P (Old Bootloader). In PlatformIO: switch the `board` id itself, `nanoatmega328new` (Optiboot, 115200 baud) ↔ `nanoatmega328` (old ATmegaBOOT, 57600 baud) — the upload speed is fixed by each board definition, not a separate setting to tune.
2. Wrong COM port / missing FTDI or CH340 driver.
3. Something wired to D0/D1 — disconnect during upload.
4. Sketch that crashes instantly or blocks interrupts — use ICSP (below) or time the RESET press.

**ICSP path (bypasses the bootloader entirely)**

- 6-pin ICSP header (§3.2) + `File → Examples → 11.ArduinoISP` on a second Arduino, or a USBasp/AVR-ISP programmer, or `avrdude` directly.
- `Tools → Burn Bootloader` both (re)writes the bootloader and sets the fuses (lfuse=0xFF, hfuse=0xDA, efuse=0xFD — see §5) — this is also the recovery route for a board whose fuses were bricked or whose bootloader was overwritten.
- The ICSP route also works for ATmega168P-era boards and for uploading sketches without any bootloader (gains back the reserved 2 KB).
- **Optiboot itself is only 512 B** (verified: `optiboot_atmega328.hex` spans exactly 0x7E00–0x7FFF), but the hfuse's BOOTSZ bits reserve the full 2 KB (0x7800–0x7FFF) regardless — the 1.5 KB gap at 0x7800–0x7DFF sits erased and unreachable by the application. Reclaiming it means burning a smaller BOOTSZ setting and re-linking Optiboot at the new offset yourself; neither the Arduino IDE nor `Burn Bootloader` will do this for you, and PlatformIO's `nanoatmega328new` board definition does not expose it. The old ATmegaBOOT (`nanoatmega328`), by contrast, genuinely fills all 2 KB (0x7800–0x7FFF), so there is no equivalent slack to reclaim on that board id.

**Recovery from a "bootloader-looping" board**: burn the bootloader over ICSP (erases the bad sketch, restores fuses/lock bits), then upload normally.

## 13. Pitfall table

| Symptom | Cause | Fix |
|---|---|---|
| Upload fails with "not in sync" / stk500_recv | Wrong bootloader era selected, or noise on D0/D1 | Switch `Tools → Processor` between ATmega328P and (Old Bootloader); in PlatformIO switch `upload_speed` 115200 ↔ 57600; disconnect D0/D1 wiring |
| Board does not appear on any COM port | Clone uses CH340 USB chip without driver | Install WCH CH340 driver; genuine boards use FTDI VCP |
| Serial monitor garbage | Baud mismatch, or D0/D1 also used as GPIO | Match baud rate; free D0/D1 while using `Serial` |
| `digitalWrite(A6/A7)` does nothing | A6/A7 are ADC-input-only (no port register) | Use them only with `analogRead()`; move the signal to another pin |
| SPI bus flaky, LED L blinks with clock | D13 is SCK + LED_BUILTIN, LED loads the line | Accept it, or bit-bang SPI on other pins |
| millis()/delay() wrong after timer tinkering | Timer0 prescaler/registers modified | Restore Timer0; never reconfigure it for PWM |
| Servo jitters when tone() plays | Both default libraries compete for timers (Timer1 vs Timer2 is fine, conflicts arise when remapped) | Keep Servo on Timer1, tone on Timer2, avoid Timer0 |
| Random resets / brown-outs with motors or servos | USB 5 V rail overloaded | Power via VIN (7–12 V); common ground; separate supply for actuators |
| 3.3 V sensor resets or misbehaves | 3V3 pin limited to 50 mA | Use an external 3.3 V regulator |
| Board dead after wiring "5 V" to +5V pin from a higher-voltage source | +5V pin bypasses the regulator | Never exceed 5.0 V on pin 27; use VIN for unregulated input |
| Unstable 5 V when VIN < 7 V | LDO dropout | Keep VIN ≥ 7 V (absolute range 6–20 V) |
| ADC readings noisy | Reference/ground quality, digital noise | Use ADC Noise Reduction sleep mode, stable AREF, averaging; `analogReference(EXTERNAL)` with clean reference ≤ 5 V |
| AREF pin damaged | External voltage applied while internal reference selected | Call `analogReference(EXTERNAL)` before applying voltage |
| Sketch corrupts variables / crashes after days | 2 KB SRAM overflow (heap/stack collision), String fragmentation | `F()` macro, static buffers, monitor free RAM |
| EEPROM values wear out | 100 k write-cycle endurance per cell | Use wear-leveling / `EEPROM.update()` |
| Watchdog reset boot-loops the board | Old bootloader does not clear WDT on entry | Burn Optiboot, or always `wdt_disable()` early; recover via ICSP |
| Output damages 3.3 V peripheral | GPIO are 5 V push-pull | Level-shift (divider/MOSFET) |
| Inputs float randomly | Pull-ups off by default (20–50 kΩ available) | `pinMode(pin, INPUT_PULLUP)` or external resistor |
| ICSP programming fails / wrong pins hit | Header oriented 180° in some diagrams | Locate pin-1 mark (outer edge), MISO/5V row toward the inside |
| Clone with different fuse set misbehaves at boot | Non-factory fuses (clock source) | Read fuses with `avrdude`, restore L=0xFF H=0xDA E=0xFD via Burn Bootloader |
| 5 V-only modules get 3.3 V data wrongly read | Nano inputs threshold VIH ≈ 0.6·VCC = 3.0 V | Usually works, but marginal — prefer proper levels |
| `avrdude` refuses to program: "Expected signature for ATMEGA328P is 1E 95 0F" | Counterfeit/relabeled chip (common on cheap clones) reports a different signature, or wiring/power fault makes ISP read garbage | Re-seat ICSP wiring and check 5 V supply first; if the signature genuinely differs, the chip is not really an ATmega328P — `-F` forces the write but the part may not behave as documented |
