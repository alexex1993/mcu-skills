# ESP32-S3 — the silicon

What is true of the chip regardless of which board it is on. Transcribed from the
*ESP32-S3 Series Datasheet v2.2* and the *ESP32-S3 Technical Reference Manual*; board
wiring is in `board-hardware.md`.

---

## 1. Core and memory

| | |
|---|---|
| CPU | Xtensa **LX7 dual-core**, up to 240 MHz, with vector (PIE) extensions for DSP/NN |
| ROM | 384 KB, mask, holds the first-stage bootloader — **cannot be bricked** |
| SRAM | **512 KB** on-chip, one flat pool for instructions and data |
| RTC FAST | 8 KB, retained in deep sleep, CPU-accessible |
| RTC SLOW | 8 KB, retained in deep sleep, ULP-accessible |
| External | up to 32 MB flash and 32 MB PSRAM through the cache/MMU |

Address map, the parts that matter:

| Range | Contents |
|---|---|
| `0x3C00_0000 – 0x3DFF_FFFF` | external memory, **data bus** — this is where PSRAM appears |
| `0x4200_0000 – 0x43FF_FFFF` | external memory, **instruction bus** — XIP flash |
| `0x3FC8_8000 – 0x3FCF_FFFF` | internal SRAM, data |
| `0x4037_0000 – 0x403D_FFFF` | internal SRAM, instruction |
| `0x6000_0000 – 0x600D_0FFF` | peripherals |
| `0x600F_E000 – 0x600F_FFFF` | RTC fast/slow memory |

Practical consequence: a PSRAM pointer is `0x3C…`, an internal-SRAM pointer is `0x3F…`.
Printing the pointer tells you which pool an allocation came from, which is faster than
instrumenting `heap_caps_*`.

---

## 2. GPIO inventory

**45 programmable GPIOs: 0-21 and 26-48. GPIO22, 23, 24 and 25 do not exist** — the
numbering skips them. Code that loops `for (int i = 0; i < 48; i++)` will fault.

| Group | Pins | Status |
|---|---|---|
| general | 0-21, 38-48 | usable, subject to the notes below |
| in-package flash bus | 26-32 (`SPICS1, SPIHD, SPIWP, SPICS0, SPICLK, SPIQ, SPID`) | **never** — the CPU is executing out of them |
| octal flash/PSRAM extension | 33-37 (`DQ4…DQ7, DQS/DM`) | unusable on any module with **octal** flash *or* PSRAM |
| USB | 19 (D-), 20 (D+) | default-owned by the USB Serial/JTAG controller |
| JTAG pads | 39 (MTCK), 40 (MTDO), 41 (MTDI), 42 (MTMS) | usable, at the cost of pad-JTAG |
| UART0 | 43 (TXD), 44 (RXD) | usable, at the cost of the console |

On an **R8** module (8 MB octal PSRAM) the datasheet footnote is explicit: *"pins IO35,
IO36, and IO37 are connected to the Octal SPI PSRAM and are not available for other
uses."* GPIO33 and GPIO34 are only claimed when the **flash** is octal too.

Drive strength defaults: GPIO17/18 = 10 mA, GPIO19/20 = 40 mA, everything else = 20 mA.

### Power-up glitches

**GPIO1-14, XTAL_32K_P and XTAL_32K_N emit a low-level glitch of ~60 µs at power-up**
(datasheet Table 2-2). Harmless for an LED, not harmless for a MOSFET gate, a relay
driver, a latching load or an active-low enable — add a pull to the safe state.

---

## 3. Strapping pins

Four pins, sampled by latches at Chip Reset and held until power-down. After the ~3 ms
hold time they are ordinary GPIOs; before it, they decide how the chip boots.

| Pin | Default | Controls |
|---|---|---|
| **GPIO0** | weak **pull-up** (1) | boot mode |
| **GPIO46** | weak **pull-down** (0) | boot mode + ROM message routing |
| **GPIO45** | weak **pull-down** (0) | VDD_SPI voltage |
| **GPIO3** | **floating, no internal pull** | JTAG signal source |

Boot mode:

| Mode | GPIO0 | GPIO46 |
|---|---|---|
| **SPI boot** (run your app) | 1 | any |
| Joint download boot | 0 | 0 |

Joint download boot covers USB-Serial-JTAG download, USB-OTG download and UART download
at once — which is why a single BOOT press works on either USB socket.

VDD_SPI:

| GPIO45 at reset | VDD_SPI |
|---|---|
| 0 (default) | **3.3 V** from VDD3P3_RTC |
| 1 | 1.8 V from the internal flash regulator |

On a 3.3 V module, GPIO45 high at reset makes the flash unreadable. The board looks dead
and is not.

JTAG source: GPIO3 high (or floating with default eFuses) → USB-Serial-JTAG owns JTAG;
GPIO3 low with `EFUSE_STRAP_JTAG_SEL` burnt → the MTDI/MTCK/MTMS/MTDO pads do. Because
GPIO3 has **no internal pull**, it must not be left in a high-impedance state by an
external circuit.

ROM messages print to **both** UART0 and the USB Serial/JTAG controller by default.

---

## 4. Analog

| | |
|---|---|
| ADC1 | 10 channels, **CH0-CH9 = GPIO1-GPIO10** |
| ADC2 | 10 channels, **CH0-CH9 = GPIO11-GPIO20** |
| Resolution | 12-bit, up to 100 kSPS |
| Linearity | DNL ±4 LSB, INL ±8 LSB — average or filter, always |
| DAC | **none.** The ESP32-S3 dropped the ESP32's two DAC channels |
| Touch | **TOUCH1-TOUCH14 = GPIO1-GPIO14** |

Attenuation and usable range, with calibration:

| Attenuation | Range | Total error |
|---|---|---|
| `ADC_ATTEN_DB_0` | 0 – 850 mV | ±5 mV |
| `ADC_ATTEN_DB_2_5` | 0 – 1100 mV | ±6 mV |
| `ADC_ATTEN_DB_6` | 0 – 1600 mV | ±10 mV |
| `ADC_ATTEN_DB_12` | 0 – 2900 mV | ±50 mV |

**ADC2 is arbitrated by the Wi-Fi PHY and loses.** With Wi-Fi running, ADC2 reads return
`ESP_ERR_TIMEOUT` (IDF) or garbage (Arduino's `analogRead`, which does not report it).
Any analog input in a connected design must be on ADC1 — GPIO1-GPIO10.

---

## 5. LCD_CAM — the DVP camera controller

One peripheral, two halves. The camera half:

- **8-bit to 16-bit parallel DVP input, PCLK up to 40 MHz.**
- Hardware conversion between RGB565, YUV422, YUV420 and YUV411.
- Feeds GDMA directly, so frames land in PSRAM without CPU involvement.
- The **XCLK the sensor needs is not generated by LCD_CAM** — the esp32-camera driver
  synthesises it with LEDC. That is why the camera occupies an LEDC channel and timer,
  and why `analogWrite()` can break it.

The LCD half drives 8-16 bit parallel RGB, i8080 and MOTO6800 panels at up to 40 MHz.
Both halves exist on this chip but only one set of pins; a board with a camera has no
parallel LCD.

---

## 6. Buses and peripherals

| Peripheral | Count | Notes |
|---|---|---|
| UART | 3 (UART0/1/2) | UART0 default GPIO43/44; any pins via GPIO matrix |
| I2C | 2 | fully matrix-routed, no IO_MUX advantage, up to 800 kHz |
| SPI | SPI2 (FSPI) + SPI3 | SPI2's IO_MUX pads are GPIO9-14 — all camera pins on this board |
| I2S | 2 | PDM and TDM, used for microphones |
| RMT | 4 TX + 4 RX | the WS2812 route; `neopixelWrite()` uses it |
| LEDC | 8 channels, 4 timers | **channel 0 / timer 0 is the camera's XCLK** |
| PCNT | 4 units | |
| TWAI | 1 | CAN 2.0 |
| USB | OTG 1.1 FS **and** Serial/JTAG, on GPIO19/20 | one at a time |
| SDMMC | 1 host, 2 slots | fully matrix-routed on the S3 — any pins, 1-bit or 4-bit |
| Timers | 4 × 54-bit + 3 watchdogs | |

SPI note specific to the S3: unlike the original ESP32, **routing SPI through the GPIO
matrix costs almost nothing** — the S3's matrix is clocked fast enough that IO_MUX pads
buy you headroom only above ~40 MHz. So picking SPI pins from the free list is fine here;
that is not true on an ESP32-WROOM.

---

## 7. Sleep and wake

| Mode | Typical current (bare chip) |
|---|---|
| Modem-sleep, dual core @ 240 MHz | 91.7 – 107.9 mA |
| Light sleep | 240 µA (+140 µA for 8 MB octal PSRAM at 3.3 V) |
| Deep sleep, RTC memory + peripherals on | 8 µA |
| Deep sleep, RTC memory only | 7 µA |
| Power off (CHIP_PU low) | 1 µA |

**Never quote these for a devkit.** With an LDO, a USB bridge and a power LED on the same
rail, this board draws milliamps in deep sleep.

Deep-sleep wake sources: **RTC GPIO (GPIO0-21 only)**, timer, touch, ULP. GPIO38-48 are
*not* RTC-capable and cannot wake the chip however you configure the interrupt. Pull
configuration on a wake pin must go through `rtc_gpio_pullup_en()` / `rtc_gpio_pulldown_en()`,
not `gpio_set_pull_mode()`.

On this board that leaves **GPIO0 (the BOOT button), GPIO1, GPIO2, GPIO3, GPIO14 and
GPIO21** as candidate wake pins once the camera and card are wired.

---

## 8. Clocks

| Source | Use |
|---|---|
| 40 MHz crystal | on the module, feeds the PLL; **not a board choice** |
| PLL 480/320 MHz | CPU 240/160/80 MHz, peripherals |
| RC_FAST ~17.5 MHz | fallback |
| RC_SLOW ~136 kHz | RTC, watchdogs |
| XTAL_32K on GPIO15/16 | optional 32.768 kHz — **both pins are camera pins here** |

`getCpuFrequencyMhz()` reports the live value; `setCpuFrequencyMhz(80)` is a legitimate
power lever but halves DVP throughput and will drop frames.

---

## 9. Wi-Fi and Bluetooth

Wi-Fi 802.11 b/g/n, 20/40 MHz, up to 150 Mbps; Bluetooth 5 LE with long range and 2 Mbps
PHY (no classic BR/EDR — the ESP32-S3 dropped it).

Peak current, 3.3 V, 25 °C: 340 mA (802.11b, 1 Mbps, 21 dBm), 291 mA (802.11g 54 Mbps),
286 mA (802.11n HT40). RX is 88-91 mA. Size the supply for the 340 mA figure, not the
average.

---

## 10. Flash and PSRAM

Flash: 80 MHz max by default (120 MHz needs a specific part and Espressif's sign-off),
100k program/erase cycles, 20-year retention, 4 KB sector erase 70-500 ms.

PSRAM on R8 parts is **octal SPI, 8 MB, 3.3 V**, and the module wires it to GPIO33-37.
The ECC option raises the ambient limit from 65 °C to 85 °C and costs 1/16 of the
capacity — worth knowing if a camera build runs hot in an enclosure.

Cache and DMA: the S3's cache is coherent for GDMA on internal SRAM but PSRAM access goes
through the same cache lines the CPU uses. The esp32-camera driver handles this; hand-
rolled DMA into PSRAM needs `heap_caps_malloc(..., MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA)`
and 32-byte alignment.
