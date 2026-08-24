# ESP32-C3 SoC — reference

A digest of **ESP32-C3 Series Datasheet v2.4** (Espressif), for chip-level
questions that the board reference cannot answer: what a pin can be, what the
boot straps do, how much memory there is, what each peripheral can do, and the
electrical limits.

Board-specific wiring — which of these pins is actually reachable, and what is
soldered to it — is in `board-hardware.md`. Where the two disagree about what a
pin does, the board file wins: it describes copper, this file describes silicon.

## Contents

1. [Series and variants](#1-series-and-variants)
2. [Pins](#2-pins)
3. [Boot configuration](#3-boot-configuration)
4. [System](#4-system)
5. [Memory](#5-memory)
6. [Peripherals](#6-peripherals)
7. [Wireless](#7-wireless)
8. [Electrical characteristics](#8-electrical-characteristics)
9. [Power modes and current](#9-power-modes-and-current)

---

## 1. Series and variants

Nomenclature: `ESP32-C3` `F`(in-package flash) `H|N`(flash temperature grade)
`4`(flash size, MB) `AZ|X`(other identification), plus the chip revision.

| Part number | In-package flash | Ambient | Package | GPIO | Revision |
|---|---|---|---|---|---|
| ESP32-C3 | — | −40…105 °C | QFN32 5×5 | 22 | v0.4 |
| ESP32-C3FN4 (EOL) | 4 MB | −40…85 °C | QFN32 5×5 | 22 | v0.4 |
| **ESP32-C3FH4** | **4 MB** | −40…105 °C | QFN32 5×5 | **22** | v0.4 |
| ESP32-C3FH4AZ (NRND) | 4 MB | −40…105 °C | QFN32 5×5 | 16 | v0.4 |
| ESP32-C3FH4X | 4 MB | −40…105 °C | QFN32 5×5 | 16 | v1.1 |

The **AZ** and **X** variants leave pins 19–24 (the SPI flash pads,
GPIO12–GPIO17) unbonded, which is why they show 16 GPIO instead of 22. The
board this skill covers carries an **FH4** — 22 GPIO, of which six are consumed
by the in-package flash.

Every variant is a single-core RISC-V at up to 160 MHz with 400 KB SRAM.

---

## 2. Pins

### 2.1 QFN32 pin overview (Table 2-1)

| # | Name | Type | Power domain | At reset | After reset |
|---|---|---|---|---|---|
| 1 | LNA_IN | analog | — | — | — |
| 2, 3 | VDD3P3 | power | — | — | — |
| 4 | XTAL_32K_P / GPIO0 | IO | VDD3P3_RTC | — | — |
| 5 | XTAL_32K_N / GPIO1 | IO | VDD3P3_RTC | — | — |
| 6 | GPIO2 | IO | VDD3P3_RTC | IE | IE |
| 7 | CHIP_EN | analog | — | — | — |
| 8 | GPIO3 | IO | VDD3P3_RTC | IE | IE |
| 9 | MTMS / GPIO4 | IO | VDD3P3_RTC | IE | — |
| 10 | MTDI / GPIO5 | IO | VDD3P3_RTC | IE | — |
| 11 | VDD3P3_RTC | power | — | — | — |
| 12 | MTCK / GPIO6 | IO | VDD3P3_CPU | IE (+WPU if `EFUSE_DIS_PAD_JTAG`=0) | — |
| 13 | MTDO / GPIO7 | IO | VDD3P3_CPU | IE | — |
| 14 | GPIO8 | IO | VDD3P3_CPU | IE | IE |
| 15 | GPIO9 | IO | VDD3P3_CPU | IE, WPU | IE, WPU |
| 16 | GPIO10 | IO | VDD3P3_CPU | IE | — |
| 17 | VDD3P3_CPU | power | — | — | — |
| 18 | VDD_SPI / GPIO11 | power/IO | VDD3P3_CPU | — | — |
| 19 | SPIHD / GPIO12 | IO | VDD_SPI | WPU | IE, WPU |
| 20 | SPIWP / GPIO13 | IO | VDD_SPI | WPU | IE, WPU |
| 21 | SPICS0 / GPIO14 | IO | VDD_SPI | WPU | IE, WPU |
| 22 | SPICLK / GPIO15 | IO | VDD_SPI | WPU | IE, WPU |
| 23 | SPID / GPIO16 | IO | VDD_SPI | WPU | IE, WPU |
| 24 | SPIQ / GPIO17 | IO | VDD_SPI | WPU | IE, WPU |
| 25 | GPIO18 | IO | VDD3P3_CPU | — | — |
| 26 | GPIO19 | IO | VDD3P3_CPU | USB_PU | — |
| 27 | U0RXD / GPIO20 | IO | VDD3P3_CPU | IE, WPU | — |
| 28 | U0TXD / GPIO21 | IO | VDD3P3_CPU | OE, WPU | — |
| 29, 30 | XTAL_N, XTAL_P | analog | — | — | — |
| 31, 32 | VDDA | power | — | — | — |
| 33 | GND | power | — | — | — |

IE = input enabled, OE = output enabled, WPU/WPD = internal weak pull-up/down,
USB_PU = USB pull-up.

**Drive strength defaults:** GPIO2, GPIO3, MTMS(4), MTDI(5) → 10 mA.
GPIO18, GPIO19 → 40 mA. Everything else → 20 mA.

`VDD_SPI` is the flash supply by default. It can be reconfigured as GPIO11 only
if the flash is powered externally — irrelevant for in-package-flash parts.

**Do not leave CHIP_EN floating.** High = chip on, low = chip off.

### 2.2 Power-up glitches (Table 2-2)

| Pin | Glitch | Typical duration |
|---|---|---|
| MTCK (GPIO6) | low | 5 ns |
| MTDO (GPIO7) | low | 5 ns |
| GPIO10 | low | 5 ns |
| U0RXD (GPIO20) | low | 5 ns |
| **GPIO18** | **high** | **50 µs** |

Only the GPIO18 one is long enough to reach anything downstream.

### 2.3 IO MUX functions (Table 2-4)

Each pin selects one of up to three IO MUX functions, F0–F2. **Bold** is the
default in the default boot mode.

| Pin | GPIO | F0 | F1 | F2 |
|---|---|---|---|---|
| 4 | GPIO0 | GPIO0 | **GPIO0** | — |
| 5 | GPIO1 | GPIO1 | **GPIO1** | — |
| 6 | GPIO2 | GPIO2 | **GPIO2** | FSPIQ |
| 8 | GPIO3 | GPIO3 | **GPIO3** | — |
| 9 | GPIO4 | **MTMS** | GPIO4 | FSPIHD |
| 10 | GPIO5 | **MTDI** | GPIO5 | FSPIWP |
| 12 | GPIO6 | **MTCK** | GPIO6 | FSPICLK |
| 13 | GPIO7 | **MTDO** | GPIO7 | FSPID |
| 14 | GPIO8 | GPIO8 | **GPIO8** | — |
| 15 | GPIO9 | GPIO9 | **GPIO9** | — |
| 16 | GPIO10 | GPIO10 | **GPIO10** | FSPICS0 |
| 18 | GPIO11 | GPIO11 | GPIO11 | — |
| 19 | GPIO12 | **SPIHD** | GPIO12 | — |
| 20 | GPIO13 | **SPIWP** | GPIO13 | — |
| 21 | GPIO14 | **SPICS0** | GPIO14 | — |
| 22 | GPIO15 | **SPICLK** | GPIO15 | — |
| 23 | GPIO16 | **SPID** | GPIO16 | — |
| 24 | GPIO17 | **SPIQ** | GPIO17 | — |
| 25 | GPIO18 | GPIO18 | **GPIO18** | — |
| 26 | GPIO19 | GPIO19 | **GPIO19** | — |
| 27 | GPIO20 | **U0RXD** | GPIO20 | — |
| 28 | GPIO21 | **U0TXD** | GPIO21 | — |

Signals routed **directly** via IO MUX (Table 2-3) are UART0 (U0TXD/U0RXD),
JTAG (MTCK/MTDO/MTDI/MTMS), SPI0/1 (the flash bus) and SPI2 (the `FSPI*` set).
Everything else reaches a pin through the GPIO Matrix, which is fully flexible
but adds routing latency and caps the usable clock on fast interfaces.

### 2.4 Analog functions (Table 2-6)

| Pin | GPIO | F0 | F1 |
|---|---|---|---|
| 4 | GPIO0 | XTAL_32K_P | ADC1_CH0 |
| 5 | GPIO1 | XTAL_32K_N | ADC1_CH1 |
| 6 | GPIO2 | ADC1_CH2 | — |
| 8 | GPIO3 | ADC1_CH3 | — |
| 9 | GPIO4 | ADC1_CH4 | — |
| 10 | GPIO5 | ADC2_CH0 | — |
| 25 | GPIO18 | USB_D− | — |
| 26 | GPIO19 | USB_D+ | — |

That is the whole analog inventory: **five ADC1 channels, one ADC2 channel**,
the optional 32 kHz crystal pins, and USB. Nothing else on this chip is analog.

USB_D−/USB_D+ can be swapped in software via `USB_SERIAL_JTAG_EXCHG_PINS`.

### 2.5 Pins that need caution as GPIO (§2.3.3)

- **GPIO12–GPIO17** — the in-package flash bus. *"Do not use the pins connected
  to in-package flash for any other purposes."*
- **GPIO2, GPIO8, GPIO9** — strapping pins.
- **GPIO18, GPIO19** — USB Serial/JTAG by default; must be reconfigured to act
  as GPIO, which costs you the console.
- **GPIO4–GPIO7** — the JTAG interface.
- **GPIO20, GPIO21** — UART0.
- **GPIO11** — VDD_SPI.

### 2.6 Peripheral pin assignment priorities (Table 2-7)

- **P1** — fixed IO MUX pin, direct connection. USB Serial/JTAG has *only* P1
  pins (GPIO18/19), so it cannot be moved.
- **P2** — free GPIO via the matrix, no restrictions. On this chip: GPIO0,
  GPIO1, GPIO3, GPIO10.
- **P3** — usable via the matrix but conflicts with something important:
  GPIO2/8/9 (straps), GPIO4–7 (JTAG), GPIO11 (VDD_SPI), GPIO18/19 (USB),
  GPIO20/21 (UART0).
- **P4** — GPIO12–GPIO17, already allocated to flash.

UART1, I2C, I2S, TWAI, LEDC and RMT have **no** P1 pins: they go anywhere from
P2 downwards.

### 2.7 Flash pin mapping (Table 2-12)

| Pin | Name | Single / Dual / Quad SPI |
|---|---|---|
| 22 | SPICLK | CLK |
| 21 | SPICS0 | CS# (in-package flash) |
| 23 | SPID | DI |
| 24 | SPIQ | DO |
| 20 | SPIWP | WP# |
| 19 | SPIHD | HOLD# |

### 2.8 Power pins (Table 2-9) and power-up timing

| Pin | Name | Supplies |
|---|---|---|
| 2, 3 | VDD3P3 | analog domain |
| 11 | VDD3P3_RTC | RTC + part of digital; powers the RTC IO pins (GPIO0–GPIO5) |
| 17 | VDD3P3_CPU | digital domain; powers the digital IO pins |
| 18 | VDD_SPI | flash (output by default) |
| 31, 32 | VDDA | analog domain |
| 33 | GND | — |

Two internal regulators produce 1.1 V: a digital one and a low-power one.

Power-up timing (Table 2-11): rails must be stable for **t_STBL ≥ 50 µs**
before CHIP_EN goes high; a reset needs CHIP_EN below V_IL_nRST for
**t_RST ≥ 50 µs**.

---

## 3. Boot configuration

Strapping pins: **GPIO2, GPIO8, GPIO9**. Defaults with nothing attached:

| Pin | Default | Bit |
|---|---|---|
| GPIO2 | floating | — |
| GPIO8 | floating | — |
| GPIO9 | weak pull-up | 1 |

Latches sample the strap values at Chip Reset and hold them until power-down,
so the pins are ordinary GPIO immediately afterwards. Setup time t_SU ≥ 0 ms,
**hold time t_H ≥ 3 ms after CHIP_EN goes high** — a button that bounces for
longer than that can land you in the wrong boot mode.

### 3.1 Boot mode (Table 3-3)

| Boot mode | GPIO2 | GPIO8 | GPIO9 |
|---|---|---|---|
| **SPI boot** | 1 | any | 1 |
| Joint download boot | 1 | 1 | 0 |

GPIO2 does not itself select the mode, but Espressif recommend pulling it high
"due to glitches" — in practice a low GPIO2 at reset stops the board booting.

Joint download boot accepts **both** USB-Serial-JTAG download and UART
download, and can load into SRAM as well as flash.

### 3.2 ROM message printing

Default: ROM messages go to **both** UART0 and the USB Serial/JTAG controller.

`EFUSE_UART_PRINT_CONTROL` + GPIO8 gate the UART0 copy (Table 3-4): value 0 →
always on; 1 → on when GPIO8 = 0; 2 → on when GPIO8 = 1; 3 → always off.

`EFUSE_USB_PRINT_CHANNEL` gates the USB copy, and `EFUSE_DIS_USB_SERIAL_JTAG`
disables the controller outright (Table 3-5).

All these eFuses default to 0 and are **one-time programmable**.

---

## 4. System

### 4.1 CPU

- 32-bit RISC-V, single core, four-stage pipeline, up to **160 MHz**
- **RV32IMC** — integer, multiply/divide, compressed. **No FPU**: keep floating
  point out of hot loops
- 32 vectored interrupts at seven priority levels
- up to 8 hardware breakpoints/watchpoints, up to 16 PMP regions
- JTAG debug

A GDMA controller provides three TX and three RX channels shared by SPI2,
UHCI0, I2S, AES, SHA and ADC.

### 4.2 Clocks

**CPU clock** sources: the external main crystal, the fast RC oscillator
(~17.5 MHz, adjustable), or the PLL. After reset the CPU runs on the **main
crystal divided by 2**. *The ESP32-C3 cannot operate without an external main
crystal.*

**RTC slow clock**: external 32 kHz crystal, internal slow RC (~136 kHz), or
the fast RC divided by 256.
**RTC fast clock**: main crystal ÷ 2, or the fast RC divided by N.

### 4.3 Reset levels

CPU Reset (core only) · Core Reset (everything digital except RTC) · System
Reset (including RTC) · Chip Reset (everything). Only Chip Reset loses the
contents of internal memory.

### 4.4 Timers and watchdogs

- **System timer**: 52-bit, two counters, three comparators, fixed 16 MHz
- **Timer groups**: two 54-bit up/down timers with 16-bit prescalers
- **Watchdogs**: one MWDT per timer group + one RWDT in RTC, four stages each.
  RWDT and TIMG0's MWDT are enabled automatically during flash boot
- **SWD**: an analog super-watchdog in the RTC domain
- **XTWDT**: monitors the external 32 kHz crystal and switches the RTC slow
  clock source if it stops oscillating

### 4.5 Other system blocks

Interrupt Matrix (62 sources → 31 CPU interrupts) · Permission Control (PMS,
privileged/unprivileged split over memory, peripherals and DMA) · World
Controller (secure/non-secure worlds) · Debug Assistant (read/write monitors,
SP bounds, PC logging, bus-access logging).

### 4.6 Cryptography

AES-128/256 (ECB, CBC, OFB, CTR, CFB8, CFB128) · SHA-1/224/256 ·
HMAC-SHA-256 · RSA up to 3072-bit · Digital Signature · XTS-AES flash
encryption · true RNG seeded from ADC thermal noise and clock mismatch ·
clock-glitch detection.

---

## 5. Memory

| Region | Size | Notes |
|---|---|---|
| ROM | 384 KB | boot and core functions |
| SRAM | **400 KB** | data and instructions, up to 160 MHz; **16 KB of it is cache** |
| RTC FAST | 8 KB | CPU-accessible, retained through deep sleep |
| eFuse | 4 Kbit | 1792 bits available for user data |
| In-package flash | 4 MB (FH4/FN4) | on SPI0/1 |

**Cache**: 16 KB, eight-way set-associative, 32-byte blocks, **read-only**,
with pre-load and lock. Because it is read-only there is no cache-vs-DMA
write-back hazard of the kind an STM32 with a D-cache has.

**External flash** (not present on this board): up to 16 MB via SPI/Dual/Quad/
QPI, with up to 8 MB of instruction space and 8 MB of data space mapped in
64 KB blocks. XTS-AES encryption is supported.

---

## 6. Peripherals

### UART

Two controllers (UART0, UART1). Up to **5 Mbps**, RS232/RS485/IrDA, hardware
(CTS/RTS) and software (XON/XOFF) flow control, both reachable by GDMA through
UHCI0.

### SPI

- **SPI0** — GDMA and cache access to flash
- **SPI1** — CPU access to flash
- **SPI2 (FSPI)** — the general-purpose one, with a GDMA channel

SPI0/1: Single/Dual/Quad/QPI, up to **120 MHz** STR.
SPI2 as master: 2-line full duplex or 1/2/4-line half duplex up to **80 MHz**,
six CS lines, configurable CPOL/CPHA and bit order. As slave: 60 MHz.

The 80 MHz master ceiling applies to IO-MUX-routed pins; a signal that goes
through the GPIO Matrix does not reach it.

### I2C

**One controller.** Standard 100 kbit/s, Fast 400 kbit/s, and up to
800 kbit/s "constrained by SCL and SDA pull-up strength". 7- and 10-bit
addressing, double addressing, 7-bit broadcast.

One controller is the constraint that shapes this board: the panel is on it.

### I2S

One controller, master or slave, full or half duplex, 8/16/24/32-bit, BCK from
10 kHz to 40 MHz, GDMA-connected. TDM PCM / TDM MSB / TDM standard / PDM.

### USB Serial/JTAG

- CDC-ACM virtual serial port **and** JTAG adapter in one controller
- USB 2.0 **full speed**, 12 Mbit/s — not high speed
- can program the flash and debug the CPU
- full-speed PHY on chip; fixed to GPIO18/GPIO19

This is what makes the board flashable and debuggable over one USB-C cable with
no bridge chip, and why bad firmware cannot brick it.

### TWAI (CAN)

ISO 11898-1 / CAN 2.0, 11-bit and 29-bit IDs, 1 kbit/s – 1 Mbit/s, Normal /
Listen Only / Self-Test, 64-byte RX FIFO, single and dual acceptance filters.

### LEDC (LED PWM)

**Six** independent channels, up to **14-bit** duty resolution, APB or main
crystal clock source, runs in Light-sleep, hardware duty fade.

### RMT

Two TX and two RX channels sharing a 192 × 32-bit RAM block. The standard way
to drive WS2812-family LEDs.

### SAR ADC

Two 12-bit SAR ADCs. **ADC1: five channels, factory-calibrated. ADC2: one
channel, not factory-calibrated** — and *"ADC2 of some chip revisions is not
operable"* per the SoC errata.

Sampling rate up to 100 kSPS. DNL ±7 LSB, INL ±12 LSB with a 100 nF cap, DC
input, 25 °C, Wi-Fi off.

Calibrated total error (Table 5-6):

| Attenuation | Effective range | Total error |
|---|---|---|
| ATTEN0 | 0–750 mV | ±10 mV |
| ATTEN1 | 0–1050 mV | ±10 mV |
| ATTEN2 | 0–1300 mV | ±10 mV |
| ATTEN3 | 0–2500 mV | ±35 mV |

### Temperature sensor

−40 °C to 125 °C, measures die temperature. Reads high relative to ambient, and
the offset moves with CPU clock and IO load — it is a die-health signal, not a
thermometer.

---

## 7. Wireless

**Wi-Fi**: 802.11 b/g/n, MCS0-7 at 20 and 40 MHz, MCS32, 0.4 µs guard interval,
up to 150 Mbps, RX STBC, adjustable TX power, antenna diversity with an
external RF switch. Full MAC: 4 virtual interfaces, STA / SoftAP / STA+SoftAP /
promiscuous, A-MPDU and A-MSDU, WMM, WPA2/WPA3 PSK and Enterprise, 802.11mc
FTM. Channels 2412–2484 MHz.

TX power: 21 dBm at 802.11b 1 Mbps, 19 dBm at 802.11g 54 Mbps, 18.5 dBm at
802.11n MCS7.

**Bluetooth LE 5**: 1 Mbps and 2 Mbps PHY, Coded PHY at 125/500 kbps, hardware
LBT. Advertising extensions and multiple advertising sets, simultaneous
advertising and scanning, multiple simultaneous central and peripheral
connections, AFH, LE privacy 1.2, data length extension, link-layer encryption.
Bluetooth mesh supported.

No 802.15.4 on the C3 (that is the C6).

---

## 8. Electrical characteristics

### Absolute maximum (Table 5-1)

| Parameter | Min | Max |
|---|---|---|
| Voltage on any power pin | −0.3 V | **3.6 V** |
| Cumulative IO output current | — | 1000 mA |
| Storage temperature | −40 °C | 150 °C |

### Recommended operating (Table 5-2)

| Parameter | Min | Typ | Max |
|---|---|---|---|
| VDDA, VDD3P3, VDD3P3_RTC | 3.0 V | 3.3 V | 3.6 V |
| VDD3P3_CPU | 3.0 V | 3.3 V | 3.6 V |
| Cumulative input current | 0.5 A | — | — |

When burning eFuses, keep VDD3P3_CPU at or below 3.3 V — the burn circuitry is
sensitive to more.

### DC characteristics at 3.3 V, 25 °C (Table 5-4)

| Parameter | Min | Typ | Max |
|---|---|---|---|
| V_IH | 0.75 × VDD | — | VDD + 0.3 |
| V_IL | −0.3 | — | 0.25 × VDD |
| V_OH | 0.8 × VDD | — | — |
| V_OL | — | — | 0.1 × VDD |
| I_OH (PAD_DRIVER = 3) | — | 40 mA | — |
| I_OL (PAD_DRIVER = 3) | — | 28 mA | — |
| Internal pull-up / pull-down | — | **45 kΩ** | — |
| Pin capacitance | — | 2 pF | — |

45 kΩ internal pull-ups are far too weak for a 400 kHz I2C bus; that is why the
board fits external ones.

### Flash (Table 5-10)

80 MHz max clock · 100,000 program/erase cycles · 20-year retention · page
program 0.8 ms typ / 5 ms max · 4 KB sector erase 70 ms typ / 500 ms max ·
32 Mb chip erase 20 s typ / 60 s max.

---

## 9. Power modes and current

| Mode | What is on |
|---|---|
| **Active** | CPU, RF, all peripherals |
| **Modem-sleep** | CPU on (clock may be reduced), RF periodically woken to keep links alive |
| **Light-sleep** | CPU stalled, optionally powered; wake on MAC, RTC timer or GPIO |
| **Deep-sleep** | RTC only; connection state kept in RTC memory |

Wi-Fi peaks (Table 5-7): 335 mA TX at 802.11b 1 Mbps @21 dBm, 285 mA at
802.11g 54 Mbps, 276–278 mA at 802.11n MCS7, 84–87 mA RX.

Modem-sleep (Table 5-8):

| CPU | State | Peripheral clocks off | on |
|---|---|---|---|
| 160 MHz | running | 23 mA | 28 mA |
| 160 MHz | idle | 16 mA | 21 mA |
| 80 MHz | running | 17 mA | 22 mA |
| 80 MHz | idle | 13 mA | 18 mA |

Low-power modes (Table 5-9): **Light-sleep 130 µA** (VDD_SPI and Wi-Fi down,
all GPIO high-Z) · **Deep-sleep 5 µA** (RTC timer + RTC memory) · **Power off
1 µA** (CHIP_EN low).

These are chip figures at the pin. A board's regulator quiescent current,
indicator LEDs and attached modules add on top — see `board-hardware.md` §7.
