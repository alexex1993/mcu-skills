# ESP32-C6 SoC — Reference

A digest of the silicon datasheet, kept here so questions about the chip itself —
pin functions, IO MUX, boot straps, memory, peripherals, electrical limits — can be
answered without leaving the skill. For what the *board* does with those pins, read
[board-hardware.md](board-hardware.md) instead; this file is chip-level only.

> Source: Espressif Systems, *ESP32-C6 Series Datasheet* **v1.5** (2026-03-31), 86 pp.
> Canonical copy: <https://www.espressif.com/documentation/esp32-c6_datasheet_en.pdf>
>
> Quoted values are reproduced verbatim; where the datasheet gives Min/Typ/Max, all
> three are kept. The board carries the **QFN32** variant (ESP32-C6FH4) — where the
> datasheet gives separate QFN40 and QFN32 tables, the QFN32 one applies, but both
> are reproduced because the QFN40 tables are the ones people find first and
> misapply.

---

## Table of contents

1. [Product overview](#1-product-overview)
2. [Series comparison and variants](#2-series-comparison-and-variants)
3. [Feature summary](#3-feature-summary)
4. [Pins](#4-pins)
   - [4.1 Pin layout](#41-pin-layout)
   - [4.2 Pin overview tables](#42-pin-overview-tables)
   - [4.3 IO MUX functions](#43-io-mux-functions)
   - [4.4 LP IO MUX functions](#44-lp-io-mux-functions)
   - [4.5 Analog functions](#45-analog-functions)
   - [4.6 GPIO restrictions](#46-gpio-restrictions)
   - [4.7 Peripheral pin assignment priorities](#47-peripheral-pin-assignment-priorities)
   - [4.8 Analog and power pins](#48-analog-and-power-pins)
   - [4.9 Power scheme, power-up and reset](#49-power-scheme-power-up-and-reset)
   - [4.10 Chip-to-flash pin mapping](#410-chip-to-flash-pin-mapping)
5. [Boot configuration and strapping pins](#5-boot-configuration-and-strapping-pins)
6. [System](#6-system)
7. [Cryptography and security](#7-cryptography-and-security)
8. [Peripherals](#8-peripherals)
9. [Wireless communication](#9-wireless-communication)
10. [Electrical characteristics](#10-electrical-characteristics)
11. [RF characteristics](#11-rf-characteristics)
12. [Packaging and reliability](#12-packaging-and-reliability)
13. [Consolidated pin overview](#13-consolidated-pin-overview)
14. [Related documentation](#14-related-documentation)
15. [Datasheet revision history](#15-datasheet-revision-history)

---

## 1. Product overview

The ESP32-C6 is a System on Chip that supports **Wi-Fi 6 in the 2.4 GHz band,
Bluetooth 5 (LE), Zigbee 3.0 and Thread 1.3**. It consists of:

- a **high-performance (HP) 32-bit RISC-V processor**,
- a **low-power (LP) 32-bit RISC-V processor**,
- wireless baseband and MAC for Wi-Fi, Bluetooth LE and 802.15.4,
- an RF module,
- and a large peripheral set.

Wi-Fi, Bluetooth and 802.15.4 **coexist and share the same antenna**; an internal
coexistence mechanism arbitrates between them.

### Functional block diagram (textual)

```
┌──────────────────────── HP Core System ────────────────────────┐
│  HP RISC-V 32-bit Microprocessor   RISC-V Trace Encoder        │
│  Cache        HP SRAM        ROM        JTAG                   │
│  Debug Assistant       Main System Watchdog Timer              │
├──────────────── HP Peripherals ────────────────────────────────┤
│  UART   SPI   I2C   I2S   PARLIO   PCNT   TWAI®                │
│  MCPWM  LED PWM  RMT   SAR ADC   Temperature Sensor            │
│  USB Serial/JTAG        SDIO Slave                             │
├──────────────── HP System Components ──────────────────────────┤
│  Fast RC Oscillator   External Main Crystal   PLL Clock Gen    │
│  Permission Control   System Timer   Event Task Matrix   GDMA  │
│  General-purpose Timers                                        │
├──────────────── Security ──────────────────────────────────────┤
│  AES  SHA  HMAC  RSA  ECC  RNG  RSA Digital Signature          │
│  Secure Boot        Flash Encryption                           │
├──────────────── LP Core System and Peripherals ────────────────┤
│  LP RISC-V 32-bit Microprocessor   LP SRAM                     │
│  eFuse Controller     LP UART     LP I2C                       │
├──────────────── LP System Components ──────────────────────────┤
│  RTC Timer   RTC Watchdog Timer   Brownout Detector            │
│  Super Watchdog Timer   Power Management Unit                  │
├──────────────── Wireless MAC and Baseband ─────────────────────┤
│  Wi-Fi MAC / Wi-Fi Baseband                                    │
│  Bluetooth LE Link Controller / Bluetooth LE Baseband          │
│  802.15.4 MAC / 802.15.4 Baseband                              │
├──────────────── RF ────────────────────────────────────────────┤
│  2.4 GHz Receiver  2.4 GHz Transmitter                         │
│  RF Synthesizer    2.4 GHz Balun + Switch                      │
└────────────────────────────────────────────────────────────────┘
```

The datasheet's block diagram is annotated with the **lowest power mode in which
each block is powered on by default** (hardware preset): Active, Modem-sleep,
Light-sleep, Deep-sleep. See [§6.6 Power Management Unit](#66-power-management-unit).

### Target applications

Smart Home · Industrial Automation · Health Care · Consumer Electronics ·
Smart Agriculture · POS Machines · Service Robot · Audio Devices ·
Generic low-power IoT sensor hubs · Generic low-power IoT data loggers.

---

## 2. Series comparison and variants

### 2.1 Nomenclature

```
ESP32-C6  F  H  4
   │      │  │  └── Flash size in MB
   │      │  └───── Flash temperature grade:  H = high temp,  N = normal temp
   │      └──────── In-package flash present
   └─────────────── Chip series
```

### 2.2 Variant comparison (Table 1-1)

| Part number | In-package flash | Ambient temp. | Package | Chip revision |
| --- | --- | --- | --- | --- |
| **ESP32-C6** | — (external flash) | –40 … 105 °C | QFN40 (5×5 mm) | v0.0 / v0.1 / v0.2 |
| **ESP32-C6FH4** | 4 MB (Quad SPI) | –40 … 105 °C | QFN32 (5×5 mm) | v0.0 / v0.1 / v0.2 |
| **ESP32-C6FH8** | 8 MB (Quad SPI) | –40 … 105 °C | QFN32 (5×5 mm) | v0.0 / v0.1 / v0.2 |

Notes reproduced from the datasheet:

- *Ambient temperature* is the recommended temperature of the environment
  **immediately outside** the chip.
- By default the in-package SPI flash runs at a **maximum clock of 80 MHz** and
  **does not support the flash auto-suspend feature**. A 120 MHz flash clock or
  auto-suspend requires contacting Espressif.
- The plain ESP32-C6 (QFN40) can connect an external flash.

> **This project's board carries the ESP32-C6FH4** — QFN32, 4 MB in-package flash,
> 22 GPIOs. See [board-hardware.md](board-hardware.md).

### 2.3 Chip revisions

Multiple chip revisions ship under the same part number. Revision identification,
the minimum ESP-IDF release supporting each revision, and the list of errata fixed
per revision are in the separate **ESP32-C6 Series SoC Errata** document.

---

## 3. Feature summary

### 3.1 Wi-Fi

- 1T1R in the 2.4 GHz band; operating frequency **2412 – 2484 MHz**
- Data rate up to **150 Mbps**
- **IEEE 802.11ax compliant**
  - 20 MHz-only non-AP mode
  - MCS0 – MCS9
  - Uplink and downlink **OFDMA** (well suited to dense multi-client environments)
  - Downlink **MU-MIMO**
  - **Beamformee** (improves signal quality)
  - Channel Quality Indication (CQI)
  - **DCM** (dual carrier modulation) for link robustness
  - **Spatial reuse** to maximise parallel transmissions
  - **TWT** (target wake time) for power saving
- Fully compatible with **802.11 b/g/n**
  - 20 MHz and 40 MHz bandwidth, MCS0 – MCS7
  - WMM, TX/RX A-MPDU, TX/RX A-MSDU, Immediate Block ACK
  - Fragmentation / defragmentation, TXOP
  - Automatic beacon monitoring (hardware TSF)
  - **Four virtual Wi-Fi interfaces**
  - Infrastructure BSS in Station, SoftAP, Station+SoftAP, and promiscuous modes
    — **note:** when scanning in Station mode, the SoftAP channel follows the
    Station channel
  - Antenna diversity
  - 802.11mc FTM *(not supported on some chip revisions — see the Errata)*

### 3.2 Bluetooth

- **Bluetooth LE: Bluetooth 5.3 certified**
- Bluetooth mesh
- High-power mode, up to **20 dBm** transmit power
- PHY rates: **125 Kbps, 500 Kbps, 1 Mbps, 2 Mbps**
- Advertising extensions, multiple advertisement sets
- Channel selection algorithm #2
- LE power control
- Internal Wi-Fi/Bluetooth coexistence sharing a single antenna

### 3.3 IEEE 802.15.4

- Compliant with **IEEE 802.15.4-2015**
- O-QPSK PHY in the 2.4 GHz band, **250 Kbps**
- **Thread 1.3**, **Zigbee 3.0**

### 3.4 CPU and memory

| Item | Value |
| --- | --- |
| HP RISC-V clock | up to **160 MHz**, four-stage pipeline |
| HP CoreMark® | **496.66 CoreMark**, **3.10 CoreMark/MHz** @160 MHz |
| LP RISC-V clock | up to **20 MHz**, two-stage pipeline |
| GDMA | 3 transmit + 3 receive channels |
| L1 cache | 32 KB |
| ROM | 320 KB |
| HP SRAM | 512 KB |
| LP SRAM | 16 KB |
| eFuse | 4096 bits total, **up to 1792 bits for users** |
| Flash interfaces | SPI, Dual SPI, Quad SPI, QPI |
| Flash controller | with cache; in-circuit programming (ICP) supported |

### 3.5 Peripheral summary

- **30 GPIOs (QFN40)** or **22 GPIOs (QFN32)**
  - 5 strapping GPIOs
  - 6 GPIOs consumed by off-package flash
- Connectivity: 2 × UART, 1 × LP UART, 2 × SPI for flash, 1 × general-purpose SPI,
  I2C, LP I2C, I2S, pulse counter, USB Serial/JTAG, **2 × TWAI®** (ISO 11898-1 /
  CAN 2.0), SDIO slave, LED PWM (up to 6 channels), MCPWM, RMT (TX/RX), PARLIO,
  Event Task Matrix
- Analog: **12-bit SAR ADC, up to 7 channels**; internal temperature sensor
- Timers: 52-bit system timer, two 54-bit general-purpose timers, three digital
  watchdogs, one analog watchdog

### 3.6 Power management

- Fine-resolution control: clock frequency, duty cycle, Wi-Fi operating mode, and
  individual internal components
- Four predefined modes: **Active, Modem-sleep, Light-sleep, Deep-sleep**
- **Deep-sleep current: 7 µA**
- LP memory stays powered in Deep-sleep

### 3.7 Security

- **Secure Boot** — permission control on internal and external memory access
- **Flash encryption** — memory encryption/decryption
- **TEE** (trusted execution environment) controller and access permission
  management (APM)
- Cryptographic acceleration: AES-128/256 (FIPS PUB 197), ECC, HMAC, RSA,
  SHA (FIPS PUB 180-4), RSA Digital Signature peripheral
- External memory encryption/decryption (XTS-AES)
- True random number generator (RNG)

### 3.8 RF module

- Antenna switches, RF balun, power amplifier, low-noise receive amplifier
- Up to **+21 dBm** for 802.11b transmission
- Up to **+19.5 dBm** for 802.11ax transmission
- Up to **–106 dBm** Bluetooth LE receive sensitivity (125 Kbps)

---

## 4. Pins

### 4.1 Pin layout

Pins are numbered **anti-clockwise starting from pin 1 in the top view**. Both
packages have a central ground pad (pin 41 on QFN40, pin 33 on QFN32).

**QFN40 (5×5 mm), top view** — pins 1–40 plus GND pad 41:

| # | Name | # | Name | # | Name | # | Name |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | ANT | 11 | MTDI | 21 | SPIQ | 31 | SDIO_CMD |
| 2 | VDDA3P3 | 12 | MTCK | 22 | SPIWP | 32 | SDIO_CLK |
| 3 | VDDA3P3 | 13 | MTDO | 23 | VDD_SPI | 33 | SDIO_DATA0 |
| 4 | CHIP_PU | 14 | GPIO8 | 24 | SPIHD | 34 | SDIO_DATA1 |
| 5 | VDDPST1 | 15 | GPIO9 | 25 | SPICLK | 35 | SDIO_DATA2 |
| 6 | XTAL_32K_P | 16 | GPIO10 | 26 | SPID | 36 | SDIO_DATA3 |
| 7 | XTAL_32K_N | 17 | GPIO11 | 27 | GPIO15 | 37 | VDDA1 |
| 8 | GPIO2 | 18 | GPIO12 | 28 | VDDPST2 | 38 | XTAL_N |
| 9 | GPIO3 | 19 | GPIO13 | 29 | U0TXD | 39 | XTAL_P |
| 10 | MTMS | 20 | SPICS0 | 30 | U0RXD | 40 | VDDA2 |
| | | | | | | 41 | GND (pad) |

**QFN32 (5×5 mm), top view** — pins 1–32 plus GND pad 33:

| # | Name | # | Name | # | Name | # | Name |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | ANT | 9 | GPIO3 | 17 | GPIO13 | 25 | SDIO_DATA0 |
| 2 | VDDA3P3 | 10 | MTMS | 18 | GPIO14 | 26 | SDIO_DATA1 |
| 3 | VDDA3P3 | 11 | MTDI | 19 | GPIO15 | 27 | SDIO_DATA2 |
| 4 | CHIP_PU | 12 | MTCK | 20 | VDDPST2 | 28 | SDIO_DATA3 |
| 5 | VDDPST1 | 13 | MTDO | 21 | U0TXD | 29 | VDDA1 |
| 6 | XTAL_32K_P | 14 | GPIO8 | 22 | U0RXD | 30 | XTAL_N |
| 7 | XTAL_32K_N | 15 | GPIO9 | 23 | SDIO_CMD | 31 | XTAL_P |
| 8 | GPIO2 | 16 | GPIO12 | 24 | SDIO_CLK | 32 | VDDA2 |
| | | | | | | 33 | GND (pad) |

**QFN32 differences vs QFN40:** the six flash pins (SPICS0/SPIQ/SPIWP/SPIHD/
SPICLK/SPID → GPIO24–GPIO26, GPIO28–GPIO30), the `VDD_SPI` pin (GPIO27), and
GPIO10/GPIO11 are **not routed out**, because the flash lives inside the package.
**GPIO14 exists only on QFN32.**

### 4.2 Pin overview tables

The chip has three kinds of pin:

1. **IO pins** with predefined function sets — IO MUX functions (every IO pin),
   LP IO MUX functions (some), analog functions (some).
2. **Analog pins** with exclusively dedicated analog functions.
3. **Power pins**.

Pin-setting abbreviations used below:

| Abbrev. | Meaning |
| --- | --- |
| `IE` | Input enabled |
| `WPU` | Internal weak pull-up resistor enabled |
| `WPD` | Internal weak pull-down resistor enabled |
| `USB_PU` | USB pull-up resistor enabled |

#### Table 2-1 — QFN40 pin overview

| Pin | Name | Type | Power from | At reset | After reset | IO MUX | LP IO MUX | Analog |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | ANT | Analog | | | | | | |
| 2 | VDDA3P3 | Power | | | | | | |
| 3 | VDDA3P3 | Power | | | | | | |
| 4 | CHIP_PU | Analog | VDDPST1 | | | | | |
| 5 | VDDPST1 | Power | | | | | | |
| 6 | XTAL_32K_P | IO | VDDPST1 | | | ✓ | ✓ | ✓ |
| 7 | XTAL_32K_N | IO | VDDPST1 | | | ✓ | ✓ | ✓ |
| 8 | GPIO2 | IO | VDDPST1 | IE | IE | ✓ | ✓ | ✓ |
| 9 | GPIO3 | IO | VDDPST1 | IE | IE | ✓ | ✓ | ✓ |
| 10 | MTMS | IO | VDDPST1 | IE | IE | ✓ | ✓ | ✓ |
| 11 | MTDI | IO | VDDPST1 | IE | IE | ✓ | ✓ | ✓ |
| 12 | MTCK | IO | VDDPST1 | IE, WPU¹ | | ✓ | ✓ | ✓ |
| 13 | MTDO | IO | VDDPST1 | IE | | ✓ | ✓ | |
| 14 | GPIO8 | IO | VDDPST2 | IE | IE | ✓ | | |
| 15 | GPIO9 | IO | VDDPST2 | IE, WPU | IE, WPU | ✓ | | |
| 16 | GPIO10 | IO | VDDPST2 | IE | | ✓ | | |
| 17 | GPIO11 | IO | VDDPST2 | IE | | ✓ | | |
| 18 | GPIO12 | IO | VDDPST2 | IE | | ✓ | | ✓ |
| 19 | GPIO13 | IO | VDDPST2 | USB_PU | IE, USB_PU | ✓ | | ✓ |
| 20 | SPICS0 | IO | VDD_SPI | WPU | IE, WPU | ✓ | | |
| 21 | SPIQ | IO | VDD_SPI | WPU | IE, WPU | ✓ | | |
| 22 | SPIWP | IO | VDD_SPI | WPU | IE, WPU | ✓ | | |
| 23 | VDD_SPI | Power/IO | — | | | ✓ | | ✓ |
| 24 | SPIHD | IO | VDD_SPI | WPU | IE, WPU | ✓ | | |
| 25 | SPICLK | IO | VDD_SPI | WPU | IE, WPU | ✓ | | |
| 26 | SPID | IO | VDD_SPI | WPU | IE, WPU | ✓ | | |
| 27 | GPIO15 | IO | VDDPST2 | IE | IE | ✓ | | |
| 28 | VDDPST2 | Power | | | | | | |
| 29 | U0TXD | IO | VDDPST2 | WPU² | | ✓ | | |
| 30 | U0RXD | IO | VDDPST2 | IE, WPU | | ✓ | | |
| 31 | SDIO_CMD | IO | VDDPST2 | WPU | IE | ✓ | | |
| 32 | SDIO_CLK | IO | VDDPST2 | WPU | IE | ✓ | | |
| 33 | SDIO_DATA0 | IO | VDDPST2 | WPU | IE | ✓ | | |
| 34 | SDIO_DATA1 | IO | VDDPST2 | WPU | IE | ✓ | | |
| 35 | SDIO_DATA2 | IO | VDDPST2 | WPU | IE | ✓ | | |
| 36 | SDIO_DATA3 | IO | VDDPST2 | WPU | IE | ✓ | | |
| 37 | VDDA1 | Power | | | | | | |
| 38 | XTAL_N | Analog | | | | | | |
| 39 | XTAL_P | Analog | | | | | | |
| 40 | VDDA2 | Power | | | | | | |
| 41 | GND | Power | | | | | | |

¹ Depends on `EFUSE_DIS_PAD_JTAG`: `0` (default) → IE & WPU; `1` → IE only.
² Output enabled.

#### Table 2-2 — QFN32 pin overview

| Pin | Name | Type | Power from | At reset | After reset | IO MUX | LP IO MUX | Analog |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | ANT | Analog | | | | | | |
| 2 | VDDA3P3 | Power | | | | | | |
| 3 | VDDA3P3 | Power | | | | | | |
| 4 | CHIP_PU | Analog | VDDPST1 | | | | | |
| 5 | VDDPST1 | Power | | | | | | |
| 6 | XTAL_32K_P | IO | VDDPST1 | | | ✓ | ✓ | ✓ |
| 7 | XTAL_32K_N | IO | VDDPST1 | | | ✓ | ✓ | ✓ |
| 8 | GPIO2 | IO | VDDPST1 | IE | IE | ✓ | ✓ | ✓ |
| 9 | GPIO3 | IO | VDDPST1 | IE | IE | ✓ | ✓ | ✓ |
| 10 | MTMS | IO | VDDPST1 | IE | IE | ✓ | ✓ | ✓ |
| 11 | MTDI | IO | VDDPST1 | IE | IE | ✓ | ✓ | ✓ |
| 12 | MTCK | IO | VDDPST1 | IE, WPU¹ | | ✓ | ✓ | ✓ |
| 13 | MTDO | IO | VDDPST1 | IE | | ✓ | ✓ | |
| 14 | GPIO8 | IO | VDDPST2 | IE | IE | ✓ | | |
| 15 | GPIO9 | IO | VDDPST2 | IE, WPU | IE, WPU | ✓ | | |
| 16 | GPIO12 | IO | VDDPST2 | IE | | ✓ | | ✓ |
| 17 | GPIO13 | IO | VDDPST2 | USB_PU | IE, USB_PU | ✓ | | ✓ |
| 18 | GPIO14 | IO | VDDPST2 | IE | | ✓ | | |
| 19 | GPIO15 | IO | VDDPST2 | IE | IE | ✓ | | |
| 20 | VDDPST2 | Power | | | | | | |
| 21 | U0TXD | IO | VDDPST2 | WPU² | | ✓ | | |
| 22 | U0RXD | IO | VDDPST2 | IE, WPU | | ✓ | | |
| 23 | SDIO_CMD | IO | VDDPST2 | WPU | IE | ✓ | | |
| 24 | SDIO_CLK | IO | VDDPST2 | WPU | IE | ✓ | | |
| 25 | SDIO_DATA0 | IO | VDDPST2 | WPU | IE | ✓ | | |
| 26 | SDIO_DATA1 | IO | VDDPST2 | WPU | IE | ✓ | | |
| 27 | SDIO_DATA2 | IO | VDDPST2 | WPU | IE | ✓ | | |
| 28 | SDIO_DATA3 | IO | VDDPST2 | WPU | IE | ✓ | | |
| 29 | VDDA1 | Power | | | | | | |
| 30 | XTAL_N | Analog | | | | | | |
| 31 | XTAL_P | Analog | | | | | | |
| 32 | VDDA2 | Power | | | | | | |
| 33 | GND | Power | | | | | | |

**Drive strength:** the default drive strength is **20 mA for all pins except
GPIO12 and GPIO13, which default to 40 mA**.

**USB pins:** by default the USB function is enabled on GPIO12/GPIO13, and the pull-up
is decided by the USB pull-up resistor (`USB_SERIAL_JTAG_DP/DM_PULLUP`, value set by
`USB_SERIAL_JTAG_PULLUP_VALUE`). When USB is disabled these are ordinary GPIOs:
GPIO13's internal weak pull-up is **disabled at reset** and **enabled after reset**;
pull-ups/pull-downs are configurable via `IO_MUX_FUN_WPU` / `IO_MUX_FUN_WPD`.

### 4.3 IO MUX functions

Each IO pin can be connected to one of **three** IO MUX functions, `F0`–`F2`.
Among these:

- Some are routed via the **GPIO Matrix** (`GPIO0`, `GPIO1`, …), which is a
  programmable routing fabric giving a pin access to almost any peripheral signal.
  The datasheet warns that *"the flexibility of programmatic mapping comes at a cost
  as it might affect the latency of routed signals."*
- Some are **directly routed from certain peripherals** (`U0TXD`, `MTCK`, …) —
  UART0, JTAG, SPI0/1, SPI2 and SDIO.

Function-type notation:

| Type | Meaning |
| --- | --- |
| `I` | Input |
| `O` | Output |
| `T` | High impedance |
| `I1` | Input; if the pin is assigned a function other than `Fn`, the input signal of `Fn` is always **1** |
| `I0` | Input; if the pin is assigned a function other than `Fn`, the input signal of `Fn` is always **0** |

#### Table 2-3 — Peripheral signals routed via IO MUX

| Pin function | Signal | Interface |
| --- | --- | --- |
| `U0TXD` / `U0RXD` | Transmit / receive data | UART0 |
| `MTCK` / `MTDO` / `MTDI` / `MTMS` | Test clock / data out / data in / mode select | JTAG (debugging) |
| `SPIQ` `SPID` `SPIHD` `SPIWP` `SPICLK` `SPICS0` | MISO / MOSI / HOLD / WP / CLK / CS | 3.3 V SPI0/1 to in- or off-package flash; 1-, 2-, 4-line SPI |
| `FSPIQ` `FSPID` `FSPIHD` `FSPIWP` `FSPICLK` `FSPICS…` | MISO / MOSI / HOLD / WP / CLK / CS | **SPI2**, general-purpose fast SPI; 1-, 2-, 4-line |
| `SDIO_CMD` / `SDIO_CLK` / `SDIO_DATA…` | Command / clock / data | SDIO slave |

#### Table 2-4 — QFN40 IO MUX pin functions

| Pin | GPIO | F0 | type | F1 | type | F2 | type |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 6 | GPIO0 | GPIO0 | I/O/T | GPIO0 | I/O/T | | |
| 7 | GPIO1 | GPIO1 | I/O/T | GPIO1 | I/O/T | | |
| 8 | GPIO2 | GPIO2 | I/O/T | GPIO2 | I/O/T | FSPIQ | I1/O/T |
| 9 | GPIO3 | GPIO3 | I/O/T | GPIO3 | I/O/T | | |
| 10 | GPIO4 | **MTMS** | I1 | GPIO4 | I/O/T | FSPIHD | I1/O/T |
| 11 | GPIO5 | **MTDI** | I1 | GPIO5 | I/O/T | FSPIWP | I1/O/T |
| 12 | GPIO6 | **MTCK** | I1 | GPIO6 | I/O/T | FSPICLK | I1/O/T |
| 13 | GPIO7 | **MTDO** | O/T | GPIO7 | I/O/T | FSPID | I1/O/T |
| 14 | GPIO8 | GPIO8 | I/O/T | GPIO8 | I/O/T | | |
| 15 | GPIO9 | GPIO9 | I/O/T | GPIO9 | I/O/T | | |
| 16 | GPIO10 | GPIO10 | I/O/T | GPIO10 | I/O/T | | |
| 17 | GPIO11 | GPIO11 | I/O/T | GPIO11 | I/O/T | | |
| 18 | GPIO12 | GPIO12 | I/O/T | GPIO12 | I/O/T | | |
| 19 | GPIO13 | GPIO13 | I/O/T | GPIO13 | I/O/T | | |
| 20 | GPIO24 | **SPICS0** | O/T | GPIO24 | I/O/T | | |
| 21 | GPIO25 | **SPIQ** | I1/O/T | GPIO25 | I/O/T | | |
| 22 | GPIO26 | **SPIWP** | I1/O/T | GPIO26 | I/O/T | | |
| 23 | GPIO27 | GPIO27 | I/O/T | GPIO27 | I/O/T | | |
| 24 | GPIO28 | **SPIHD** | I1/O/T | GPIO28 | I/O/T | | |
| 25 | GPIO29 | **SPICLK** | O/T | GPIO29 | I/O/T | | |
| 26 | GPIO30 | **SPID** | I1/O/T | GPIO30 | I/O/T | | |
| 27 | GPIO15 | GPIO15 | I/O/T | GPIO15 | I/O/T | | |
| 29 | GPIO16 | **U0TXD** | O | GPIO16 | I/O/T | FSPICS0 | I1/O/T |
| 30 | GPIO17 | **U0RXD** | I1 | GPIO17 | I/O/T | FSPICS1 | O/T |
| 31 | GPIO18 | **SDIO_CMD** | I1/O/T | GPIO18 | I/O/T | FSPICS2 | O/T |
| 32 | GPIO19 | **SDIO_CLK** | I1 | GPIO19 | I/O/T | FSPICS3 | O/T |
| 33 | GPIO20 | **SDIO_DATA0** | I1/O/T | GPIO20 | I/O/T | FSPICS4 | O/T |
| 34 | GPIO21 | **SDIO_DATA1** | I1/O/T | GPIO21 | I/O/T | FSPICS5 | O/T |
| 35 | GPIO22 | **SDIO_DATA2** | I1/O/T | GPIO22 | I/O/T | | |
| 36 | GPIO23 | **SDIO_DATA3** | I1/O/T | GPIO23 | I/O/T | | |

Bold = the default function in the default boot mode.

#### Table 2-5 — QFN32 IO MUX pin functions

Identical to the QFN40 table for the pins that exist, minus GPIO10, GPIO11 and
GPIO24–GPIO30, plus GPIO14:

| Pin | GPIO | F0 | type | F1 | type | F2 | type |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 6 | GPIO0 | GPIO0 | I/O/T | GPIO0 | I/O/T | | |
| 7 | GPIO1 | GPIO1 | I/O/T | GPIO1 | I/O/T | | |
| 8 | GPIO2 | GPIO2 | I/O/T | GPIO2 | I/O/T | FSPIQ | I1/O/T |
| 9 | GPIO3 | GPIO3 | I/O/T | GPIO3 | I/O/T | | |
| 10 | GPIO4 | **MTMS** | I1 | GPIO4 | I/O/T | FSPIHD | I1/O/T |
| 11 | GPIO5 | **MTDI** | I1 | GPIO5 | I/O/T | FSPIWP | I1/O/T |
| 12 | GPIO6 | **MTCK** | I1 | GPIO6 | I/O/T | FSPICLK | I1/O/T |
| 13 | GPIO7 | **MTDO** | O/T | GPIO7 | I/O/T | FSPID | I1/O/T |
| 14 | GPIO8 | GPIO8 | I/O/T | GPIO8 | I/O/T | | |
| 15 | GPIO9 | GPIO9 | I/O/T | GPIO9 | I/O/T | | |
| 16 | GPIO12 | GPIO12 | I/O/T | GPIO12 | I/O/T | | |
| 17 | GPIO13 | GPIO13 | I/O/T | GPIO13 | I/O/T | | |
| 18 | GPIO14 | GPIO14 | I/O/T | GPIO14 | I/O/T | | |
| 19 | GPIO15 | GPIO15 | I/O/T | GPIO15 | I/O/T | | |
| 21 | GPIO16 | **U0TXD** | O | GPIO16 | I/O/T | FSPICS0 | I1/O/T |
| 22 | GPIO17 | **U0RXD** | I1 | GPIO17 | I/O/T | FSPICS1 | O/T |
| 23 | GPIO18 | **SDIO_CMD** | I1/O/T | GPIO18 | I/O/T | FSPICS2 | O/T |
| 24 | GPIO19 | **SDIO_CLK** | I1 | GPIO19 | I/O/T | FSPICS3 | O/T |
| 25 | GPIO20 | **SDIO_DATA0** | I1/O/T | GPIO20 | I/O/T | FSPICS4 | O/T |
| 26 | GPIO21 | **SDIO_DATA1** | I1/O/T | GPIO21 | I/O/T | FSPICS5 | O/T |
| 27 | GPIO22 | **SDIO_DATA2** | I1/O/T | GPIO22 | I/O/T | | |
| 28 | GPIO23 | **SDIO_DATA3** | I1/O/T | GPIO23 | I/O/T | | |

### 4.4 LP IO MUX functions

In **Deep-sleep mode the ordinary IO MUX stops working**. The LP IO MUX takes over
for the eight LP-capable pins, which stay connected to the LP system and are
powered by `VDDPST1`.

An LP IO pin can either act as an **LP GPIO** (`LP_GPIO0`…), driven by the LP CPU,
or carry an **LP peripheral signal**.

#### Table 2-6 — LP peripheral signals routed via LP IO MUX

| Pin function | Signal | Interface |
| --- | --- | --- |
| `LP_I2C_SDA` | Serial data | LP I2C |
| `LP_I2C_SCL` | Serial clock | LP I2C |
| `LP_UART_RXD` | Receive | LP UART |
| `LP_UART_TXD` | Transmit | LP UART |
| `LP_UART_RTSN` | Request to send | LP UART |
| `LP_UART_CTSN` | Clear to send | LP UART |
| `LP_UART_DTRN` | Data set ready | LP UART |
| `LP_UART_DSRN` | Data terminal ready | LP UART |

#### Table 2-7 — LP IO MUX functions

| Pin | LP IO name | F0 | F1 |
| --- | --- | --- | --- |
| 6 | LP_GPIO0 | **LP_GPIO0** | LP_UART_DTRN |
| 7 | LP_GPIO1 | **LP_GPIO1** | LP_UART_DSRN |
| 8 | LP_GPIO2 | **LP_GPIO2** | LP_UART_RTSN |
| 9 | LP_GPIO3 | **LP_GPIO3** | LP_UART_CTSN |
| 10 | LP_GPIO4 | **LP_GPIO4** | LP_UART_RXD |
| 11 | LP_GPIO5 | **LP_GPIO5** | LP_UART_TXD |
| 12 | LP_GPIO6 | **LP_GPIO6** | LP_I2C_SDA |
| 13 | LP_GPIO7 | **LP_GPIO7** | LP_I2C_SCL |

LP GPIO numbering maps 1:1 onto HP GPIO0–GPIO7 (`LP_GPIO4` is the same physical
pin as `GPIO4` / `MTMS`).

### 4.5 Analog functions

#### Table 2-8 — Analog signals routed to analog functions

| Pin function | Signal | Description |
| --- | --- | --- |
| `ADC1_CH…` | ADC1 channel signal | ADC1 interface |
| `XTAL_32K_P` / `XTAL_32K_N` | Positive / negative clock | 32 kHz external clock in/out to crystal or oscillator |
| `USB_D-` / `USB_D+` | Data − / Data + | USB Serial/JTAG |

#### Table 2-9 — Analog functions

| QFN40 pin | QFN32 pin | IO | F0 | F1 |
| --- | --- | --- | --- | --- |
| 6 | 6 | GPIO0 | XTAL_32K_P | ADC1_CH0 |
| 7 | 7 | GPIO1 | XTAL_32K_N | ADC1_CH1 |
| 8 | 8 | GPIO2 | | ADC1_CH2 |
| 9 | 9 | GPIO3 | | ADC1_CH3 |
| 10 | 10 | GPIO4 | | ADC1_CH4 |
| 11 | 11 | GPIO5 | | ADC1_CH5 |
| 12 | 12 | GPIO6 | | ADC1_CH6 |
| 18 | 16 | GPIO12 | USB_D− | |
| 19 | 17 | GPIO13 | USB_D+ | |
| 23 | — | GPIO27 | VDD_SPI | |

**So the seven ADC channels are GPIO0–GPIO6, and only those.**

### 4.6 GPIO restrictions

Certain IO pins need extra caution before being repurposed as general GPIO:

| Category | Pins | Why |
| --- | --- | --- |
| **Flash pins** | GPIO24, 25, 26, 28, 29, 30 (QFN40 only) | Allocated for communication with flash. **Not recommended for anything else.** |
| **Strapping pins** | GPIO4 (MTMS), GPIO5 (MTDI), GPIO8, GPIO9, GPIO15 | Must be at defined logic levels at startup — see [§5](#5-boot-configuration-and-strapping-pins). |
| **USB_D+/−** | GPIO12, GPIO13 | Connected to the USB Serial/JTAG controller by default; must be explicitly reconfigured to act as GPIO. |
| **JTAG interface** | GPIO4, GPIO5, GPIO6, GPIO7 | Commonly used for debugging. Can be freed by using the USB Serial/JTAG controller's `USB_D+/−` instead. |
| **UART0** | GPIO16, GPIO17 | Commonly used for debugging/console. |
| **VDD_SPI** | GPIO27 (QFN40 only) | Power supply pin for off-package flash by default; usable as GPIO **only** if the flash is powered externally. |

### 4.7 Peripheral pin assignment priorities

Tables 2-10 (QFN40) and 2-11 (QFN32) classify every pin/peripheral combination into
four priorities:

| Priority | Meaning |
| --- | --- |
| **P1** | Fixed pins connected directly to peripheral signals via IO MUX or RTC IO MUX. Lowest latency. |
| **P2** | GPIO pins reachable via GPIO Matrix, **freely usable without restrictions**. |
| **P3** | GPIO pins reachable via GPIO Matrix, **use with caution** — collide with a strap, JTAG, UART0, USB, or VDD_SPI. |
| **P4** | GPIO pins **already allocated or not recommended** (SPI0/1 flash pins). |

Concretely, the caution/no-go lists are:

- **P3 causes:**
  - `GPIO4, GPIO5, GPIO8, GPIO9, GPIO15` — strapping pins
  - `GPIO12, GPIO13` — USB Serial/JTAG interface
  - `GPIO4, GPIO5, GPIO6, GPIO7` — JTAG interface
  - `GPIO16, GPIO17` — UART0
  - `GPIO27` — VDD_SPI (QFN40)
- **P4:** `GPIO24, GPIO25, GPIO26, GPIO28, GPIO29, GPIO30` — SPI0/1 flash

If a peripheral has no P1 pin (e.g. **UART1**) it can go on any GPIO from P2–P4.
If a peripheral has no P2–P4 pins (e.g. **USB Serial/JTAG**) it can *only* use its
P1 pins.

#### QFN32 priority map — the one that applies to this board

| GPIO | Pin | P1 (fixed) functions | Priority for general peripherals |
| --- | --- | --- | --- |
| GPIO0 | 6 | `LP_UART_DTRN`, `ADC1_CH0`, `XTAL_32K_P` | **P2** |
| GPIO1 | 7 | `LP_UART_DSRN`, `ADC1_CH1`, `XTAL_32K_N` | **P2** |
| GPIO2 | 8 | `LP_UART_RTSN`, `ADC1_CH2`, `FSPIQ` | **P2** |
| GPIO3 | 9 | `LP_UART_CTSN`, `ADC1_CH3` | **P2** |
| GPIO4 | 10 | `MTMS`, `LP_UART_RXD`, `ADC1_CH4`, `FSPIHD` | **P3** (strap + JTAG) |
| GPIO5 | 11 | `MTDI`, `LP_UART_TXD`, `ADC1_CH5`, `FSPIWP` | **P3** (strap + JTAG) |
| GPIO6 | 12 | `MTCK`, `LP_I2C_SDA`, `ADC1_CH6`, `FSPICLK` | **P3** (JTAG) |
| GPIO7 | 13 | `MTDO`, `LP_I2C_SCL`, `FSPID` | **P3** (JTAG) |
| GPIO8 | 14 | — | **P3** (strap) |
| GPIO9 | 15 | — | **P3** (strap) |
| GPIO12 | 16 | `USB_D−` | **P3** (USB) |
| GPIO13 | 17 | `USB_D+` | **P3** (USB) |
| GPIO14 | 18 | — | **P2** |
| GPIO15 | 19 | — | **P3** (strap) |
| GPIO16 | 21 | `U0TXD`, `FSPICS0` | **P3** (UART0) |
| GPIO17 | 22 | `U0RXD`, `FSPICS1` | **P3** (UART0) |
| GPIO18 | 23 | `SDIO_CMD`, `FSPICS2` | **P2** |
| GPIO19 | 24 | `SDIO_CLK`, `FSPICS3` | **P2** |
| GPIO20 | 25 | `SDIO_DATA0`, `FSPICS4` | **P2** |
| GPIO21 | 26 | `SDIO_DATA1`, `FSPICS5` | **P2** |
| GPIO22 | 27 | `SDIO_DATA2` | **P2** |
| GPIO23 | 28 | `SDIO_DATA3` | **P2** |

Peripherals that can be mapped through the GPIO Matrix to *any* GPIO:
UART0, UART1, SPI2, I2C, I2S, PCNT, TWAI, LED PWM, MCPWM, RMT, PARLIO.
**USB Serial/JTAG is P1-only** (GPIO12/GPIO13), though `USB_D−`/`USB_D+` can be
swapped by setting `USB_SERIAL_JTAG_EXCHG_PINS`.

### 4.8 Analog and power pins

#### Table 2-12 — Analog pins

| QFN40 | QFN32 | Name | Type | Function |
| --- | --- | --- | --- | --- |
| 1 | 1 | `ANT` | I/O | RF input and output |
| 4 | 4 | `CHIP_PU` | — | **High: chip on (powered up). Low: chip off.** *Do not leave floating.* |
| 38 | 30 | `XTAL_N` | — | External clock in/out to the crystal or oscillator (differential negative) |
| 39 | 31 | `XTAL_P` | — | …(differential positive) |

#### Table 2-13 — Power pins

| QFN40 | QFN32 | Name | Direction | Power domain / IO pins |
| --- | --- | --- | --- | --- |
| 2 | 2 | `VDDA3P3` | Input | Analog power domain |
| 3 | 3 | `VDDA3P3` | Input | Analog power domain |
| 5 | 5 | `VDDPST1` | Input | LP digital + part of analog pin power domains; **LP IO** |
| 23 | — | `VDD_SPI` | Input | In-package flash (backup power line) |
| 23 | — | `VDD_SPI` | Output | In-package **and** off-package flash |
| 28 | 20 | `VDDPST2` | Input | HP digital power domain; **HP IO** |
| 37 | 29 | `VDDA1` | Input | Analog power domain |
| 40 | 32 | `VDDA2` | Input | Analog power domain |
| 41 | 33 | `GND` | — | External ground connection |

### 4.9 Power scheme, power-up and reset

Two internal regulators generate the 1.1 V rails:

| Voltage regulator | Output | Powers |
| --- | --- | --- |
| **LP** | 1.1 V | LP power domain |
| **HP** | 1.1 V | HP power domain |

```
                    ┌──────────────┐
 VDD_PST1  ────────►│ LP Regulator │──► LP System
        │           └──────────────┘
        └──► LP IO
                    ┌──────────────┐
 VDD_PST2  ────────►│ HP Regulator │──► HP System
        │           └──────────────┘
        ├──► HP IO
        └──[ R_SPI ]──► VDD_SPI ──► flash
 VDDA1, VDDA2 ─────► Analog
```

#### Power-up and reset timing (Table 2-15)

| Parameter | Description | Min |
| --- | --- | --- |
| `t_STBL` | Time for `VDDA3P3`, `VDDPST1`, `VDDPST2`, `VDDA1`, `VDDA2` to stabilise **before** `CHIP_PU` is pulled high | **50 µs** |
| `t_RST` | Time `CHIP_PU` must stay below `V_IL_nRST` to reset the chip | **50 µs** |

Sequence: rails come up → wait ≥ `t_STBL` → `CHIP_PU` rises above `V_IH_nRST`
(≥ 0.75 × VDD) → chip active. To reset, hold `CHIP_PU` below `V_IL_nRST`
(≤ 0.25 × VDD) for ≥ `t_RST`.

### 4.10 Chip-to-flash pin mapping

#### Table 2-16 — QFN40 chip ↔ off-package flash

| Pin | Name | Single SPI | Dual SPI | Quad SPI / QPI |
| --- | --- | --- | --- | --- |
| 25 | `SPICLK` | CLK | CLK | CLK |
| 20 | `SPICS0` | CS# | CS# | CS# |
| 26 | `SPID` | MOSI | SIO0 | SIO0 |
| 21 | `SPIQ` | MISO | SIO1 | SIO1 |
| 22 | `SPIWP` | — | WP# | SIO2 |
| 24 | `SPIHD` | — | HOLD# | SIO3 |

> **Notice (verbatim):** *Do not use the pins connected to flash for any other purposes.*

On **QFN32** variants with in-package flash these pins are **not routed out** —
the table is reference only.

---

## 5. Boot configuration and strapping pins

Boot parameters are configured through **strapping pins** and **eFuse parameters**
at power-up or hardware reset, without any microcontroller interaction.

| Parameter | Strapping pin(s) | eFuse parameter(s) |
| --- | --- | --- |
| Chip boot mode | `GPIO8`, `GPIO9` | — |
| SDIO sampling / driving clock edge | `MTMS`, `MTDI` | — |
| ROM message printing | `GPIO8` | `EFUSE_UART_PRINT_CONTROL`, `EFUSE_DIS_USB_SERIAL_JTAG_ROM_PRINT` |
| JTAG signal source | `GPIO15` | `EFUSE_DIS_PAD_JTAG`, `EFUSE_DIS_USB_JTAG`, `EFUSE_JTAG_SEL_ENABLE` |

**All the eFuse parameters above default to 0 (not burnt). eFuse is one-time
programmable: once a bit is programmed to 1 it can never be reverted to 0.**

### Table 3-1 — Default configuration of strapping pins

| Strapping pin | Default configuration | Bit value |
| --- | --- | --- |
| `MTMS` | Floating | – |
| `MTDI` | Floating | – |
| `GPIO8` | Floating | – |
| `GPIO9` | **Weak pull-up** | **1** |
| `GPIO15` | Floating | – |

Defaults are set by the pins' internal weak pull-up/pull-down resistors, and hold
only if the pin is unconnected or connected to a high-impedance circuit. To change
a bit value, add an external pull-up/pull-down — or, if the ESP32-C6 is a device
under a host MCU, drive the level from the host.

**All strapping pins have latches.** At Chip Reset the latches sample the pins and
hold the values until the chip is powered down or shut down; the states cannot be
changed any other way. This means the strap values remain available for the whole
chip operation while the pins are freed up as ordinary IO after reset.

### Table 3-2 — Strapping pin timing

| Parameter | Description | Min |
| --- | --- | --- |
| `t_SU` | Setup time — time for the power rails to stabilise before `CHIP_PU` goes high | **0 ms** |
| `t_H` | Hold time — time for the chip to read the strapping values after `CHIP_PU` is high and before the pins start behaving as regular IO | **3 ms** |

### 5.1 Chip boot mode control (Table 3-3)

| Boot mode | GPIO8 | GPIO9 |
| --- | --- | --- |
| **SPI boot mode** (default) | any value | **1** |
| Joint download boot mode | 1 | 0 |

Joint download boot mode supports:

- **USB-Serial-JTAG Download Boot**
- **UART Download Boot**
- **SDIO Download Boot**

### 5.2 SDIO sampling / driving clock edge (Table 3-4)

| Edge behaviour | MTMS | MTDI |
| --- | --- | --- |
| Falling-edge sampling, falling-edge output | 0 | 0 |
| Falling-edge sampling, rising-edge output | 0 | 1 |
| Rising-edge sampling, falling-edge output | 1 | 0 |
| Rising-edge sampling, rising-edge output | 1 | 1 |

`MTMS` and `MTDI` float by default, so **none of these is a default configuration.**

### 5.3 ROM message printing control

ROM message printing is enabled while `LP_AON_STORE4_REG[0]` is `0` (default) and
disabled when it is `1`. When enabled, messages can go to:

- **(default)** UART0 **and** the USB Serial/JTAG controller
- USB Serial/JTAG controller only
- UART0 only

#### Table 3-5 — UART0 ROM message printing

| UART0 ROM code printing | `EFUSE_UART_PRINT_CONTROL` | GPIO8 |
| --- | --- | --- |
| **Enabled** | **0** | **ignored** |
| Enabled | 1 | 0 |
| Enabled | 2 | 1 |
| Disabled | 1 | 1 |
| Disabled | 2 | 0 |
| Disabled | 3 | ignored |

#### Table 3-6 — USB Serial/JTAG ROM message printing

| USB Serial/JTAG ROM code printing | `EFUSE_DIS_USB_SERIAL_JTAG` | `EFUSE_DIS_USB_SERIAL_JTAG_ROM_PRINT` |
| --- | --- | --- |
| **Enabled** | **0** | **0** |
| Disabled | 0 | 1 |
| Disabled | 1 | ignored |

### 5.4 JTAG signal source control (Table 3-7)

`GPIO15` selects the JTAG signal source during early boot. **This pin has no
internal pull resistors** — the strapping value must be driven by an external
circuit that cannot be high-impedance.

| JTAG signal source | `EFUSE_DIS_PAD_JTAG` | `EFUSE_DIS_USB_JTAG` | `EFUSE_JTAG_SEL_ENABLE` | GPIO15 |
| --- | --- | --- | --- | --- |
| **USB Serial/JTAG controller** | **0** | **0** | **0** | **ignored** |
| USB Serial/JTAG controller | 0 | 0 | 1 | 1 |
| USB Serial/JTAG controller | 1 | 0 | ignored | ignored |
| JTAG pins (MTDI/MTCK/MTMS/MTDO) | 0 | 0 | 1 | 0 |
| JTAG pins | 0 | 1 | ignored | ignored |
| JTAG disabled | 1 | 1 | ignored | ignored |

---

## 6. System

### 6.1 High-performance CPU

The **ESP-RISC-V CPU (HP CPU)** is a 32-bit core implementing base integer (I),
multiplication/division (M), atomic (A) and compressed (C) standard extensions.

- Four-stage pipeline, clock up to **160 MHz**
- **RV32IMAC** ISA
- Compatible with *RISC-V ISA Manual Vol. I: Unprivileged ISA v2.2* and
  *Vol. II: Privileged Architecture v1.10*
- **Zero wait-cycle access** to on-chip SRAM and cache over the IRAM/DRAM interface
- Branch target buffer (BTB) with static branch prediction
- User (U) mode support with interrupt delegation
- Interrupt controller: up to **28 external vectored interrupts** for M and U modes,
  **16 programmable priority and threshold levels**
- Core local interrupts (CLINT) per privilege mode
- Debug module compliant with *RISC-V External Debug Support v0.13*, external
  debugger over standard JTAG/USB
- Instruction trace support (see below)
- Hardware triggers: up to **4 breakpoints/watchpoints**
- **PMP and PMA** for up to **16 configurable regions**

### 6.2 RISC-V Trace Encoder

Captures the HP CPU's instruction trace, compresses it into small packets, stores
them in internal SRAM.

- Compatible with *RISC-V Processor Trace v1.0*
- Synchronisation packets sent every few clock cycles or packets
- Zero bytes as anchor tags to identify packet boundaries
- Configurable memory-writing mode: **loop** or **non-loop**
- Trace-lost status flag; automatic restart after packet loss

### 6.3 Low-power CPU

32-bit RV32IMAC processor designed for ultra-low power; **stays powered during
Deep-sleep while the HP CPU is powered down**.

- Two-stage pipeline, up to **20 MHz**
- **19 vector interrupts**
- Debug module + hardware triggers (up to 2 breakpoints/watchpoints), *RISC-V
  External Debug Support v0.13*
- 32-bit AHB system bus for peripheral and memory access
- Core performance metric events
- **Can wake the HP CPU and send it an interrupt**
- Access to HP memory, LP memory, and the entire peripheral address space

### 6.4 GDMA controller

Peripheral↔memory and memory↔memory transfers without CPU intervention.

- **6 independent channels: 3 TX + 3 RX**, shared by SPI2, UHCI (UART0/UART1),
  I2S, AES, SHA, ADC, PARLIO
- Programmable transfer length in bytes
- **Linked list of descriptors**
- INCR burst transfer when accessing internal RAM
- Address space of up to **384 KB** in internal RAM
- Software-selectable requesting peripheral
- **Fixed-priority and round-robin** channel arbitration
- Event Task Matrix support

### 6.5 Memory organisation

#### Internal memory

| Region | Size | Notes |
| --- | --- | --- |
| ROM | **320 KB** | Booting and core functions |
| HP SRAM | **512 KB** | Data and instructions |
| LP SRAM | **16 KB** | Accessible by HP or LP CPU; **retains data in Deep-sleep** |
| eFuse | **4096 bits** | 1792 bits available to users |
| In-package flash | 4 MB (FH4) / 8 MB (FH8) | See [§10.7](#107-memory-specifications) |

#### External memory

Connected via SPI / Dual SPI / Quad SPI / QPI.

- Off-package flash up to **16 MB**
- Hardware **XTS-AES** encryption/decryption
- Up to 16 MB of CPU **instruction** memory space mappable to flash in **64 KB
  blocks**; 32-bit fetch supported
- Up to 16 MB of CPU **data** memory space mappable to flash in 64 KB blocks;
  8-, 16- and 32-bit reads supported
- Access through a **32 KB read-only cache**: four-way set associative, 32-byte
  cache block, critical-word-first and early restart

#### eFuse controller

One-time programmable storage for parameters and user data.

- Configurable **write protection** for some blocks
- Configurable **read protection** for some blocks
- Various hardware encoding schemes guard against data corruption

### 6.6 System components

#### IO MUX and GPIO Matrix

- **30 or 22 GPIO pins**
- GPIO Matrix routes **85 peripheral input** and **93 output** signals to any GPIO
- Signal synchronisation for peripheral inputs based on the IO MUX operating clock
- **GPIO Filter** and a second-stage **Glitch Filter** for input signals
- Sigma-delta modulated (SDM) output
- IO MUX directly connects SPI, JTAG and UART signals to pins
- LP IO MUX controls eight LP GPIO pins (GPIO0–GPIO7) for LP-system peripherals
- Event Task Matrix support

#### Reset

Four reset levels; **all except Chip Reset preserve data in internal memory**.

| Reset type | Scope |
| --- | --- |
| **CPU Reset** | The CPU core |
| **Core Reset** | The whole digital system **except** the LP system |
| **System Reset** | The whole digital system **including** the LP system |
| **Chip Reset** | The whole chip |

Triggers: directly by hardware, or by software writing the corresponding CPU
registers. **Reset cause retrieval is supported.**

#### Clock

High-speed clocks for the HP system:

- **40 MHz external crystal clock** — ⚠︎ **the chip cannot operate without it**
- **480 MHz internal PLL clock**

Slow-speed clocks for the LP system and low-power peripherals:

- 32 kHz external crystal clock
- Internal **fast RC oscillator**, adjustable, **17.5 MHz by default**
- **136 kHz** internal slow RC oscillator
- External slow clock input through `XTAL_32K_P` (32 kHz by default)

*(Datasheet v1.3 removed the "32 kHz internal slow RC oscillator" per AR2024-011.)*

#### Interrupt Matrix

- **77 peripheral interrupt sources** in
- **31 CPU peripheral interrupts** out
- Current interrupt status query per source
- **Shared interrupts** — multiple sources may map onto one CPU interrupt

#### Event Task Matrix (ETM)

Maps events from any peripheral to tasks of any peripheral so peripherals act
**without CPU intervention**.

- **50 independently enabled/configured channels**
- Receives **124 events**, generates **130 tasks**
- Participating peripherals: GPIO, LED PWM, general-purpose timers, RTC Timer,
  system timer, MCPWM, temperature sensor, ADC, I2S, LP CPU, GDMA, PMU

#### System Timer (SYSTIMER)

52-bit timer for OS tick interrupts or general periodic/one-shot interrupts.

- **Two 52-bit counters, three 52-bit comparators**
- 52-bit alarm values, 26-bit alarm periods
- Two alarm modes: **target** and **period**
- Three independent interrupts from the three comparators
- Sleep time recorded by the RTC timer can be loaded back by software after
  Deep-sleep or Light-sleep
- Counters can stall with the CPU or in OCD mode
- Real-time alarm events

#### Power Management Unit

The PMU can be configured to power up different power domains to balance
performance, power consumption and wake-up latency. Because configuring it
directly is complex, four predefined modes exist:

| Mode | What is on | Notes |
| --- | --- | --- |
| **Active** | Everything | Full processing, continuous wireless communication |
| **Modem-sleep** | HP CPU, all HP peripherals, LP system; **Wi-Fi is clock-gated** | Continuous CPU operation, intermittent wireless. RF circuits and wireless MAC/baseband power on only for a transmission. |
| **Light-sleep** | All HP peripherals and the LP system, by default | Resumes execution **without re-initialising the context** on wake-up |
| **Deep-sleep** | Only LP system components: LP SRAM, RTC Timer, Brownout Detector, PMU, … | Lowest possible power, longest wake-up |

Consumption figures: [§10.6](#106-current-consumption).

#### Timer Group (TIMG)

Two timer groups, each with one general-purpose timer + one Main System Watchdog.

- **16-bit prescaler**
- **54-bit auto-reload-capable up/down counter**
- Real-time read of the time-base counter; halt / resume / disable
- Programmable alarm generation; timer value reload (auto at alarm, or
  software-controlled instant reload)
- RTC slow-clock frequency calculation
- Real-time alarm events, level interrupt generation
- Several ETM tasks and events

#### Watchdog timers

Three **digital** watchdogs — one Main WDT (MWDT) per timer group and one RTC WDT
(RWDT) — plus one **analog** Super Watchdog (SWD).

Digital WDT:

- **Four stages**, each with an independently programmable timeout value and action
- Timeout actions: **interrupt, CPU reset, core reset, system reset (RWDT only)**
- Flash-boot protection under SPI boot mode at stage 0
- Write protection makes WDT registers read-only until unlocked
- 32-bit timeout counter

Analog WDT (SWD):

- Timeout period **slightly less than one second**
- Timeout actions: interrupt, system reset

#### Permission Control (PMS)

Two parts: **PMP** (Physical Memory Protection) and **APM** (Access Permission
Management).

- Access permission management for ROM, HP memory, HP peripheral, LP memory and
  LP peripheral address spaces
- APM lets **each master (e.g. DMA) select one of four security modes**
- Access permission configuration for up to **16 address ranges**
- Interrupt function and exception information recording

#### System registers (HP_SYSREG)

Control of external memory encryption/decryption, HP core / LP core debugging, and
bus timeout protection.

#### Debug Assistant (ASSIST_DEBUG)

- **Read/write monitoring** of a specified HP-CPU-bus memory address space
- **Stack pointer monitoring** — prevents stack overflow / erroneous push-pop;
  a violation triggers an interrupt
- **Program counter logging** — the last PC value at the most recent HP CPU reset
- **Bus access logging** when the HP CPU, LP CPU or DMA writes a specified value

---

## 7. Cryptography and security

| Accelerator | Capabilities |
| --- | --- |
| **AES** | Two working modes. *Typical AES*: AES-128/256 encrypt & decrypt. *DMA-AES*: AES-128/256 plus block cipher modes **ECB, CBC, OFB, CTR, CFB8, CFB128**; interrupt on completion. |
| **ECC** | Two curves: **P-192 and P-256**. Six working modes covering Base Point Verification, Base Point Multiplication, Jacobian Point Verification, Jacobian Point Multiplication. Smaller public keys than RSA at equivalent security. |
| **HMAC** | Standard **HMAC-SHA-256** (RFC 2104), keyed from eFuse. *Downstream mode*: result not accessible to software (high security), generates keys for RSA_DS, and can **re-enable soft-disabled JTAG**. *Upstream mode*: result readable by software. |
| **RSA** | Large-number **modular exponentiation** (two acceleration options) and **modular multiplication** up to **3072-bit** operands; large-number multiplication up to **1536-bit**; operands of differing widths; interrupt on completion. |
| **SHA** | **SHA-1, SHA-224, SHA-256**. Two modes: *Typical SHA* (CPU-driven) and *DMA-SHA*. |
| **RSA_DS** (RSA Digital Signature) | RSA digital signatures with keys up to **3072 bits**; private key data stored encrypted and decryptable only by RSA_DS; **SHA-256 digest** protects the private key against tampering. |
| **XTS_AES** (external memory encryption) | General **XTS-AES** per **IEEE Std 1619-2007**; software-based manual encryption; high-speed automatic decryption with no software involvement; enabled/disabled jointly by register configuration, eFuse parameters and boot mode; configurable **anti-DPA**. |
| **RNG** | True random number generator producing 32-bit values. Entropy sources: **thermal noise** from the high-speed ADC or SAR ADC, and an **asynchronous clock mismatch**. |

---

## 8. Peripherals

### 8.1 UART controller

Two UARTs in the main system plus one **LP UART**.

- Programmable baud rates up to **5 MBaud**
- RAM shared between the TX and RX FIFOs
- Various data-bit and stop-bit lengths; parity bit
- Special character **AT_CMD** detection
- **RS485** protocol support *(not on LP UART)*
- **IrDA** protocol support *(not on LP UART)*
- High-speed data via **GDMA** *(not on LP UART)*
- Receive timeout
- **UART as a wake-up source**
- Software and hardware flow control

### 8.2 SPI controller

| Controller | Purpose |
| --- | --- |
| **SPI0** | Used by the cache and GDMA to access in-package or off-package flash |
| **SPI1** | Used by the CPU to access in-package or off-package flash |
| **SPI2** | **General-purpose**, with access to general-purpose DMA channels |

**SPI0 and SPI1 are reserved for system use. Only SPI2 is available to users.**

SPI0/SPI1 features: Single/Dual/Quad SPI (QPI) modes; byte-wise transmission.

SPI2 features:

- Master **or** slave operation, with GDMA support
- Single / Dual / Quad SPI (QPI) modes
- Configurable clock polarity (**CPOL**) and phase (**CPHA**), configurable clock
  frequency
- Byte-wise transmission; configurable bit order (**MSB-first or LSB-first**)
- **As master:** 2-line full-duplex up to **80 MHz**; 1-/2-/4-line half-duplex up to
  **80 MHz**; **six `FSPICS…` pins for six independent slaves**; configurable CS
  setup and hold time
- **As slave:** 2-line full-duplex up to **40 MHz**; 1-/2-/4-line half-duplex up to
  **40 MHz**

### 8.3 I2C controller

- **Two controllers**: one in the main system, one in the low-power system
- Master and slave modes for I2C; **master mode only for LP I2C**
- **Standard mode (100 Kbit/s)** and **fast mode (400 Kbit/s)**
- SCL clock stretching in slave mode
- Programmable digital noise filtering
- **7-bit and 10-bit addressing**, plus dual address mode

### 8.4 I2S controller

- Master and slave modes; full-duplex and half-duplex
- Separate TX and RX units, independent or simultaneous
- Audio standards: **TDM Philips**, **TDM MSB-aligned**, **TDM PCM**, **PDM**
- **PCM-to-PDM TX** interface
- Configurable high-precision BCK clock up to **40 MHz**
  - Sampling rates 8, 16, 32, 44.1, 48, 88.2, 96, 128, 192 kHz, etc.
- **8-/16-/24-/32-bit** data
- DMA
- **A-law and µ-law** compression/decompression
- Flexible data format control

### 8.5 Pulse Count controller (PCNT)

- **Four independent pulse counters with two channels each**
- Counter modes: increment, decrement, disable
- Glitch filtering on input pulse and control signals
- Selectable counting on **rising or falling** edges

### 8.6 USB Serial/JTAG controller

Integrated CDC-ACM serial port **and** JTAG adapter — no external USB-UART chip or
JTAG probe needed.

- **USB 2.0 full speed**, up to **12 Mbit/s** *(no 480 Mbit/s high-speed mode)*
- CDC-ACM serial-port emulation, plug-and-play on most modern OSes
- **Host-controllable chip reset and entry into download mode**
- JTAG adapter: fast communication with the CPU debug core using a compact JTAG
  instruction representation
- **Reprogramming of attached flash through the ROM startup code**
- **Internal PHY**

### 8.7 Two-wire Automotive Interface (TWAI®)

- Compatible with **ISO 11898-1** (CAN Specification 2.0)
- Standard (11-bit ID) and extended (29-bit ID) frame formats
- Bit rates **1 Kbit/s – 1 Mbit/s**
- Operating modes: **Normal, Listen Only, Self-Test** (no acknowledgment required)
- Special transmissions: **single-shot** and **self-reception**
- Acceptance filter with **single and dual filter modes**
- Error detection and handling: error counters, configurable error warning limit,
  error code capture, arbitration-lost capture, automatic transceiver standby

*(The chip has two TWAI controllers — see [§3.5](#35-peripheral-summary).)*

### 8.8 SDIO slave controller

- Compatible with **SD Physical Layer Specification V2.00** and **SDIO V2.00**
- **SPI, 1-bit SDIO and 4-bit SDIO** transfer modes
- Clock range **0 – 50 MHz**; configurable sample and drive clock edge
- Integrated, SDIO-accessible registers for information interchange
- SDIO interrupt mechanism; **bidirectional interrupt vector** between host and slave
- Automatic padding, and discarding of padded data, on the SDIO bus
- Block size up to **512 bytes**
- DMA for data transfer
- **Wake-up from sleep while the connection is retained**

### 8.9 LED PWM controller (LEDC)

- **Six independent PWM generators**
- Maximum PWM duty-cycle resolution of **20 bits**
- **Four independent timers** with 20-bit counters, configurable fractional clock
  dividers and counter overflow values
- Adjustable output phase
- **PWM duty cycle dithering**
- Automatic duty-cycle fading:
  - **Linear** fading (one duty-cycle range)
  - **Gamma-curve** fading — up to **16 duty-cycle ranges per generator**, each with
    independent fading direction, amount, count and frequency
- **PWM output continues in Light-sleep mode**
- Event generation and task response via ETM

### 8.10 Motor Control PWM (MCPWM)

Five modules: PWM timers, PWM operators, capture, fault detection, ETM.

- **Three PWM timers**
  - Dedicated **8-bit clock prescaler** each
  - **16-bit counter** in count-up, count-down or count-up-down mode
  - Hardware or software synchronisation to reload the timer or restart the
    prescaler, with selectable hardware sync source
- **Three PWM operators** generating waveform pairs
  - **Six PWM outputs** for several topologies
  - Independently configurable **dead time** on rising and falling edges
  - **High-frequency carrier modulation** of the PWM output, useful with
    transformer-insulated gate drivers
- **Capture module**
  - Rotating-machinery speed measurement; elapsed time between position-sensor pulses
  - Period and duty-cycle measurement of pulse trains
  - Decoding current/voltage amplitude from duty-cycle-encoded sensor signals
  - **Three capture channels, each with a 32-bit time-stamp register**
  - Edge-polarity selection and prescaling of input capture signals
  - Capture timer can sync with a PWM timer or external signals
- **Fault detection**
  - Programmable handling in **cycle-by-cycle** and **one-shot** modes
  - A fault can force the PWM output high or low
- ETM support

### 8.11 Remote Control peripheral (RMT)

- **Four channels** for sending and receiving infrared remote-control signals — **2 TX + 2 RX, fixed direction** (not flexible as on the original ESP32), 48 symbols each
- Independent TX and RX per channel
- Modes: **Normal TX/RX, Wrap TX/RX, Continuous TX**
- **Modulation on TX pulses, demodulation on RX pulses**
- RX filtering
- Simultaneous transmission on multiple channels
- Clock divider counter, state machine and receiver per RX channel
- Default RAM-block allocation by channel number
- RAM holds **16-bit entries with "level" and "period" fields**
- **No DMA.** Refill is interrupt-driven, so radio activity can stretch a WS2812 bit past its timing window — the symptom is an intermittently wrong-coloured LED under network load. Only the S3 and P4 have RMT DMA

### 8.12 Parallel IO controller (PARLIO)

Parallel-bus data transfer between external devices and internal memory through GDMA.

- **1/2/4/8/16-bit configurable data bus width**
- **Half-duplex at 16-bit**, **full-duplex at 8-bit**
- Bit reordering in 1/2/4-bit modes
- RX unit supports **15 receive modes** in three categories: **Level Enable**,
  **Pulse Enable**, **Software Enable**
- TX unit can generate a **valid signal aligned with TX**

### 8.13 SAR ADC

- **12-bit** sampling resolution
- Analog voltage sampling from up to **seven pins** (GPIO0–GPIO6 = ADC1_CH0–CH6)
- Attenuation of input signals for voltage conversion
- Software-triggered one-shot sampling
- **Timer-triggered multi-channel scanning**
- **DMA continuous conversion**
- **Two filters** with configurable filter coefficients
- **Threshold monitoring** that can trigger an interrupt
- ETM support

### 8.14 Temperature sensor

- Measurement range **–40 °C … 125 °C**
- Software triggering — data can be read continuously once triggered
- Hardware automatic triggering and temperature monitoring
- Configurable temperature offset for accuracy in a given environment
- Adjustable measurement range
- Two automatic monitoring wake-up modes: **absolute value** and **incremental value**
- ETM support

---

## 9. Wireless communication

### 9.1 Radio

Blocks: 2.4 GHz receiver, 2.4 GHz transmitter, bias and regulators, balun and
transmit-receive switch, clock generator.

**2.4 GHz receiver** — demodulates the RF signal to quadrature baseband and
converts to digital with two high-resolution, high-speed ADCs. Integrates RF
filters, **Automatic Gain Control (AGC)**, DC-offset cancellation and baseband
filters to adapt to varying channel conditions.

**2.4 GHz transmitter** — modulates quadrature baseband to 2.4 GHz RF and drives the
antenna via a high-power CMOS power amplifier; digital calibration improves PA
linearity. Built-in calibrations cancel radio imperfections: **carrier leakage,
I/Q amplitude/phase matching, baseband nonlinearity, RF nonlinearity, antenna
matching** — reducing cost, time and test equipment.

**Clock generator** — produces quadrature 2.4 GHz clocks for RX and TX. All
components (inductors, varactors, filters, regulators, dividers) are on-chip, with
built-in calibration and self-test; quadrature phases and phase noise are optimised
by patented calibration algorithms.

### 9.2 Wi-Fi radio and baseband

- Compliant with **IEEE 802.11 b/g/n/ax**, 1T1R in 2.4 GHz
- **802.11ax:** 20 MHz-only non-AP mode; MCS0–MCS9; uplink and downlink OFDMA;
  downlink MU-MIMO; longer OFDM symbol with **0.8 / 1.6 / 3.2 µs guard interval**;
  **DCM up to 16-QAM**; single-user/multi-user beamformee; CQI; **RX STBC**
  (single spatial stream)
- **802.11 b/g/n:** MCS0–MCS7 at 20 and 40 MHz; MCS32; up to **150 Mbps**;
  **0.4 µs guard interval**
- Adjustable transmit power
- **Antenna diversity** with an external RF switch controlled by one or more GPIOs,
  to select the best antenna and minimise channel imperfections

### 9.3 Wi-Fi MAC

Full 802.11 b/g/n/ax MAC, supporting BSS STA and SoftAP under DCF. Power management
is automatic with minimal host interaction.

Automatic low-level protocol functions:

- **4 virtual Wi-Fi interfaces**
- Infrastructure BSS in Station, SoftAP, Station+SoftAP and promiscuous modes
- RTS protection, CTS protection, Immediate Block ACK
- Fragmentation and defragmentation
- TX/RX A-MPDU, TX/RX A-MSDU
- Transmit opportunity (TXOP), WMM
- Security: **GCMP, CCMP, TKIP, WAPI, WEP, BIP, WPA2-PSK / WPA2-Enterprise,
  WPA3-PSK / WPA3-Enterprise**
- Automatic beacon monitoring (hardware TSF)
- **802.11mc FTM** — ⚠︎ *not supported in some chip revisions, see the Errata*

802.11ax specifics: TWT requester · multiple BSSIDs · triggered response scheduling ·
uplink power headroom · operating mode (control) · buffer status report ·
MU-RTS / MU-BAR / M-BA frames · intra-PPDU power saving · two NAVs · BSS coloring ·
spatial reuse · TXOP duration RTS threshold · **UL-OFDMA random access (UORA)**.

**Networking:** Espressif provides libraries for TCP/IP, ESP-WIFI-MESH and other
protocols over Wi-Fi. **TLS 1.0, 1.1 and 1.2** are supported.

### 9.4 Bluetooth LE

Hardware link controller + RF/modem block + software protocol stack, supporting the
core features of **Bluetooth 5** and **Bluetooth mesh**.

**PHY:** 1 Mbps · 2 Mbps · Coded PHY for longer range (125 Kbps and 500 Kbps) ·
hardware listen-before-talk (LBT).

**Link controller:** LE Advertising Extensions · multiple advertising sets ·
simultaneous advertising and scanning · multiple connections in simultaneous
central and peripheral roles · **Adaptive Frequency Hopping (AFH)** and channel
assessment · LE Channel Selection Algorithm #2 · **LE Power Control** · connection
parameter update · high-duty-cycle non-connectable advertising · **LE privacy 1.2** ·
LE Data Packet Length Extension · Link Layer Extended Scanner Filter policies ·
low-duty-cycle directed advertising · link layer encryption · LE Ping.

### 9.5 802.15.4

Integrated PHY and MAC, supporting software stacks including **Thread, Zigbee,
Matter, HomeKit, MQTT**.

**PHY:** O-QPSK in 2.4 GHz · **250 Kbps** · RSSI and LQI.

**MAC** (most key features of IEEE Std 802.15.4-2015): **CSMA/CA** · active scan and
energy detect · **hardware frame filter** · **hardware auto-acknowledge** ·
**hardware auto frame pending** · **coordinated sampled listening (CSL)**.

---

## 10. Electrical characteristics

### 10.1 Absolute maximum ratings (Table 5-1)

> Stresses above these may cause **permanent damage**. These are stress ratings only;
> normal operation at these conditions is not implied. Prolonged exposure may affect
> reliability.

| Parameter | Description | Min | Max | Unit |
| --- | --- | --- | --- | --- |
| Input power pins | Allowed input voltage | –0.3 | **3.6** | V |
| `I_output` | Cumulative IO output current | — | **1000** | mA |
| `T_STORE` | Storage temperature | –40 | 150 | °C |

*The product proved fully functional after all IO pins were pulled high while
connected to ground for 24 consecutive hours at 25 °C ambient.*

### 10.2 Recommended operating conditions (Table 5-2)

| Parameter | Description | Min | Typ | Max | Unit |
| --- | --- | --- | --- | --- | --- |
| `VDDA1`, `VDDA2`, `VDDA3P3` | Recommended input voltage | 3.0 | **3.3** | 3.6 | V |
| `VDDPST1` | Recommended input voltage | 3.0 | **3.3** | 3.6 | V |
| `VDD_SPI` (as input) | — | 3.0 | 3.3 | 3.6 | V |
| `VDDPST2` | Recommended input voltage | 3.0 | **3.3** | 3.6 | V |
| `I_VDD` | Cumulative input current | **0.5** | — | — | A |
| `T_A` | Ambient temperature | –40 | — | **105** | °C |

- If `VDDPST2` powers `VDD_SPI`, account for the voltage drop across `R_SPI`.
- ⚠︎ **When writing eFuses, `VDDPST2` must not exceed 3.3 V** — the eFuse burning
  circuits are sensitive to higher voltages.

### 10.3 VDD_SPI output characteristics (Table 5-3)

| Parameter | Description | Typ | Unit |
| --- | --- | --- | --- |
| `R_SPI` | `VDD_SPI` powered by `VDDPST2` via `R_SPI` for 3.3 V flash | **3** | Ω |

`VDDPST2` must exceed `VDD_flash_min + I_flash_max × R_SPI`.

### 10.4 DC characteristics (3.3 V, 25 °C) — Table 5-4

| Parameter | Description | Min | Typ | Max | Unit |
| --- | --- | --- | --- | --- | --- |
| `C_IN` | Pin capacitance | — | 2 | — | pF |
| `V_IH` | High-level input voltage | 0.75 × VDD | — | VDD + 0.3 | V |
| `V_IL` | Low-level input voltage | –0.3 | — | 0.25 × VDD | V |
| `I_IH` | High-level input current | — | — | 50 | nA |
| `I_IL` | Low-level input current | — | — | 50 | nA |
| `V_OH` | High-level output voltage | 0.8 × VDD | — | — | V |
| `V_OL` | Low-level output voltage | — | — | 0.1 × VDD | V |
| `I_OH` | High-level source current (VDD = 3.3 V, V_OH ≥ 2.64 V, `PAD_DRIVER = 3`) | — | **40** | — | mA |
| `I_OL` | Low-level sink current (VDD = 3.3 V, V_OL = 0.495 V, `PAD_DRIVER = 3`) | — | **28** | — | mA |
| `R_PU` | Internal weak pull-up resistor | — | **45** | — | kΩ |
| `R_PD` | Internal weak pull-down resistor | — | **45** | — | kΩ |
| `V_IH_nRST` | Chip reset release voltage (on `CHIP_PU`) | 0.75 × VDD | — | VDD + 0.3 | V |
| `V_IL_nRST` | Chip reset voltage (on `CHIP_PU`) | –0.3 | — | 0.25 × VDD | V |

`VDD` = the voltage of the respective power domain's power pin. `V_OH`/`V_OL` are
measured with a high-impedance load.

### 10.5 ADC characteristics

Measured with an external **100 nF capacitor** on the ADC, DC input signals, 25 °C
ambient, **Wi-Fi disabled**.

#### Table 5-5

| Symbol | Min | Max | Unit |
| --- | --- | --- | --- |
| DNL (differential nonlinearity) | –8 | 12 | LSB |
| INL (integral nonlinearity) | –10 | 10 | LSB |
| Sampling rate | — | **100 kSPS** | |

*For better DNL, sample multiple times and filter, or average.*

#### Table 5-6 — Calibrated ADC results (hardware + software calibration)

| Attenuation | Effective measurement range | Total error min | max | Unit |
| --- | --- | --- | --- | --- |
| ATTEN0 | 0 – 1000 mV | –12 | +12 | mV |
| ATTEN1 | 0 – 1300 mV | –12 | +12 | mV |
| ATTEN2 | 0 – 1900 mV | –23 | +23 | mV |
| ATTEN3 | 0 – 3300 mV | –40 | +40 | mV |

> These ranges and accuracies apply to chips manufactured on/after date code
> **212023** on shielding cases, or assembled on/after **D/C 1** and **D/C 22321**
> on barcode labels. For earlier chips, ask Espressif sales for the actual range and
> accuracy by batch.

### 10.6 Current consumption

Measured with a **3.3 V** supply at **25 °C** ambient. TX current is rated at 100 %
duty cycle; RX current is rated with peripherals disabled and the CPU idle.

#### Table 5-7 — Wi-Fi (2.4 GHz), Active mode

| Direction | Condition | Peak (mA) |
| --- | --- | --- |
| TX | 802.11b, 1 Mbps, DSSS @ 21.0 dBm | **354** |
| TX | 802.11g, 54 Mbps, OFDM @ 19.5 dBm | 300 |
| TX | 802.11n, HT20, MCS7 @ 18.5 dBm | 280 |
| TX | 802.11n, HT40, MCS7 @ 18.0 dBm | 268 |
| TX | 802.11ax, MCS9 @ 16.5 dBm | 252 |
| RX | 802.11b/g/n, HT20 | 78 |
| RX | 802.11n, HT40 | 82 |
| RX | 802.11ax, HE20 | 78 |

#### Table 5-8 — Bluetooth LE, Active mode

| Direction | Condition | Peak (mA) |
| --- | --- | --- |
| TX | Bluetooth LE @ 20.0 dBm | **315** |
| TX | Bluetooth LE @ 9.0 dBm | 190 |
| TX | Bluetooth LE @ 0 dBm | 130 |
| TX | Bluetooth LE @ –15.0 dBm | 94 |
| RX | Bluetooth LE | 71 |

#### Table 5-9 — 802.15.4, Active mode

| Direction | Condition | Peak (mA) |
| --- | --- | --- |
| TX | 802.15.4 @ 20.0 dBm | **305** |
| TX | 802.15.4 @ 12.0 dBm | 187 |
| TX | 802.15.4 @ 0 dBm | 119 |
| TX | 802.15.4 @ –15.0 dBm | 92 |
| RX | 802.15.4 | 74 |

#### Table 5-10 — Modem-sleep mode

| CPU frequency | CPU state | All peripheral clocks **disabled** (mA) | All peripheral clocks **enabled** (mA) |
| --- | --- | --- | --- |
| 160 MHz | running | **27** | 38 |
| 160 MHz | idle | 17 | 28 |
| 80 MHz | running | 19 | 30 |
| 80 MHz | idle | **14** | 25 |

In Modem-sleep, Wi-Fi is clock-gated. Consumption may be **higher when accessing
flash**, and varies with which peripherals are enabled.

#### Table 5-11 — Low-power modes

| Mode | Description | Typ (µA) |
| --- | --- | --- |
| **Light-sleep** | CPU and wireless modules powered down, peripheral clocks disabled, all GPIOs high-impedance | **180** |
| **Light-sleep** | CPU, wireless modules **and peripherals** powered down, all GPIOs high-impedance | **35** |
| **Deep-sleep** | RTC timer and LP memory powered on | **7** |
| **Power off** | `CHIP_PU` low, chip powered off | **1** |

### 10.7 Memory specifications

Sourced from the memory vendor's datasheet; guaranteed by design and/or
characterisation but **not fully tested in production**. Devices ship with the
memory erased.

#### Table 5-12 — Flash specifications

| Parameter | Description | Min | Typ | Max | Unit |
| --- | --- | --- | --- | --- | --- |
| `V_CC` | Supply voltage (1.8 V part) | 1.65 | 1.80 | 2.00 | V |
| `V_CC` | Supply voltage (3.3 V part) | 2.7 | 3.3 | 3.6 | V |
| `F_C` | Maximum clock frequency | **80** | — | — | MHz |
| — | Program/erase cycles | **100,000** | — | — | cycles |
| `T_RET` | Data retention time | **20** | — | — | years |
| `T_PP` | Page program time | — | 0.8 | 5 | ms |
| `T_SE` | Sector erase time (4 KB) | — | 70 | 500 | ms |
| `T_BE1` | Block erase time (32 KB) | — | 0.2 | 2 | s |
| `T_BE2` | Block erase time (64 KB) | — | 0.3 | 3 | s |
| `T_CE` | Chip erase, 16 Mb | — | 7 | 20 | s |
| `T_CE` | Chip erase, **32 Mb (= 4 MB, the FH4 part)** | — | **20** | **60** | s |
| `T_CE` | Chip erase, 64 Mb (= 8 MB, FH8) | — | 25 | 100 | s |
| `T_CE` | Chip erase, 128 Mb | — | 60 | 200 | s |
| `T_CE` | Chip erase, 256 Mb | — | 70 | 300 | s |

### 10.8 Reliability qualifications (Table 5-13)

| Test item | Test conditions | Standard |
| --- | --- | --- |
| **HTOL** (High Temperature Operating Life) | 125 °C, 1000 hours | JESD22-A108 |
| **ESD — HBM** (Human Body Model) | ± 2000 V | JS-001 |
| **ESD — CDM** (Charged Device Model) | ± 1000 V | JS-002 |
| **Latch-up** | Current trigger ± 200 mA; voltage trigger 1.5 × VDDmax | JESD78 |
| **Preconditioning** | Bake 24 h @ 125 °C; moisture soak level 3 (192 h @ 30 °C, 60 % RH); IR reflow solder 260 +0 °C, 20 s, three times | J-STD-020, JESD47, JESD22-A113 |
| **TCT** (Temperature Cycling) | –65 °C / 150 °C, 500 cycles | JESD22-A104 |
| **uHAST** (unbiased Highly Accelerated Stress) | 130 °C, 85 % RH, 96 hours | JESD22-A118 |
| **HTSL** (High Temperature Storage Life) | 150 °C, 1000 hours | JESD22-A103 |
| **LTSL** (Low Temperature Storage Life) | –40 °C, 1000 hours | JESD22-A119 |

*JEP155 states 500 V HBM allows safe manufacturing with a standard ESD control
process; JEP157 states 250 V CDM does.*

---

## 11. RF characteristics

RF data is measured **at the antenna port** where the RF cable connects, **including
front-end loss**; the front-end circuit is a **0 Ω resistor**. Devices should operate
in the centre-frequency range allocated by regional regulators; the target centre
frequency range and transmit power are software-configurable (see *ESP RF Test Tool
and Test Guide*). Unless stated otherwise, tests use **3.3 V (±5 %) at 25 °C**.

### 11.1 Wi-Fi radio

| Name | Description |
| --- | --- |
| Centre frequency range of operating channel | **2412 – 2484 MHz** |
| Wi-Fi wireless standard | IEEE 802.11 b/g/n/ax |

#### Table 6-2 — TX power meeting spectral mask and EVM

| Rate | Typ (dBm) |
| --- | --- |
| 802.11b, 1 Mbps, DSSS | **21.0** |
| 802.11b, 11 Mbps, CCK | **21.0** |
| 802.11g, 6 Mbps, OFDM | 20.5 |
| 802.11g, 54 Mbps, OFDM | 19.5 |
| 802.11n, HT20, MCS0 | 19.5 |
| 802.11n, HT20, MCS7 | 18.5 |
| 802.11n, HT40, MCS0 | 19.0 |
| 802.11n, HT40, MCS7 | 18.0 |
| 802.11ax, HE20, MCS0 | 19.5 |
| 802.11ax, HE20, MCS9 | 16.5 |

#### Table 6-3 — TX EVM

Measured at the corresponding typical TX power above.

| Rate | Typ (dB) | Limit (dB) |
| --- | --- | --- |
| 802.11b, 1 Mbps, DSSS | –25.5 | –10.0 |
| 802.11b, 11 Mbps, CCK | –25.5 | –10.0 |
| 802.11g, 6 Mbps, OFDM | –26.5 | –5.0 |
| 802.11g, 54 Mbps, OFDM | –29.0 | –25.0 |
| 802.11n, HT20, MCS0 | –29.0 | –5.0 |
| 802.11n, HT20, MCS7 | –30.0 | –27.0 |
| 802.11n, HT40, MCS0 | –28.5 | –5.0 |
| 802.11n, HT40, MCS7 | –29.5 | –27.0 |
| 802.11ax, HE20, MCS0 | –29.0 | –5.0 |
| 802.11ax, HE20, MCS9 | –34.0 | –32.0 |

#### Table 6-4 — RX sensitivity

PER limit: **8 % for 802.11b**, **10 % for 802.11 g/n/ax**.

| Rate | Typ (dBm) | | Rate | Typ (dBm) |
| --- | --- | --- | --- | --- |
| 802.11b, 1 Mbps, DSSS | **–99.2** | | 802.11n, HT20, MCS4 | –82.8 |
| 802.11b, 2 Mbps, DSSS | –96.8 | | 802.11n, HT20, MCS5 | –78.8 |
| 802.11b, 5.5 Mbps, CCK | –93.8 | | 802.11n, HT20, MCS6 | –77.2 |
| 802.11b, 11 Mbps, CCK | –90.0 | | 802.11n, HT20, MCS7 | –75.6 |
| 802.11g, 6 Mbps, OFDM | –94.0 | | 802.11n, HT40, MCS0 | –91.0 |
| 802.11g, 9 Mbps, OFDM | –93.2 | | 802.11n, HT40, MCS1 | –90.0 |
| 802.11g, 12 Mbps, OFDM | –92.6 | | 802.11n, HT40, MCS2 | –87.4 |
| 802.11g, 18 Mbps, OFDM | –90.0 | | 802.11n, HT40, MCS3 | –83.8 |
| 802.11g, 24 Mbps, OFDM | –86.8 | | 802.11n, HT40, MCS4 | –80.8 |
| 802.11g, 36 Mbps, OFDM | –83.2 | | 802.11n, HT40, MCS5 | –76.6 |
| 802.11g, 48 Mbps, OFDM | –79.0 | | 802.11n, HT40, MCS6 | –75.0 |
| 802.11g, 54 Mbps, OFDM | –77.6 | | 802.11n, HT40, MCS7 | –73.4 |
| 802.11n, HT20, MCS0 | –93.6 | | 802.11ax, HE20, MCS0 | **–93.8** |
| 802.11n, HT20, MCS1 | –92.4 | | 802.11ax, HE20, MCS1 | –91.2 |
| 802.11n, HT20, MCS2 | –89.6 | | 802.11ax, HE20, MCS2 | –88.4 |
| 802.11n, HT20, MCS3 | –86.2 | | 802.11ax, HE20, MCS3 | –85.6 |
| | | | 802.11ax, HE20, MCS4 | –82.2 |
| | | | 802.11ax, HE20, MCS5 | –78.4 |
| | | | 802.11ax, HE20, MCS6 | –76.6 |
| | | | 802.11ax, HE20, MCS7 | –74.8 |
| | | | 802.11ax, HE20, MCS8 | –71.0 |
| | | | 802.11ax, HE20, MCS9 | –69.0 |

#### Table 6-5 — Maximum RX level

| Rate | Typ (dBm) |
| --- | --- |
| 802.11b, 1 Mbps, DSSS | 5 |
| 802.11b, 11 Mbps, CCK | 5 |
| 802.11g, 6 Mbps, OFDM | 5 |
| 802.11g, 54 Mbps, OFDM | 0 |
| 802.11n, HT20, MCS0 | 5 |
| 802.11n, HT20, MCS7 | 0 |
| 802.11n, HT40, MCS0 | 5 |
| 802.11n, HT40, MCS7 | 0 |
| 802.11ax, HE20, MCS0 | 5 |
| 802.11ax, HE20, MCS9 | 0 |

#### Table 6-6 — RX adjacent channel rejection

| Rate | Typ (dB) |
| --- | --- |
| 802.11b, 1 Mbps, DSSS | 38 |
| 802.11b, 11 Mbps, CCK | 38 |
| 802.11g, 6 Mbps, OFDM | 31 |
| 802.11g, 54 Mbps, OFDM | 20 |
| 802.11n, HT20, MCS0 | 31 |
| 802.11n, HT20, MCS7 | 16 |
| 802.11n, HT40, MCS0 | 28 |
| 802.11n, HT40, MCS7 | 10 |
| 802.11ax, HE20, MCS0 | 25 |
| 802.11ax, HE20, MCS9 | 2 |

### 11.2 Bluetooth 5 (LE) radio

| Name | Description |
| --- | --- |
| Centre frequency range of operating channel | **2402 – 2480 MHz** |
| RF transmit power range | **–15.0 … 20.0 dBm** |

#### Transmitter characteristics

| Parameter | 1 Mbps | 2 Mbps | 125 Kbps | 500 Kbps |
| --- | --- | --- | --- | --- |
| Max \|f_n\| | 1.3 kHz | 2.2 kHz | 0.7 kHz | 0.5 kHz |
| Max \|f_0 − f_n\| | 1.5 kHz | 1.1 kHz | 0.3 kHz | 0.3 kHz |
| Max \|f_n − f_(n−5)\| | 0.9 kHz | 1.1 kHz | — | — |
| Max \|f_n − f_(n−3)\| | — | — | 0.4 kHz | 0.4 kHz |
| \|f_1 − f_0\| | 0.6 kHz | 0.5 kHz | — | — |
| \|f_0 − f_3\| | — | — | 0.1 kHz | 0.1 kHz |
| ΔF1avg | 249.9 kHz | 499.4 kHz | 250.0 kHz | — |
| ΔF2avg | — | — | — | 230.7 kHz |
| Min ΔF2max (≥ 99.9 %) | 212.1 kHz | 443.5 kHz | — | 217.6 kHz |
| Min ΔF1max (≥ 99.9 %) | — | — | 238.0 kHz | — |
| ΔF2avg / ΔF1avg | 0.88 | 0.95 | — | — |
| In-band emission @ ±2 MHz | –29 dBm | — | –29 dBm | –28 dBm |
| In-band emission @ ±3 MHz | –36 dBm | — | –36 dBm | –36 dBm |
| In-band emission > ±3 MHz | –39 dBm | — | –39 dBm | –39 dBm |
| In-band emission @ ±4 MHz | — | –40 dBm | — | — |
| In-band emission @ ±5 MHz | — | –41 dBm | — | — |
| In-band emission > ±5 MHz | — | –42 dBm | — | — |

#### Receiver characteristics

| Parameter | 1 Mbps | 2 Mbps | 125 Kbps | 500 Kbps |
| --- | --- | --- | --- | --- |
| **Sensitivity @ 30.8 % PER** | **–98.5 dBm** | **–95.5 dBm** | **–106.0 dBm** | **–102.0 dBm** |
| Maximum received signal @ 30.8 % PER | 8 dBm | 8 dBm | 8 dBm | 8 dBm |
| Co-channel C/I, F = F0 | 7 dB | 8 dB | 2 dB | 4 dB |
| Image frequency | –26 dB | –23 dB | –31 dB | –30 dB |

Adjacent-channel C/I (1 Mbps): +1 MHz = 4 dB, −1 MHz = 3 dB, +2 MHz = −21 dB,
−2 MHz = −22 dB, +3 MHz = −28 dB, −3 MHz = −36 dB, ≥ +4 MHz = −27 dB,
≤ −4 MHz = −36 dB.
(2 Mbps): +2 = 3, −2 = 2, +4 = −23, −4 = −25, +6 = −31, −6 = −35, ≥+8 = −36,
≤−8 = −36 dB.
(125 Kbps): +1 = −1, −1 = −3, +2 = −31, −2 = −27, +3 = −33, −3 = −42, ≥+4 = −31,
≤−4 = −48 dB.
(500 Kbps): +1 = 1, −1 = −1, +2 = −23, −2 = −24, +3 = −33, −3 = −41, ≥+4 = −31,
≤−4 = −41 dB.

Out-of-band blocking (1 Mbps / 2 Mbps): 30 MHz–2000 MHz = −16 / −18 dBm;
2003–2399 MHz = −24 / −28 dBm; 2484–2997 MHz = −16 / −16 dBm;
3000 MHz–12.75 GHz = −1 / −1 dBm. Intermodulation: −27 / −29 dBm.

### 11.3 802.15.4 radio

| Name | Description |
| --- | --- |
| Centre frequency range of operating channel | **2405 – 2480 MHz** |

*Zigbee in the 2.4 GHz range uses 16 channels at 5 MHz spacing, channel 11 to
channel 26.*

#### Transmitter, 250 Kbps (Table 6-17)

| Parameter | Min | Typ | Max | Unit |
| --- | --- | --- | --- | --- |
| RF transmit power range | –15.0 | — | **20.0** | dBm |
| EVM | — | **13.0 %** | — | — |

#### Receiver, 250 Kbps (Table 6-18)

| Parameter | Typ |
| --- | --- |
| **Sensitivity @ 1 % PER** | **–104.0 dBm** |
| Maximum received signal @ 1 % PER | 8 dBm |
| Adjacent channel rejection, F = F0 + 5 MHz | 27 dB |
| Adjacent channel rejection, F = F0 − 5 MHz | 32 dB |
| Alternate channel rejection, F = F0 + 10 MHz | 47 dB |
| Alternate channel rejection, F = F0 − 10 MHz | 50 dB |

---

## 12. Packaging and reliability

- Tape, reel and chip marking details: see *ESP32-C6 Chip Packaging Information*.
- Pins are numbered **anti-clockwise from pin 1 in the top view**.
- A recommended **land pattern source file (`.asc`)** is available for download and
  can be imported into PADS, Altium Designer and similar tools.
- Packages: **QFN40 (5 × 5 mm)** and **QFN32 (5 × 5 mm)**.

Reliability qualification data is in [§10.8](#108-reliability-qualifications-table-5-13).

---

## 13. Consolidated pin overview

The datasheet's Tables 7-1 / 7-2 merge every function column into one row per pin.
Reproduced here for **QFN32**, the package on this board:

| Pin | Name | Type | Power | At reset | After reset | Analog F0 | Analog F1 | LP F0 | LP F1 | IO MUX F0 | F1 | F2 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | ANT | Analog | | | | | | | | | | |
| 2 | VDDA3P3 | Power | | | | | | | | | | |
| 3 | VDDA3P3 | Power | | | | | | | | | | |
| 4 | CHIP_PU | Analog | | | | | | | | | | |
| 5 | VDDPST1 | Power | | | | | | | | | | |
| 6 | XTAL_32K_P | IO | VDDPST1 | | | XTAL_32K_P | ADC1_CH0 | LP_GPIO0 | LP_UART_DTRN | GPIO0 | GPIO0 | |
| 7 | XTAL_32K_N | IO | VDDPST1 | | | XTAL_32K_N | ADC1_CH1 | LP_GPIO1 | LP_UART_DSRN | GPIO1 | GPIO1 | |
| 8 | GPIO2 | IO | VDDPST1 | IE | IE | | ADC1_CH2 | LP_GPIO2 | LP_UART_RTSN | GPIO2 | GPIO2 | FSPIQ |
| 9 | GPIO3 | IO | VDDPST1 | IE | IE | | ADC1_CH3 | LP_GPIO3 | LP_UART_CTSN | GPIO3 | GPIO3 | |
| 10 | MTMS | IO | VDDPST1 | IE | IE | | ADC1_CH4 | LP_GPIO4 | LP_UART_RXD | MTMS | GPIO4 | FSPIHD |
| 11 | MTDI | IO | VDDPST1 | IE | IE | | ADC1_CH5 | LP_GPIO5 | LP_UART_TXD | MTDI | GPIO5 | FSPIWP |
| 12 | MTCK | IO | VDDPST1 | IE, WPU | | | ADC1_CH6 | LP_GPIO6 | LP_I2C_SDA | MTCK | GPIO6 | FSPICLK |
| 13 | MTDO | IO | VDDPST1 | IE | | | | LP_GPIO7 | LP_I2C_SCL | MTDO | GPIO7 | FSPID |
| 14 | GPIO8 | IO | VDDPST2 | IE | IE | | | | | GPIO8 | GPIO8 | |
| 15 | GPIO9 | IO | VDDPST2 | IE, WPU | IE, WPU | | | | | GPIO9 | GPIO9 | |
| 16 | GPIO12 | IO | VDDPST2 | IE | | USB_D− | | | | GPIO12 | GPIO12 | |
| 17 | GPIO13 | IO | VDDPST2 | IE, WPU | | USB_D+ | | | | GPIO13 | GPIO13 | |
| 18 | GPIO14 | IO | VDDPST2 | IE | | | | | | GPIO14 | GPIO14 | |
| 19 | GPIO15 | IO | VDDPST2 | IE | IE | | | | | GPIO15 | GPIO15 | |
| 20 | VDDPST2 | Power | | | | | | | | | | |
| 21 | U0TXD | IO | VDDPST2 | WPU | | | | | | U0TXD | GPIO16 | FSPICS0 |
| 22 | U0RXD | IO | VDDPST2 | IE, WPU | | | | | | U0RXD | GPIO17 | FSPICS1 |
| 23 | SDIO_CMD | IO | VDDPST2 | WPU | IE | | | | | SDIO_CMD | GPIO18 | FSPICS2 |
| 24 | SDIO_CLK | IO | VDDPST2 | WPU | IE | | | | | SDIO_CLK | GPIO19 | FSPICS3 |
| 25 | SDIO_DATA0 | IO | VDDPST2 | WPU | IE | | | | | SDIO_DATA0 | GPIO20 | FSPICS4 |
| 26 | SDIO_DATA1 | IO | VDDPST2 | WPU | IE | | | | | SDIO_DATA1 | GPIO21 | FSPICS5 |
| 27 | SDIO_DATA2 | IO | VDDPST2 | WPU | IE | | | | | SDIO_DATA2 | GPIO22 | |
| 28 | SDIO_DATA3 | IO | VDDPST2 | WPU | IE | | | | | SDIO_DATA3 | GPIO23 | |
| 29 | VDDA1 | Power | | | | | | | | | | |
| 30 | XTAL_N | Analog | | | | | | | | | | |
| 31 | XTAL_P | Analog | | | | | | | | | | |
| 32 | VDDA2 | Power | | | | | | | | | | |
| 33 | GND | Power | | | | | | | | | | |

---

## 14. Related documentation

**Espressif documentation**

- **ESP32-C6 Technical Reference Manual** — detailed information on using the
  ESP32-C6 memory and peripherals. *(Referenced constantly by the datasheet; it is
  the document to read for register-level detail.)*
- **ESP32-C6 Hardware Design Guidelines** — integrating the ESP32-C6 into a product.
- **ESP32-C6 Series SoC Errata** — known errors per chip revision.
- Certificates: <https://espressif.com/en/support/documents/certificates>
- PCNs: <https://espressif.com/en/support/documents/pcns?keys=ESP32-C6>
- Documentation updates & subscription: <https://espressif.com/en/support/download/documents>

**Developer zone**

- ESP-IDF Programming Guide for ESP32-C6
- ESP-IDF and other frameworks: <https://github.com/espressif>
- ESP32 BBS Forum: <https://esp32.com/>
- ESP-FAQ: <https://espressif.com/projects/esp-faq/en/latest/index.html>
- The ESP Journal: <https://blog.espressif.com/>
- SDKs, demos, apps, tools, AT firmware: <https://espressif.com/en/support/download/sdks-demos>

**Products**

- ESP32-C6 SoCs: <https://espressif.com/en/products/socs?id=ESP32-C6>
- ESP32-C6 modules: <https://espressif.com/en/products/modules?id=ESP32-C6>
- ESP32-C6 devkits: <https://espressif.com/en/products/devkits?id=ESP32-C6>
- ESP Product Selector: <https://products.espressif.com/#/product-selector?language=en>

### Datasheet versioning scheme

| Version | Status | Watermark | Meaning |
| --- | --- | --- | --- |
| v0.1 – <v0.5 | Draft | *Confidential* | Product in design stage; specs may change without notice |
| v0.5 – <v1.0 | Preliminary release | *Preliminary* | Product in verification stage; specs may change before mass production, changes documented in the Revision History |
| **v1.0 and higher** | **Official release** | — | Publicly released for mass production; specs finalised, major changes announced via **PCN** |
| any | — | *NRND* | Not recommended for new design; updated less frequently |
| any | — | *EOL* | End of life; no longer maintained |

---

## 15. Datasheet revision history

| Date | Version | Release notes |
| --- | --- | --- |
| **2026-03-31** | **v1.5** | Renamed and clarified the RSA digital signature peripheral (RSA_DS) in the Product Overview, §4.1.4.6 and related HMAC/SHA text · simplified the functional block diagram to show default configurations only · updated the descriptions of predefined power modes in §4.1.3.7 |
| 2025-11-20 | v1.4 | "Ordering Code" → "Part Number" in Table 1-1 · added §1.3 Chip Revision · added a note about USB pin swapping to Tables 2-10 / 2-11 · added §5.7 Memory Specifications · deleted TSLP-related information from the QFN40 package diagram · added the Datasheet Versioning appendix · other minor updates |
| 2025-03-21 | v1.3 | Updated the CPU CoreMark® score · added §2.3.5 Peripheral Pin Assignment · per AR2024-011, removed "32 kHz internal slow RC oscillator" from §4.1.3.3 Clock |
| 2024-08-23 | v1.2 | Added the ESP32-C6FH8 variant |

*(Earlier revisions are listed in the source PDF.)*
