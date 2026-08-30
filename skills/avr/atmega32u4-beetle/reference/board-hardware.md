# Beetle (Mini Arduino Leonardo, ATmega32U4) hardware reference

The CJMCU / DFRobot **Beetle**: a 21 × 28 mm ATmega32U4 board with
castellated edge pads, a micro-USB connector, no reset button, no regulator
and no expansion header. Electrically it is an Arduino Leonardo with 16 of
its 26 I/O lines left unbonded.

Part I (§1–§7) is what the board *is*. Part II (§8–§13) is how to work on
it. `recipes.md` holds the code.

Sources, and how far each can be trusted:

- **Arduino AVR core, `variants/leonardo/pins_arduino.h`, `Tone.cpp`,
  `ServoTimers.h`, `boards.txt`** — the pin, timer, ADC-channel and
  interrupt maps, the VID/PIDs, the fuses and the upload protocol. These are
  the files the compiler and uploader actually read, so for this toolchain
  they are not "documentation", they are ground truth.
- **ATmega16U4/32U4 datasheet, Atmel-7766J (04/2016)** — memory map, clock
  tree and PLL, fuse semantics, BOD levels, USB DPRAM.
- **DFRobot's own Beetle documentation (SKU DFR0282, "Powered By DFRobot
  © 2008-2017")** — the pad-to-Arduino-pin table, the supply limits, the
  ICSP-on-the-back reset procedure. This is the vendor's manual for the
  original Beetle, and it is what the pad map in §2.1 is transcribed from.
- **Caterina.c from `arduino/ArduinoCore-avr`** — the bootloader's own source:
  the 8-second timeout, the magic key and the reset-cause logic.
- **The reseller listing the user supplied** — dimensions (21 × 28 mm) and a
  spec table. Note this **disagrees with DFRobot** on size (20 × 22 × 3.8 mm);
  see §1.
- **Not verified on hardware.** See SKILL.md §Reporting.

---

## 1. Overview

| | |
|---|---|
| Marketing names | Beetle · CJMCU Beetle · "Mini Arduino Leonardo" · DFRobot Beetle (DFR0282) |
| MCU | ATmega32U4-AU, 8-bit AVR, 44-lead TQFP/QFN, 26 programmable I/O lines |
| Clock | 16 MHz crystal, no PLL for the core. On-chip PLL 32–96 MHz for USB and Timer4 |
| Flash | 32 KB total · **28,672 B usable** · 4 KB Caterina bootloader at the top |
| SRAM | **2,560 B** (`0x0100`–`0x0AFF`) |
| EEPROM | 1,024 B, 100 k write cycles/cell |
| USB DPRAM | 832 B, **separate silicon** — endpoint buffers do not come out of SRAM |
| USB | Full-speed device, native on-chip. CDC + HID, composite |
| Dimensions | DFRobot original: **20 × 22 × 3.8 mm**. The reseller listing for the board this skill was written against says **21 × 28 mm** — see the note below |
| Supply | **4.5–5 V reliable; 3–4.5 V "may work, reliability not guaranteed"; 6 V destroys the board** (DFRobot's own wording) |
| Reset | **No button.** Short **RESET to GND on the 6-dot ICSP pad array on the back** |
| LED | DFRobot advertise a "magic light blue soft BLINK indicator" — there is an onboard LED |

The chip is the one in the Arduino Leonardo and Micro; the board is a Leonardo
with most of its pins amputated. Anything true of a Leonardo at the silicon
level is true here — the interesting differences are all about which pins
survived and about the absent button and regulator.

**On the two different sizes.** DFRobot document their Beetle as
20 × 22 × 3.8 mm; the reseller page for the "CJMCU Beetle / Mini Arduino
Leonardo" quotes 21 × 28 mm. Both quote the *same* interface counts (10 I/O,
4 PWM, 5 analog, 1 UART, 1 I2C) and the same 5 V, 16 MHz, 32 KB/4 KB
bootloader, 2.5 KB SRAM, 1 KB EEPROM. So the pad *map* below is very likely
shared across the family, while the board *outline* is not — clones are
redrawn, and a larger one may place the ICSP dots or the LED differently.
Trust the pin table; measure the physical layout.

---

## 2. Pin map

### 2.1 The ten exposed pads

> **Transcribed from DFRobot's own "IO Port Mapping in correspondence with
> Arduino Port" table** (Beetle SKU:DFR0282 documentation), cross-checked
> against the Arduino core's leonardo variant file. The silkscreen labels, the
> Arduino pin numbers, the four PWM pads and the five analog channels below
> are the vendor's, not an inference. What remains unverified is only whether
> a *clone* of a different outline (§1) relabels anything — §2.4 has a
> ten-minute check.

Ordered by how you will look for them. "Ard." is the number to pass to
`pinMode()`/`digitalWrite()`.

| Pad | Ard. | Port | ADC | PWM (timer) | Interrupt | Also |
|---|---|---|---|---|---|---|
| `RX` | D0 | PD2 | — | — | **INT2** | `Serial1` RXD1 |
| `TX` | D1 | PD3 | — | — | **INT3** | `Serial1` TXD1 |
| `SDA` | D2 | PD1 | — | — | **INT1** | `Wire` SDA |
| `SCL` | D3 | PD0 | — | **Timer0 OC0B** | **INT0** | `Wire` SCL |
| `9` | D9 | PB5 | **A9** = ADC12 | **Timer1 OC1A** | PCINT5 | — |
| `10` | D10 | PB6 | **A10** = ADC13 | **Timer1 OC1B** | PCINT6 | — |
| `11` | D11 | PB7 | — | **Timer0 OC0A** | PCINT7 | OC1C, `RTS` |
| `A0` | D18 | PF7 | ADC7 | — | — | JTAG **TDI** |
| `A1` | D19 | PF6 | ADC6 | — | — | JTAG **TDO** |
| `A2` | D20 | PF5 | ADC5 | — | — | JTAG **TMS** |
| `+` | — | — | — | — | — | 5 V rail (VBUS) |
| `-` | — | — | — | — | — | GND |

Ten I/O, four PWM, five ADC channels, one UART, one I2C, four external
interrupts. DFRobot's table lists analog channel A9 against the "9" pad and
A10 against the "10" pad — the vendor counts the two dual-role PWM pads toward
the advertised "5 analog inputs", exactly as the silicon does. Note also that
**the "11" pad has PWM but no analog channel** (PB7 has no ADC), which is why
the five channels are A0/A1/A2/A9/A10.

Three things in that table are easy to miss:

- **`SCL` (D3) is both the I2C clock and a PWM output.** Using `Wire` costs
  you one of four PWM pads.
- **D9 and D10 are dual-role analog pads.** They are the only ADC inputs
  that are also PWM outputs on this board.
- **A0–A2 are the JTAG TAP.** Harmless with Arduino's fuses (§6.3), fatal
  with the factory ones.

### 2.2 The sixteen lines that are not brought out

These compile, toggle real port bits, and reach nothing. Library examples
written for a Leonardo, Micro or Pro Micro use them freely.

| Ard. | Port | What it would have been | On the Beetle |
|---|---|---|---|
| D4 | PD4 | A6 / ADC8 | not bonded |
| D5 | PC6 | PWM Timer3 OC3A | not bonded — this is why `tone()` steals no PWM pad |
| D6 | PD7 | A7 / ADC10, PWM Timer4 OC4D | not bonded |
| D7 | PE6 | INT6 | not bonded |
| D8 | PB4 | A8 / ADC11, PCINT4 | not bonded |
| D12 | PD6 | A11 / ADC9 | not bonded |
| **D13** | **PC7** | PWM Timer4 OC4A | **onboard LED only** — no pad |
| A3 | PF4 | ADC4, JTAG TCK | not bonded |
| A4 | PF1 | ADC1 | not bonded |
| A5 | PF0 | ADC0 | not bonded |
| D14 | PB3 | SPI MISO | **test pad** (§3.3) |
| D15 | PB1 | SPI SCK | **test pad** |
| D16 | PB2 | SPI MOSI | **test pad** |
| D17 | PB0 | SPI SS + RX LED | **test pad** |
| D30 | PD5 | TX LED, XCK1 | LED only, if fitted |
| — | PE2 | HWB | tied off; `HWBE` is unprogrammed anyway |

### 2.4 Confirming the pad map on a clone

The map in §2.1 is DFRobot's own. If your board is a differently-shaped clone
(§1) the labels are almost certainly identical — every Beetle clone advertises
the same 10/4/5/1/1 interface counts — but ten minutes settles it. Flash the
`--full` template, open the serial monitor, and walk the pads:

```c
/* walk every pad this skill claims exists, 1 s each */
const uint8_t pads[] = { 0, 1, 2, 3, 9, 10, 11, A0, A1, A2 };
for (uint8_t i = 0; i < sizeof(pads); i++) {
    pinMode(pads[i], OUTPUT);
    digitalWrite(pads[i], HIGH);
    if (Serial) { Serial.print(F("driving pad ")); Serial.println(i); }
    delay(1000);
    digitalWrite(pads[i], LOW);
    pinMode(pads[i], INPUT);
}
```

If a pad on your silkscreen never responds, or your board has a pad this
table does not list, **this reference is wrong for your board.** Correct it
locally and say so.

For the ICSP dots on the back (§3.3), a multimeter in continuity mode against
the QFN pin numbers is faster than any code.

### 2.3 The PWM ↔ timer map

Read out of `digital_pin_to_timer_PGM` in the core's leonardo variant. Only
the four bold rows exist on a Beetle.

| Pin | Timer/channel | Freq | Owned by |
|---|---|---|---|
| **D3** | **Timer0 OC0B** | **~977 Hz** | **`millis()`/`micros()`/`delay()` — never reconfigure** |
| **D11** | **Timer0 OC0A** | **~977 Hz** | **same** |
| **D9** | **Timer1 OC1A** | **~490 Hz** | **`Servo` (32U4 uses Timer1 *only*)** |
| **D10** | **Timer1 OC1B** | **~490 Hz** | **same** |
| D5 | Timer3 OC3A | — | `tone()` — not on a pad |
| D6 | Timer4 OC4D | — | high-speed PLL timer — not on a pad |
| D13 | Timer4 OC4A | — | onboard LED — not on a pad |

The two frequencies differ because `init()` in the core's `wiring.c` puts
Timer0 in **fast PWM** and Timer1 in **phase-correct** mode, both at prescaler
/64: 16 MHz/(64x256) = 976.6 Hz against 16 MHz/(64x510) = 490.2 Hz. Anything
that cares about PWM pitch — motors, audio, LED flicker on camera — behaves
differently on the two halves of the pad set.

The practical shape of this: **`tone()` is free** (Timer3 has no pad),
**`Servo` costs you D9 and D10**, and **Timer0 is untouchable** because the
two PWM pads that survive a Servo are on it.

Timer inventory on the chip, for completeness: Timer0 (8-bit), Timer1
(16-bit), Timer3 (16-bit), Timer4 (10-bit high-speed, clocked from the PLL
at up to 64 MHz). **There is no Timer2** — 328P code that writes `TCCR2A`
will not compile, which is the one failure mode in this area that is loud
rather than silent.

---

## 3. Connectors and pads

### 3.1 USB — micro-B

The only connector. Carries power and is the entire programming and debug
interface. Native USB: there is no CH340/FTDI chip on this board, which is
why the port name changes between running and bootloader states (§9).

### 3.2 Castellated edge pads

2.54 mm pitch half-holes; the board is designed to be reflowed onto a larger
PCB like a module, or to have headers soldered through the half-holes.
Inventory in §2.1.

### 3.3 The ICSP pads on the back — SPI *and* reset

DFRobot's documentation is explicit: **"There are 6 dots on the back of the
module. This is ICSP interface."** That single array is both the SPI bus and
the only reset the board has, which makes it the most important six pads on
the Beetle.

They are a standard 6-pin ICSP footprint, so the numbering is the usual one.
The right-hand column is the package pin (datasheet Figure 1-1) for ringing a
dot out with a multimeter in continuity mode:

| ICSP | Signal | Ard. | Port | QFN/TQFP pin |
|---|---|---|---|---|
| 1 | MISO | D14 | PB3 | 11 |
| 2 | VCC | — | — | — |
| 3 | SCK | D15 | PB1 | 9 |
| 4 | MOSI | D16 | PB2 | 10 |
| 5 | **RESET** | — | RESET | 13 |
| 6 | **GND** | — | — | — |

DFRobot's own recovery instruction — *"Using a cable to touch Pin 5 and Pin 6,
then Beetle will reset"* — is exactly ICSP RESET shorted to ICSP GND. That is
the reset procedure this whole skill refers to (§12.2).

Two consequences:

- **Hardware SPI slave-select is D17/PB0, which is not on this header** — the
  ICSP footprint carries MISO/MOSI/SCK but not SS. Use any of the ten edge
  pads for chip-select instead; the SPI library only needs SS to be an output.
- PB0 is package pin **7**, not 8 — pin 8 is PE6/HWB, which sits between PB0
  and PB1 in the package order.

`SS`/PB0 is also the RX LED, so on boards that fit it the RX LED flickers
with chip-select traffic.

Those five pads are also the **ISP header** (§12.4): MISO/MOSI/SCK/RESET
plus power and ground is exactly what an ArduinoISP or USBasp needs.

### 3.4 LEDs

DFRobot list among the Beetle's key features a **"magic light blue soft BLINK
indicator"** — so an onboard LED exists, it is blue, and "BLINK indicator" is
the vendor describing the pin that the Blink sketch drives, i.e. D13.

| LED | Pin | Polarity | Certain? |
|---|---|---|---|
| "L" (blue) | D13 = PC7 | active **HIGH** | pin and polarity from the core; existence from DFRobot's feature list |
| TX | D30 = PD5 | active **LOW** | pin and polarity certain (`TXLED0`/`TXLED1`); **fitted on a Beetle: not documented** |
| RX | D17 = PB0 | active **LOW** | same (`RXLED0`/`RXLED1`); also SPI `SS` |
| Power | — | — | not documented |

DFRobot's board has no separate TX/RX LEDs in its feature list, and there is
little room for them at 20 × 22 mm — do not count on them.

Practical consequence stands regardless: **"it does not blink" is not evidence
the board is dead.** If `--minimal` uploads successfully, the upload alone has
already proved the chip, the crystal, USB and the bootloader. Wire a real LED
and resistor to D9 to distinguish.

### 3.5 Mechanics

DFRobot original: **20 × 22 × 3.8 mm**; the reseller listing for the larger
clone says 21 × 28 mm (§1). DFRobot describe the edge pads as *"V-shaped
large-size gold-plated IO ports … convenient for the user to twist wires
upon, and can also be directly sewn on clothes with conductive thread"*, plus
*"two honeycomb shape gold-plated power interface"* — that is the `+` and `-`
pair. Neither vendor mentions a reset button, and the documented reset
procedure is the ICSP short (§3.3, §12.2). **If your board has a button, use
it.**

---

## 4. Power tree

No schematic is published, but DFRobot state the envelope in their own
documentation, and the numbers are unusually specific:

> *"This product uses DC power supply with a working voltage of 5V. **6V will
> damage the product by overvoltage.** This product works reliably between
> **4.5V-5V**; it may work under **3V-4.5V**, but the reliability is not
> guaranteed."*

| Range | Behaviour |
|---|---|
| **6 V and above** | **destroys the board** |
| 5 V | nominal |
| 4.5–5 V | reliable |
| 3–4.5 V | "may work", explicitly not guaranteed — and below 4.5 V the chip is out of spec for 16 MHz anyway (datasheet: 8 MHz at 2.7 V, 16 MHz at 4.5 V) |

"6 V damages it" is as good as a statement that **there is no regulator** — a
regulated input would simply drop the extra volt. Treat the `+` pad as wired
straight to `VCC`, `AVCC` and `UVCC`.

The other line worth quoting, because it names a failure most people
misdiagnose as a firmware bug:

> *"For large load applications (such as motor control), you need to connect
> the loaded VCC and GND directly with power supply, and parallel with
> capacitor over 10uF, in order to prevent restart or malfunction caused by
> large load transient."*

A Beetle that reboots when a motor or a relay kicks in is not a watchdog
problem, a USB problem or a code problem: it is the transient on a shared
rail. Power the load from the supply directly, not through the board, and put
**≥ 10 µF** across it.

| Fact | Consequence |
|---|---|
| **No regulator.** Vendor lists 5 V DC as the only input | `+` is a raw rail. 6.0 V is the chip's absolute maximum; anything above ~5.5 V destroys it |
| **No VIN pin** | a battery pack above 5 V needs an external regulator you provide |
| **No 3V3 output** | 3.3 V peripherals need their own supply. There is no 50 mA rail to borrow, unlike a Nano |
| **No supply OR-ing worth relying on** | driving `+` while USB is plugged in connects your supply to the host's VBUS |
| **All I/O are 5 V push-pull, LVTTL inputs** | into a 3.3 V-only part this is damage, not a level problem |
| BOD is set to `BODLEVEL = 011` by the Arduino fuse set | typ. 2.6 V (datasheet Table 8-1: min 2.4 / typ 2.6 / max 2.8 V). The board browns out well below the 4.5 V the chip needs for 16 MHz — a sagging supply misbehaves *before* the BOD saves you |

Running 16 MHz below 4.5 V is out of spec (datasheet: 8 MHz at 2.7 V,
16 MHz at 4.5 V). A Beetle on a drooping USB cable is operating outside the
datasheet, and the symptom is not a clean reset — it is arithmetic and USB
that go wrong intermittently.

---

## 5. Clock and PLL

| Source | Frequency | Feeds |
|---|---|---|
| External crystal | 16 MHz | CPU, Timer0/1/3, UART, ADC prescaler |
| On-chip PLL | 32–96 MHz | USB (48 MHz) and Timer4 (up to 64 MHz) |

The PLL wants a nominal 8 MHz input, so with a 16 MHz crystal the `PINDIV`
bit in `PLLCSR` must be set to halve it; `PLLFRQ` then selects the output.
The Arduino core does all of this in `USBCore.cpp` at startup — you never
configure it by hand unless you are driving Timer4 asynchronously.

Two consequences worth holding on to:

- **USB depends on the crystal.** A cracked or badly-loaded 16 MHz crystal
  gives a board that runs (badly) but never enumerates. "Windows says
  unknown device" on an otherwise-alive board points here.
- `CKDIV8` is unprogrammed in the Arduino fuse set, so the core runs at the
  full 16 MHz. A clone fused for `CKDIV8` runs at 2 MHz: every `delay()` is
  8× long and USB never comes up. Re-burning the bootloader fixes it (§12.4).

---

## 6. Memory map and fuses

### 6.1 Flash

| Range (byte) | Size | Contents |
|---|---|---|
| `0x0000`–`0x6FFF` | 28,672 B | **application** — what `pio run` reports against |
| `0x7000`–`0x7FFF` | 4,096 B | Caterina bootloader (`BOOTSZ = 00` → 2048 words) |

`BOOTRST` is programmed, so reset vectors into the bootloader, which decides
whether to stay (an external reset, or the magic key in RAM) or jump to
the application.

### 6.2 SRAM and EEPROM

| Range | Size | Contents |
|---|---|---|
| `0x0000`–`0x001F` | 32 B | register file |
| `0x0020`–`0x005F` | 64 B | I/O registers |
| `0x0060`–`0x00FF` | 160 B | extended I/O |
| `0x0100`–`0x0AFF` | **2,560 B** | internal SRAM — globals, heap, stack |
| EEPROM `0x000`–`0x3FF` | 1,024 B | separate address space, `EEPROM.h` |
| USB DPRAM | 832 B | **separate** — endpoint buffers, not your SRAM |

### 6.3 Fuses (Arduino Leonardo set, from `boards.txt`)

| Fuse | Value | Decoded |
|---|---|---|
| LOW | `0xFF` | `CKDIV8` off, external crystal, slow rising power start-up |
| HIGH | `0xD8` | OCD off, **JTAG off**, SPI programming on, WDT not forced, EEPROM erased with chip, `BOOTSZ = 00` (4 KB), `BOOTRST` on |
| EXT | `0xCB` | `HWBE` unprogrammed, `BODLEVEL = 011` (typ. 2.6 V) |
| Lock | `0x2F` | bootloader section protected |

The one that bites: **the ATmega32U4 leaves the factory with `JTAGEN`
*programmed*, i.e. JTAG enabled** (datasheet Table 28-4, "Default Value:
0 (programmed, JTAG enabled)"). JTAG's TAP is PF4–PF7 — on this board
A3 (unbonded), **A2, A1, A0**. The Arduino fuse set unprograms it. A clone
that was fused wrong, or a board you re-fused yourself, presents as exactly
three dead analog pads and nothing else wrong. Fix by re-burning the
bootloader (§12.4), or at runtime:

```c
MCUCR |= (1 << JTD);   /* must be written twice within four cycles */
MCUCR |= (1 << JTD);
```

Some tools read the extended fuse back as a different byte than `0xCB`
because its top four bits are unimplemented and read as 1 — that is the tool,
not a mis-fused board.

---

## 7. Vendor material

There is no vendor SDK. The Beetle is programmed with the stock Arduino AVR
core, selecting **Arduino Leonardo** as the board.

| Source | What it gives | What is wrong with it |
|---|---|---|
| **DFRobot Beetle SKU:DFR0282 documentation** | **the pad↔pin table, the supply limits, the ICSP reset procedure, the load-transient warning** | documents DFRobot's own 20 × 22 mm board; a larger clone may differ in outline, though every Beetle advertises identical interface counts. This is the primary board source for this skill |
| **`Caterina.c`** (arduino/ArduinoCore-avr) | the 8 s timeout, magic key `0x7777` at `0x0800`, reset-cause logic | none — it is the bootloader's source |
| Reseller listings | dimensions, pad count, "5 V DC" | marketing copy, no schematic; several list "2.5 KB SRAM" and "32 KB flash" without mentioning the 4 KB bootloader reserve |
| Arduino Leonardo docs | everything at the silicon level | assumes 20 headers' worth of pins that this board does not have (§2.2) |
| ATmega16U4/32U4 datasheet | authoritative for the chip | says nothing about which pins are bonded on a Beetle |

**There is still no published schematic for any Beetle**, DFRobot's included —
their documentation is a wiki page, not a design file. Everything about the
board here comes from that page plus the bootloader source; nothing comes from
tracing copper. Where this reference says "clone-dependent", that is the
reason.

---

# Part II — development guide

## 8. Toolchain and project configuration

PlatformIO 6.1 + `atmelavr` 5.3.0 + `framework-arduino-avr`. There is **no
`beetle` board in PlatformIO** — use `board = leonardo`, which is correct
rather than approximate: the Beetle ships Caterina and the Leonardo VID/PID,
so the board definition's `f_cpu`, `upload.protocol = avr109`,
`use_1200bps_touch`, `maximum_size = 28672` and `maximum_data_size = 2560`
all apply exactly.

Minimum working `platformio.ini` is in `recipes.md` §1. The one thing not to
do is set `upload_port` (§9).

Libraries bundled with the core PlatformIO installs: `EEPROM`, `HID`,
`SoftwareSerial`, `SPI`, `Wire`. **`Keyboard`, `Mouse` and `Servo` are
not** — the Arduino IDE bundles them, PlatformIO needs `lib_deps`:

```ini
lib_deps =
    arduino-libraries/Keyboard
    arduino-libraries/Mouse
    arduino-libraries/Servo
```

`SoftwareSerial` works but is a poor trade here: the board already has a
free hardware UART on the TX/RX pads (`Serial1`), which costs no CPU and
does not break under interrupt load.

## 9. USB, and the two serial ports

The single most confusing thing about this board.

| State | VID:PID | Enumerates as | Speaks |
|---|---|---|---|
| Sketch running | `0x2341:0x8036` | CDC serial port A | whatever your sketch prints |
| Caterina bootloader | `0x2341:0x0036` | CDC serial port **B** (different name) | AVR109 at 57600 |

An upload is a choreography across both. The sketch-side half of it is
readable in the core (`cores/arduino/CDC.cpp`, the `CDC_SET_LINE_CODING` /
`CDC_SET_CONTROL_LINE_STATE` handler), and it is worth knowing exactly
because it explains every failure mode:

1. The IDE opens **port A at 1200 baud and then closes it.** The trigger is
   the *close*, not the open: the handler fires when `dwDTERate == 1200` and
   DTR has just gone low. The running sketch then writes the magic value
   `0x7777` into the top of its own RAM (`RAMEND-1`) and calls
   `wdt_enable(WDTO_120MS)` — **the sketch resets itself via the watchdog**,
   and the bootloader finds the magic key and stays put. (If DTR comes back
   high before the 120 ms expires, the handler cancels the reset and restores
   the watchdog state — which is why an OS that toggles DTR while opening a
   port does not spuriously reboot the board.)
2. Port A **disappears**. This is correct behaviour and alarms everyone once.
3. Port B appears, typically within a second.
4. `avrdude -c avr109 -b 57600 -P <port B>` flashes.
5. The chip resets, port B disappears, port A comes back.

Therefore:

- **Never hard-code `upload_port`.** Pinning it to port A breaks step 4;
  pinning it to port B breaks step 1 (port B does not exist yet).
  `wait_for_upload_port = true` in the board definition exists to let
  PlatformIO discover B for itself.
- `monitor_port` may legitimately need pinning, to port A.
- A serial monitor holding port A open can prevent the 1200 baud open in
  step 1. Close it.
- **The reset is executed by the sketch's own USB interrupt handler.** This
  is the mechanical reason rule 2 in SKILL.md is true: a sketch sitting in
  `noInterrupts()`, wedged in an ISR, or one that never called
  `USBDevice.attach()`, cannot perform its own reset no matter what the host
  does. Nothing external is listening. Only a hardware RESET is left.
- On macOS, prefer `/dev/cu.*` over `/dev/tty.*`.
- Both ports are CDC, so `Serial.begin(<anything>)` sets no baud rate. The
  argument is discarded in the most literal sense available — the core's
  signature is `void Serial_::begin(unsigned long /* baud_count */)`, with
  the parameter name commented out.

`Serial` is USB CDC. `Serial1` is the real UART on D1/D0, where baud rate
means something. They are different objects and `Serial1` keeps working when
USB is unplugged.

`if (Serial)` is true only while a host has the port open — it returns
`_usbLineInfo.lineState > 0`, i.e. the CDC DTR/RTS state. That is the
non-blocking guard; `while (!Serial);` waits forever on a board that is not
plugged into a computer, which on this board is the normal case.

**`operator bool()` contains a `delay(10)`.** Every single `if (Serial)`
costs ten milliseconds, unconditionally, whether it returns true or false.
Testing it once per pass through a fast `loop()` silently caps that loop at
100 Hz. Read it once into a `bool` if you need it more than occasionally —
this is measurable, surprising, and in the core source at `CDC.cpp`.

## 10. Peripheral cookbook

Code in `recipes.md`; this is the map.

| Task | Pads | Notes |
|---|---|---|
| Blink | D13 (LED, no pad) or any of the ten | LED is active HIGH, clone-dependent |
| USB serial | — | `Serial`, guard with `if (Serial)`; `recipes.md` §4 |
| Hardware UART | TX=D1, RX=D0 | `Serial1`, real baud rates; §5 |
| I2C | SDA=D2, SCL=D3 | `Wire`, internal pull-ups are weak — add 4.7 kΩ; §6 |
| SPI | test pads D14/D15/D16, SS=D17 | ring the pads out first; §7 |
| ADC | A0, A1, A2, A9 (=D9), A10 (=D10) | **use the constants**, not bare integers; §8 |
| PWM | D3, D9, D10, D11 | timer ownership decides everything; §10 |
| External interrupt | D0, D1, D2, D3 | always via `digitalPinToInterrupt()`; §9 |
| Pin-change interrupt | D9, D10, D11 | one shared PCINT0 vector; §9 |
| Servo | D9, D10 (or any pin) | takes Timer1, kills D9/D10 `analogWrite()`; §11 |
| `tone()` | any pin | takes Timer3, which has no pad — free here; §11 |
| HID keyboard/mouse | — | `lib_deps` required, guards mandatory; §12 |
| EEPROM | — | `put()`/`get()`, update semantics; §13 |
| Sleep | — | USB complicates it badly; §14 |

## 11. Core-specific gotchas

- **No Timer2.** 328P code that touches `TCCR2A`/`OCR2A` fails to compile.
  Loud, for once.
- **Timer0 owns two of four PWM pads.** Changing its prescaler to get a
  different PWM frequency on D3/D11 breaks `millis()`, `delay()`,
  `Serial1` framing and every bit-banged protocol at the same time.
- **`analogRead()` maps its argument as an analog channel index.** `pin >= 18`
  gets 18 subtracted, then everything goes through
  `analog_pin_to_channel_PGM`. So `analogRead(2)` → A2 → PF5, *not* the SDA
  pad. Use `A0`/`A1`/`A2`/`A9`/`A10`.
- **`digitalPinToInterrupt()` is not optional.** The mapping is scrambled:
  D3→INT0, D2→INT1, D0→INT2, D1→INT3. Passing a raw pin number attaches the
  wrong interrupt or none.
- **The ADC needs `AVCC` settled.** `analogReference(EXTERNAL)` before any
  voltage is applied to AREF, and never drive AREF above VCC — AREF is not
  on a Beetle pad, so in practice: leave the reference alone (`DEFAULT`).
- **USB interrupts run under your code.** A `noInterrupts()` section or an
  ISR longer than a few hundred µs will drop USB frames — the host sees a
  flaky device, not a slow one.
- **Caterina uses the watchdog** to hand off to the sketch. `MCUSR = 0;
  wdt_disable();` as the first two lines of `setup()` (recipes §2), or an
  armed WDT boot-loops the board.
- **`String` on a 2.5 KB part** fragments the heap; the collision with the
  stack surfaces as corruption, not an error.

## 12. Flashing and recovery

### 12.1 Normal upload

```sh
pio run -t upload -t monitor
```

The 1200 bps touch and port hand-off of §9 happen automatically.

### 12.2 Double-tap RESET

The universal escape, and **a single touch is enough** — the double-tap habit
comes from SAMD boards (Zero/M0), not from this bootloader.

The Beetle has no reset button. Short **ICSP pin 5 (RESET) to ICSP pin 6
(GND)** on the six dots on the back (§3.3), once. DFRobot's own words: *"Using
a cable to touch Pin 5 and Pin 6, then Beetle will reset, and Device manager
should appear a COM port named 'Arduino Leonardo bootloader (COM x)'. After a
few seconds, it will disappear."*

Caterina's source (`arduino/ArduinoCore-avr`, `bootloaders/caterina/Caterina.c`)
gives the exact numbers behind that:

| Constant / logic | Value | Meaning |
|---|---|---|
| `TIMEOUT_PERIOD` | **8000** | the bootloader waits **8 seconds**, counted in a 1 ms timer tick |
| `bootKey` | `0x7777` | magic key, at RAM address `0x0800` |
| `mcusr_state & (1<<EXTRF)` | — | **external reset → enter the bootloader**. One RESET short does it |
| `PORF` + valid app | — | plain power-up with an app present → skip the bootloader |
| `WDRF` + key mismatch + valid app | — | watchdog reset that was *not* an upload request → skip |

Two useful consequences of reading that table:

- **The timeout only counts down if an application is present** — the tick does
  `if (pgm_read_word(0) != 0xFFFF) Timeout++`. On a chip with blank flash the
  bootloader waits indefinitely, so a board that has never been programmed
  cannot time out on you.
- There is **no double-tap detection anywhere in Caterina.** A second tap
  merely restarts the same 8 s window — harmless, never required.

DFRobot's practical recipe for a board whose sketch has killed USB: click
Upload first, and *"touch the Pin 5 and Pin 6 immediately. You need try
several times, because the correct time is not easy to be caught."*

This is the fix for: a crashed or tight-looping sketch, a sketch that
disabled USB, a WDT boot-loop, and a HID sketch spamming the host.

### 12.3 Upload failure ladder

Work down it; stop at the first that works.

1. Retry, watching `pio device list` before and after — two different port
   names is the handshake working.
2. Close the serial monitor and anything else holding the sketch's port.
3. Double-tap RESET and start the upload immediately (§12.2 — and note the timing caveat there).
4. `pio run -t upload --upload-port <the bootloader port>` for one shot.
5. ISP (§12.4).

### 12.4 ISP — the route that cannot be locked out

MISO/MOSI/SCK/RESET on the test pads (§3.3), plus `+` and `-`, driven by a
second Arduino running `11.ArduinoISP` or a USBasp. This bypasses USB and
the bootloader entirely, so **no firmware permanently bricks this board.**

`Burn Bootloader` over ISP also rewrites the fuses to LOW `0xFF` / HIGH
`0xD8` / EXT `0xCB` — the fix for a clone with JTAG enabled (§6.3) or
`CKDIV8` set (§5). Uploading over ISP without a bootloader also reclaims the
4 KB Caterina reserve, at the cost of USB uploads.

## 13. Pitfall table

| Symptom | Cause | Fix |
|---|---|---|
| Port vanishes mid-upload | The 1200 bps touch reset the chip into the bootloader | Nothing — this is step 2 of §9. Let PlatformIO wait |
| `avrdude: butterfly_recv(): programmer is not responding` | Talking AVR109 to the sketch's port, not the bootloader's | Unset `upload_port`; or short ICSP 5–6 and target the port that appears |
| Upload times out "waiting for new serial port" | Sketch crashed / blocks interrupts / disabled USB, so the touch never landed | Short ICSP pin 5 to pin 6 (§12.2), then upload inside 8 s |
| Board never appears on any port | Sketch hangs before `USBDevice.attach()`, or the 16 MHz crystal is damaged, or `CKDIV8` is fused | Short RESET to GND on the ICSP dots; if the bootloader port also never appears, ISP (§12.4) |
| Sketch works over USB, does nothing on battery | `while (!Serial);` in `setup()` | `if (Serial)` guard, or a timed wait with an exit |
| Serial output garbled at "the wrong baud rate" | It is USB CDC — there is no baud rate to mismatch | The problem is elsewhere; check you are on `Serial` and not `Serial1` |
| `delay()` and `millis()` drift after adding PWM | Reconfigured Timer0 for D3/D11 PWM frequency | Leave Timer0 alone; move PWM to D9/D10 (Timer1) |
| `analogWrite(9)` / `analogWrite(10)` stopped working | A `Servo` is attached — it owns Timer1 | Use D3/D11, or detach the servo |
| `analogRead(2)` returns the wrong sensor | Argument is an analog *channel index*: it read A2/PF5 | Use `A0`/`A1`/`A2`/`A9`/`A10` constants |
| A0, A1, A2 all dead, everything else fine | `JTAGEN` fused on — those are TDI/TDO/TMS | Burn Bootloader over ISP (§12.4), or set `JTD` twice (§6.3) |
| Host keyboard types garbage on plug-in | HID sketch with no arm pin / grace window | Short RESET to GND, re-flash with the guards (`template/variants/hid/`) |
| `#include <Keyboard.h>` — no such file | Not bundled with the PlatformIO core | `lib_deps = arduino-libraries/Keyboard` |
| `TCCR2A` undeclared | There is no Timer2 on an ATmega32U4 | Port the code to Timer1 or Timer3 |
| Random corruption after minutes/hours | Heap met stack in 2,560 B | `F()` on every literal, no `String`, watch `free_ram()` |
| Board dies the moment external power is applied | `+` is unregulated; DFRobot: "6V will damage the product by overvoltage" | Regulate to 5 V externally. 4.5–5 V is the reliable band |
| **Board resets or misbehaves whenever a motor/relay/LED strip switches** | Load transient on the shared rail — DFRobot call this out explicitly | Feed the load's VCC/GND **from the supply directly**, not through the board, and put **≥ 10 µF** across it (§4) |
| Blink uploads fine but no light | LED (blue, D13) missing or dead on this clone | The successful upload already proved the board runs; drive an LED on D9 to confirm |
