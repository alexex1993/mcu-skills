# ESP32 silicon reference

Chip-level facts, digested from *ESP32 Series Datasheet v4.3*, *ESP32-WROOM-32 Datasheet
v3.1* and the *ESP32 Technical Reference Manual v5.8*. Everything here is true of every
ESP32-WROOM-32 board regardless of how many pins its header has; what differs between the
30-, 36- and 38-pin boards is only **which of these pads you can reach**, which is in
`board-hardware.md`.

---

## 1. Part identity

The module is **ESP32-WROOM-32** (older silkscreens say ESP-WROOM-32, Ai-Thinker calls its
version ESP-WROOM-32S). Inside is an **ESP32-D0WDQ6** or, on anything made after ~2020, an
**ESP32-D0WD-V3**. Both are dual-core; the V3 die is the one with the rev-3 silicon fixes.

| | |
|---|---|
| Cores | 2 × Xtensa LX6 32-bit, individually controllable, 80 / 160 / 240 MHz |
| ROM | 448 KB (boot + core functions) |
| SRAM | 520 KB on-chip; ~320 KB is what the linker actually hands an app |
| RTC FAST RAM | 8 KB — survives deep sleep, reachable by the main CPU on RTC boot |
| RTC SLOW RAM | 8 KB — survives deep sleep, reachable by the ULP coprocessor |
| eFuse | 1 Kbit; 256 bits system (MAC, config), 768 bits user (flash encryption, chip ID) |
| Flash | 4 MB in-package on WROOM-32, on GPIO6–11, 3.3 V part |
| Crystal | 40 MHz, on the module. There is **no** 32.768 kHz crystal on any of these devkits |
| Radio | Wi-Fi 802.11 b/g/n (150 Mbps, HT40), Bluetooth v4.2 BR/EDR **and** BLE |
| FPU | Single-precision hardware FPU present (unlike the RISC-V ESP32-C3/C6) |

`esp_chip_info()` reports the revision as `revision/100` major, `revision%100` minor — a
rev-3 chip prints as `v3.0`, not `300`.

### What the module is not

- **No USB peripheral.** The original ESP32 has no USB controller of any kind. Every devkit
  built on it therefore carries a separate USB-to-UART bridge chip, and "flashing over USB"
  actually means "flashing over UART0 through a bridge". This is the single biggest
  behavioural difference from an ESP32-S2/S3/C3/C6 board and it drives most of the flashing
  rules.
- **No in-package PSRAM** on WROOM-32. An ESP32-**WROVER** module has 8 MB of PSRAM and
  spends GPIO16/GPIO17 on it.
- **No Hall sensor**, as of PCN20221202. Espressif removed it from the datasheet and the
  API; do not offer it.
- **Xtensa, not RISC-V.** Toolchain is `xtensa-esp32-elf`. ESP32-C/S-series notes about the
  RISC-V core, USB-Serial-JTAG or the LP core do not transfer.

---

## 2. Pins that do not exist

ESP32 GPIO numbers run 0–39, but **GPIO20, 24, 28, 29, 30 and 31 are not bonded out** on
the WROOM-32 package, and **GPIO37 and GPIO38** exist on the die (they are ADC1_CH1 and
ADC1_CH2) but are not bonded out on WROOM-32 either. Referring to any of these is a
compile-time-valid, runtime-useless mistake.

That leaves 34 GPIOs on the die and **32 pads on the module**. The oft-repeated "the ESP32
has 34 GPIOs" is a die-level count and is where the mistaken idea of a "34-pin ESP32 board"
comes from.

## 3. Input-only pins

**GPIO34, 35, 36 (SENSOR_VP), 39 (SENSOR_VN)** have no output driver and — this is the part
that bites — **no internal pull-up or pull-down**. Consequences:

- `gpio_set_direction(GPIO_NUM_34, GPIO_MODE_OUTPUT)` returns `ESP_ERR_INVALID_ARG`, but
  a lot of code ignores the return value and then wonders why the pin never moves.
- A button on GPIO34 needs a physical 10 kΩ resistor. `GPIO_PULLUP_ENABLE` is accepted by
  `gpio_config()` and does nothing.
- Left floating they read as noise, which is the correct behaviour, not a fault.

They are the lowest-noise ADC1 inputs on the chip, which is what they are for.

## 4. Strapping pins

Five pins are sampled at reset-release (power-on, RTC watchdog and brownout resets) and
latched for the whole boot:

| Pin | Internal default | Meaning |
|---|---|---|
| **GPIO0** | pull-**up** | 1 = SPI boot from flash · 0 = serial download mode |
| **GPIO2** | pull-down | must be 0 (or floating) to enter download mode with GPIO0 low |
| **GPIO5** | pull-up | SDIO slave timing |
| **MTDI / GPIO12** | pull-**down** | 0 = VDD_SDIO 3.3 V · 1 = 1.8 V |
| **MTDO / GPIO15** | pull-up | 1 = ROM bootloader log on U0TXD · 0 = silent |

Read them back at any point in the boot from `GPIO_STRAP_REG` (0x3FF44038). **TRM Register
6.13 orders the bits high-to-low**: bit5..bit0 are MTDI, GPIO0, GPIO2, GPIO4, MTDO, GPIO5.
Guessing the order from the pin list gives you the reverse, which decodes plausible-looking
nonsense. `template/src/board_report.c` has the correct decode.

**MTDI is the dangerous one.** The module's flash is a 3.3 V part. Strapping VDD_SDIO to
1.8 V by holding GPIO12 high at reset under-supplies it: boots become intermittent, and the
symptom presents as flash corruption or random resets rather than as a pin problem. A 10 kΩ
pull-up on GPIO12, or an LED to 3V3, or a display module that idles its chip-select high
and happens to be wired there, all do it.

After reset-release all five behave as ordinary pins.

## 5. Flash pins — GPIO6 to GPIO11

`SCK/CLK` (6), `SDO/SD0` (7), `SDI/SD1` (8), `SHD/SD2` (9), `SWP/SD3` (10), `SCS/CMD` (11)
are wired inside the module to the 4 MB flash die. The datasheet's wording is "not
recommended for other uses"; in practice they are unusable, because the CPU fetches
instructions from that flash through those exact wires. Loading a pin, or configuring one
as an output, hangs the chip somewhere between the bootloader and the first cache miss.

Whether they appear on the header is the main difference between the board sizes:

| Board | GPIO6–11 on the header? |
|---|---|
| 30-pin | no |
| 36-pin | yes, six pins at the USB end |
| 38-pin | yes, six pins at the USB end |

Two side effects worth knowing:

- **UART1's default pins are GPIO9 and GPIO10** — i.e. flash pins. Any use of UART1 must
  remap them through the GPIO Matrix first. UART0 (1/3) and UART2 (16/17) are fine.
- The ESP32's "SPI" host `SPI1_HOST` is the flash controller. Applications get `SPI2_HOST`
  (HSPI) and `SPI3_HOST` (VSPI) only.

## 6. Power domains and current

Three digital domains: **VDD3P3_RTC**, **VDD3P3_CPU**, **VDD_SDIO** (fed by an internal LDO
from VDD3P3_RTC, which is why MTDI's strap matters).

| Parameter | Value |
|---|---|
| Supply voltage | 3.0 – 3.6 V (chip tolerates 2.3 – 3.6 V), 3.3 V typical |
| Supply current the source must be able to deliver | **≥ 500 mA** |
| Average operating current | ~80 mA |
| Absolute max cumulative IO output current | **1100 mA** |
| Per-pin source current, max drive strength | ~40 mA typ (VDD3P3_CPU / VDD3P3_RTC) |
| — degradation | falls toward ~29 mA per pin as more pins source at once |
| Per-pin sink current | ~28 mA typ at V_OL = 0.495 V |
| Internal pull-up / pull-down | 45 kΩ typ |
| V_IH / V_IL | 0.75 × VDD / 0.25 × VDD |
| Power-on: 3V3 must be stable before CHIP_PU rises | ≥ 50 µs |
| CHIP_PU low time for a reset | ≥ 50 µs, below 0.6 V |

Below 2.3 V the datasheet asks you to hold CHIP_PU low rather than let the chip try to
boot — relevant for battery designs, and the reason a sagging USB rail shows up as
`ESP_RST_BROWNOUT` instead of as garbage.

### Power modes (datasheet Table 3-2)

| Mode | Current |
|---|---|
| Active, Wi-Fi TX | see RF table; bursts to ~240–300 mA |
| Modem-sleep, 240 MHz, dual-core | 30–68 mA |
| Modem-sleep, 160 MHz | 27–44 mA |
| Modem-sleep, 80 MHz | 20–31 mA |
| Light-sleep | 0.8 mA |
| Deep-sleep, ULP powered | 150 µA |
| Deep-sleep, ULP sensor pattern @1 % duty | 100 µA |
| Deep-sleep, RTC timer + RTC memory | **10 µA** |
| Hibernation, RTC timer only | 5 µA |
| CHIP_PU held low | 1 µA |

These are **chip** figures. On a devkit the LDO's quiescent draw, the USB bridge chip and
the power LED dominate: a typical CH340/AMS1117 board idles at 8–20 mA in deep sleep, three
orders of magnitude above the 10 µA number. Never quote the datasheet figure for a devkit.

## 7. Analog

### ADC

Two 12-bit SAR ADCs, 18 channels total, 200 ksps via the RTC controller and 2 Msps via the
DIG controller.

| Unit | Channels → GPIO |
|---|---|
| **ADC1** | CH0→36 (VP), CH1→37\*, CH2→38\*, CH3→39 (VN), CH4→32, CH5→33, CH6→34, CH7→35 |
| **ADC2** | CH0→4, CH1→0, CH2→2, CH3→15, CH4→13, CH5→12, CH6→14, CH7→27, CH8→25, CH9→26 |

\* not bonded out on WROOM-32.

**ADC2 is unusable whenever Wi-Fi is running.** The Wi-Fi PHY arbitrates for the same
hardware and wins; `adc_oneshot_read()` on ADC2 returns `ESP_ERR_TIMEOUT` for the entire
time the driver is started. There is no workaround. ADC1's eight channels are the whole
analog budget of a connected application — and on a 30-pin board four of those eight
(GPIO34/35/36/39) are the input-only pins, which is convenient rather than limiting.

Accuracy, from datasheet Tables 3-3 and 3-4:

| Attenuation | Effective range | Total error after eFuse-Vref calibration |
|---|---|---|
| 0 dB | 100 – 950 mV | ±23 mV |
| 2.5 dB | 100 – 1250 mV | ±30 mV |
| 6 dB | 150 – 1750 mV | ±40 mV |
| 12 dB (`ADC_ATTEN_DB_12`) | 150 – 2450 mV | ±60 mV |

- Uncalibrated chip-to-chip spread is **±6 %**. Calibration is not optional if the number
  means anything.
- Above ~2450 mV at 12 dB the transfer function flattens and accuracy collapses — a divider
  that maps 4.2 V onto 3.3 V puts you in that region. Aim the top of the range at ~2.4 V.
- The ESP32 supports **line fitting only** (`adc_cali_create_scheme_line_fitting`). Curve
  fitting is an ESP32-S2/S3/C3 scheme; calling it here fails to link.
- DNL ±7 LSB, INL ±12 LSB. Averaging several samples is worth more than any calibration.

### DAC

Two 8-bit DACs: **DAC_1 on GPIO25**, **DAC_2 on GPIO26**. The reference is the supply, so
full scale is VDD3P3_RTC, not a fixed 3.3 V. Removed from the newer chip generations, so
these two pins are the only true analog outputs you will ever get from an ESP32.

### Touch

Ten capacitive channels: **TOUCH0**→GPIO4, 1→0, 2→2, 3→15, 4→13, 5→12, 6→14, 7→27,
8→GPIO33, 9→GPIO32. Note that six of the ten sit on strapping or JTAG pins; on a bare
devkit the two clean ones are **GPIO32 and GPIO33**. The datasheet carries a note about
"limited applications of touch sensor" added in v4.1 — treat touch on this chip as usable
but not a precision instrument.

## 8. IO MUX vs GPIO Matrix — the speed rule

Every peripheral signal can reach any pin through the **GPIO Matrix**, which is what makes
the ESP32 pleasant to lay out. High-speed functions (SPI, SDIO, Ethernet, JTAG, UART) can
alternatively use their **IO_MUX** pads and bypass the matrix.

The matrix costs **25 ns** of round-trip routing delay (`_GPIO_MATRIX_DELAY_NS` in the IDF
HAL). For SPI the driver turns that into a hard clamp:

```
limit = APB_CLK / (floor((1 + input_delay_ns + gpio_delay_ns) * 80/1000) + 1)
```

- IO_MUX pins, no input delay → `80 MHz / 1` = **80 MHz**
- GPIO Matrix → `(1+25)*0.08 = 2` → `80 MHz / 3` = **26.67 MHz**

So SPI on arbitrary pins tops out at 26.67 MHz for full-duplex transfers, and no
configuration recovers it. The IO_MUX pads are:

| Signal | SPI2 / HSPI | SPI3 / VSPI |
|---|---|---|
| CLK | GPIO14 (MTMS) | **GPIO18** |
| MISO (Q) | GPIO12 (MTDI) | **GPIO19** |
| MOSI (D) | GPIO13 (MTCK) | **GPIO23** |
| CS0 | GPIO15 (MTDO) | **GPIO5** |
| WP | GPIO2 | GPIO22 |
| HD | GPIO4 | GPIO21 |

**SPI3/VSPI is the one to use.** SPI2's IO_MUX pads are the JTAG group and include two
strapping pins (GPIO12 MTDI and GPIO15 MTDO), so getting 80 MHz out of SPI2 means accepting
the MTDI hazard on your MISO line. Half-duplex and write-only transfers (displays) are not
bound by the same read-timing limit and can be pushed higher on matrix pins, but the 80 MHz
figure only ever applies to IO_MUX.

I2C, LEDC, RMT, PCNT, UART and everything else are matrix-routed and pin-agnostic; there is
no speed penalty that matters at their frequencies.

## 9. Digital peripheral inventory

| Peripheral | Count / note |
|---|---|
| UART | 3 (UART0/1/2). UART1 defaults to the flash pins — remap before use |
| I2C | 2, any pins, master or slave. 100 k / 400 k standard, up to 5 MHz in theory — in practice bounded by the pull-up strength |
| SPI | SPI2 (HSPI) and SPI3 (VSPI) available; SPI1 is the flash controller |
| I2S | 2, with an APLL for exact audio rates |
| LEDC | 16 channels: **8 high-speed + 8 low-speed**, 4 timers each. Only low-speed channels can keep running in light sleep |
| MCPWM | 2 units, 3 pairs each — the one for motor bridges with dead-time |
| RMT | 8 channels, **flexible** — any channel is TX or RX, 64 symbols each. No DMA; the receiver stops when its buffer fills (RMT v1). See `esp32-family.md` §6 for why this is the chip where long WS2812 chains glitch under Wi-Fi load |
| PCNT | 8 units, quadrature-capable |
| TWAI (CAN) | 1, ISO 11898-1, needs an external transceiver |
| SDMMC host | 1, 1/4/8-bit — collides with the JTAG/strapping group |
| Ethernet MAC | RMII, needs an external PHY; EMAC pins are scattered across 0/16/17/18–27 |
| Timers | 4 × 64-bit general purpose, plus 2 task watchdogs and an RTC watchdog |
| Crypto | AES, SHA, RSA, RNG, flash encryption, secure boot |
| ULP | FSM coprocessor in the RTC domain, runs during deep sleep |

## 10. Clocks

- **40 MHz crystal** on the module. `CONFIG_ESP32_XTAL_FREQ_40=y` matches it; the 26 MHz
  setting exists for other modules and produces a garbled console at every baud rate if
  selected by mistake.
- CPU: 80 / 160 / 240 MHz from PLL (320 or 480 MHz), or XTAL/N.
- **APB_CLK is 80 MHz** whenever the CPU runs from PLL. Timer, SPI and I2C dividers are all
  relative to it — this is the number that turns up in every peripheral frequency
  calculation.
- RTC slow clock: internal 150 kHz RC by default. The alternative is a 32.768 kHz crystal
  on GPIO32/GPIO33 — **not fitted on any of these devkits**, and those two pins are wired to
  the header instead. Deep-sleep timing therefore has the RC oscillator's temperature drift,
  several percent, which matters if you sleep for hours.

## 11. Memory map, briefly

| Region | Address | Size |
|---|---|---|
| External flash, instruction (XIP) | 0x400C_2000 – 0x40BF_FFFF | up to 11 MB + 248 KB mapped; past 3 MB + 248 KB the CPU's speculative reads start costing cache performance |
| External flash, read-only data | 0x3F40_0000 – 0x3F7F_FFFF | up to 4 MB mapped at a time |
| External RAM (WROVER PSRAM only) | 0x3F80_0000 – 0x3FBF_FFFF | up to 4 MB mapped |
| Internal SRAM 1 | 0x3FFE_0000 (data) / 0x400A_0000 (instr) | 128 KB, reachable both ways |
| Internal SRAM 2 | 0x3FFA_E000 – 0x3FFD_FFFF | 200 KB, data only |
| RTC FAST | 0x3FF8_0000 (data) / 0x400C_0000 (instr) | 8 KB |
| RTC SLOW | 0x5000_0000 | 8 KB |

The 520 KB of SRAM is not a single pool: the linker's DRAM window plus the space the
bootloader and Wi-Fi stack claim leaves roughly **320 KB** for an application, and
`heap_caps_get_free_size(MALLOC_CAP_8BIT)` after `app_main()` starts is the honest number.
A Wi-Fi station with a couple of sockets costs about 50 KB of that.

## 12. Boot and reset

Reset sources, as `esp_reset_reason()` reports them: `ESP_RST_POWERON`, `ESP_RST_EXT` (the
EN button), `ESP_RST_SW`, `ESP_RST_PANIC`, `ESP_RST_INT_WDT`, `ESP_RST_TASK_WDT`,
`ESP_RST_WDT`, `ESP_RST_DEEPSLEEP`, `ESP_RST_BROWNOUT`, `ESP_RST_SDIO`.

`ESP_RST_BROWNOUT` on a devkit almost always means the USB supply, not the firmware. It
characteristically appears at the first Wi-Fi transmission, because that is the first time
the board pulls ~300 mA.

The ROM bootloader prints its banner on U0TXD at **115200 baud** derived from the 40 MHz
crystal before any of your configuration applies, which is why 115200 is the right monitor
speed even if the app reconfigures the console later. `rst:0x1 (POWERON_RESET),boot:0x13`
in that banner decodes the strapping: the `boot:` byte's low bits are the same GPIO0/GPIO2
latch described above.

---

## 13. Deep sleep

Wake sources: RTC timer, **ext0** (one RTC GPIO, level-triggered), **ext1** (a mask of RTC
GPIOs, any-high or all-low), touch, ULP, and the RTC watchdog.

Only **RTC GPIOs** can wake the chip. On WROOM-32 those are:

```
0, 2, 4, 12, 13, 14, 15, 25, 26, 27, 32, 33, 34, 35, 36, 39
```

(GPIO37/38 are RTC-capable on the die but not bonded out.) Everything else — 1, 3, 5, 16,
17, 18, 19, 21, 22, 23 — cannot wake the chip no matter how the interrupt is configured.

Two details that cost an afternoon each:

- **`ext0` needs the pin's internal pull configured through `rtc_gpio_pullup_en()`**, not
  `gpio_set_pull_mode()`. The digital GPIO pull registers are powered down in deep sleep.
- **GPIO state is not held across deep sleep** unless you call `gpio_hold_en()` and
  `gpio_deep_sleep_hold_en()`. A relay or a MOSFET gate driven high before sleeping goes
  back to floating the moment the chip powers the digital domain down.
