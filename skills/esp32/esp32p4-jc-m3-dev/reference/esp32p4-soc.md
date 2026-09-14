# ESP32-P4 — the silicon

Everything here is from the **ESP32-P4 Series Datasheet v0.5** (marked PRELIMINARY on every
page), which Guition ships in the board archive as
`4-Driver_IC_Data_Sheet/esp32-p4_datasheet_en.pdf`. Section numbers below are this file's,
not the datasheet's.

The P4 is not a bigger S3. It is an application processor with no radio, and most of the
habits that carry between ESP32 variants break here in the same three places: there is no
Wi-Fi, the display and camera are MIPI rather than parallel, and the pins that are usually
eaten by flash and PSRAM are not.

---

## 1. Core and memory

| | |
|---|---|
| HP cores | 2 × RISC-V, with FPU and AI instruction extensions |
| LP core | 1 × RISC-V |
| Clock | **360 MHz on silicon rev < 3.0**, 400 MHz on rev ≥ 3.0 |
| L2MEM | 768 KB |
| HP core RAM (TCM) | 8 KB |
| LP SRAM | 32 KB |
| ROM | 128 KB HP + 16 KB LP |
| In-package PSRAM | 0, 16 MB (`ESP32-P4NRW16`) or **32 MB (`ESP32-P4NRW32`)**, OPI/HPI, `VDD_PSRAM` 1.8 V |
| Flash | **external** — dedicated `FLASH_*` pins, `VDDO_FLASH` 3.3 V by default |
| Main crystal | 40 MHz |

The silicon-revision split is a genuine hardware fork, not a stepping bump: ESP-IDF states
that support for rev < 3.0 and rev ≥ 3.0 is *mutually exclusive*, and a binary for one does
not boot on the other. `board-hardware.md` §9 has the configuration for each.

---

## 2. GPIO inventory

**GPIO0 through GPIO54 all exist. There are no holes.** That is unusual for an ESP32 and it
is worth stating because the reflex from every other variant — "some of these numbers do
not exist, and six more belong to the flash" — is wrong here:

- **Flash is on dedicated pins** (`FLASH_CS`, `FLASH_Q`, `FLASH_WP`, `FLASH_HOLD`,
  `FLASH_CK`, `FLASH_D`), not on GPIOs.
- **In-package PSRAM is on dedicated pins.** Unlike the S3's octal PSRAM, it steals nothing
  — there is no P4 equivalent of "never touch GPIO35/36/37".
- **MIPI DSI and CSI are dedicated pins** (`DSI_*`, `CSI_*`, plus a 4.02 kΩ `REXT` resistor
  for each PHY). A two-lane display and a two-lane camera cost **zero GPIOs**.
- **The high-speed USB OTG PHY is on dedicated pins** (`DM`, `DP`).

So all 55 GPIOs are, in principle, yours. In practice a board spends them; see
`board-hardware.md` §2.2 for what this one did.

### Power domains

GPIOs are grouped by supply, which matters if you are levelling to something else:

| GPIOs | Rail |
|---|---|
| 0-15 | `VDD_LP` / `VDD_BAT` (selectable per pin by register) |
| 16-23 | `VDD_IO_0` |
| 24-38 | `VDD_IO_4` |
| 39-48 | `VDD_IO_5` |
| 49-54 | `VDD_IO_6` |

Default drive strength is 20 mA, except **GPIO24 and GPIO25 at 40 mA** (they are a USB
full-speed PHY).

### Low-power and touch

**GPIO0-GPIO15 are the LP GPIOs** — the only ones the LP core can drive, the only ones that
can wake the chip from deep sleep, and the only ones with touch channels:

- `TOUCH_CHANNEL1..14` sit on **GPIO2 through GPIO15**.
- `XTAL_32K_N` / `XTAL_32K_P` are **GPIO0 / GPIO1**.
- `LP_UART` TXD/RXD are **GPIO14 / GPIO15**.
- `MTCK MTDI MTMS MTDO` (pad JTAG) are **GPIO2 GPIO3 GPIO4 GPIO5** at reset.

---

## 3. Strapping pins

Five of them, and only one has a pull by default:

| Pin | Controls | Default at reset |
|---|---|---|
| **GPIO34** | JTAG signal source | **floating — no internal pull** |
| **GPIO35** | boot mode | **weak pull-up → 1** |
| GPIO36 | ROM message printing to UART0 | floating |
| GPIO37 | boot mode (secondary) | floating |
| GPIO38 | boot mode (secondary) | floating |

Boot mode:

| Mode | GPIO35 | GPIO36-38 |
|---|---|---|
| **SPI Boot** (default) | **1** | any |
| Joint Download Boot | **0** | any |

Joint Download Boot accepts USB-Serial/JTAG, USB 2.0 OTG, UART0 **and** SPI-slave
downloads — all four at once, which is why a P4 board can usually be flashed through
whichever socket is plugged in.

Two things to carry:

- **GPIO34 has no internal pull and the datasheet says its level "must be controlled by the
  external circuit that cannot be in a high impedance state."** A board that leaves it
  genuinely floating is out of spec, even though the default eFuse combination makes the
  strap value *ignored*.
- **All straps are latched at reset** and the pins are ordinary IO afterwards. Sharing a
  strap pin with a peripheral is legal; holding it at the wrong level *through reset* is
  not. Hold time after `CHIP_PU` goes high is 3 ms minimum.

`VDDO_FLASH` is 3.3 V until `EFUSE_0PXA_TIEH_SEL_0` is burnt, after which it is 1.8 V
**permanently**. eFuses are one-time. On a board with a 3.3 V flash that is an
unrecoverable brick, and it is the only one available.

---

## 4. Analog

| Unit | Channels | GPIOs |
|---|---|---|
| **ADC1** | `CHANNEL0..7` | **GPIO16, 17, 18, 19, 20, 21, 22, 23** |
| **ADC2** | `CHANNEL0..5` | **GPIO49, 50, 51, 52, 53, 54** |

**There is no ADC2-versus-Wi-Fi problem on the P4**, because there is no Wi-Fi. The rule
you carry from every other ESP32 — "ADC2 stops working once the radio is up" — does not
apply, and ADC2 pins are first-class here.

Analog comparators: `ANA_COMP0` on GPIO51/GPIO52, `ANA_COMP1` on GPIO53/GPIO54.

An internal die temperature sensor exists. Its driver takes a *range*, and the range must
fall inside **one row** of the P4 range table in
`soc/esp32p4/temperature_sensor_periph.c`; `(-10, 80)` is the most accurate row at ±1 °C.
The rows are `{50..125, ±3} {20..100, ±2} {-10..80, ±1} {-30..50, ±2} {-40..20, ±3}`, and the
driver picks the most accurate row that **contains** the requested range. A range contained
in none of them — `(-10, 100)`, for instance — fails `temperature_sensor_install()` with
`ESP_ERR_INVALID_ARG` and the log line "Out of testing range".

---

## 5. USB — three controllers

This is the part that most often surprises someone coming from a C3 or C6.

| Controller | PHY | Pins |
|---|---|---|
| **USB 2.0 OTG high speed** (480 Mbit/s) | dedicated | `DM`, `DP` |
| **USB full speed PHY 0** | multiplexed | **GPIO24 / GPIO25** (`USB1P1_N0` / `_P0`) |
| **USB full speed PHY 1** | multiplexed | **GPIO26 / GPIO27** (`USB1P1_N1` / `_P1`) |

The USB-Serial/JTAG controller and the OTG full-speed controller both draw from the two
full-speed PHYs, and either PHY can be assigned to either — which is why a P4 board can
have a "console" USB socket and still keep a second full-speed port. **By default the
USB-Serial/JTAG controller takes GPIO24/GPIO25 and OTG-FS takes GPIO26/GPIO27**, and the
D+/D− functions can be exchanged. GPIO25 comes up with the USB pull-up enabled at reset.

On *this* board that default matters: GPIO26/GPIO27 are the RS485 UART, so enabling the
full-speed OTG controller on its default pair collides with RS485 — move it to the other
pair or give up RS485.

---

## 6. Buses and peripherals unique to, or notable on, the P4

| | |
|---|---|
| **MIPI-DSI** | 2 data lanes, dedicated pins, powered by **on-chip LDO channel 3** |
| **MIPI-CSI + ISP** | 2 data lanes, dedicated pins, integrated image signal processor |
| **H.264 encoder, JPEG codec, 2D-DMA, PPA** | present — a real video pipeline |
| **Ethernet MAC** | RMII, needs an external PHY. Each RMII signal is an IO MUX pad with its own short list of alternatives (`soc/esp32p4/emac_periph.c`): `CRS_DV` 28/45/51 · `RXD0` 29/46/52 · `RXD1` 30/47/53 · `RX_ER` 31/48/54 · `TXD0` 34/41 · `TXD1` 35/42 · `TX_EN` 33/40/49 · `TX_ER` 36/43 · `RMII_CLK` in 32/44/50 · 50 MHz clock out 23/39. MDC and MDIO go through the GPIO matrix |
| **SDMMC** | **two** external card slots. The SDIO 3.0 slot is IO MUX on **GPIO39-48**; the SDIO 2.0 slot is GPIO-matrix and can go anywhere |
| **SD card power** | can be switched by an **on-chip LDO channel**, via `sd_pwr_ctrl_by_on_chip_ldo` |
| I2S | **three** I2S controllers (`SOC_I2S_NUM` = 3), plus a separate LP I2S |
| SPI | SPI2 has an IO MUX pad set on GPIO6-11 and another on GPIO28-36 |
| I3C | one I3C master. The datasheet puts clock and data on **GPIO32-GPIO33**, which on this board are JP1 pins 19 and 21 |
| LP-I2C, LP-UART | on the LP GPIOs, usable while the HP cores sleep |
| TWAI / CAN | **three** controllers, ISO 11898-1 (CAN 2.0), 1 Kbit/s to 1 Mbit/s, pins anywhere via the GPIO matrix. **The datasheet describes no CAN FD** and ESP-IDF defines no FD capability for the P4 — where `esp32-family.md` lists the P4 under CAN FD, that row is from Espressif's product selector and this datasheet does not support it |
| Touch sensor | 14 channels, GPIO2-15 |
| **Radio** | **none. No Wi-Fi, no Bluetooth, no 802.15.4** |

`REF_50M_CLK_PAD` — a 50 MHz reference output, useful for clocking an Ethernet PHY — is
available on **GPIO23 and GPIO39**.

---

## 7. No radio, and what that means in practice

The P4 is always paired with a second chip for connectivity. The standard pairing is an
**ESP32-C6 over SDIO** running `esp_hosted`, with `esp_wifi_remote` on the P4 side
re-exporting the ordinary `esp_wifi_*` API over the transport. From the application's point
of view the code is the same; from the debugging point of view it is not:

- A Wi-Fi call that fails is often a **transport** failure — the companion chip is held in
  reset, has no slave firmware, or is on the wrong SDIO slot.
- The companion chip has its own flash and its own firmware lifecycle. Updating the P4
  application does not update it.
- `esp_wifi_remote` version and `esp_hosted` version must match the IDF version; they are
  pulled through the component manager, not vendored.

There is no ULP-FSM and no ULP-RISC-V in the S2/S3 sense — the LP core is a full RISC-V
core with its own SRAM, UART and I2C.

---

## 8. Where the family file helps

`esp32-family.md` compares the P4 against the rest of the line — what ports between chips,
which chip to choose, and the host + radio co-processor pattern in general. Use this file
for P4 detail; use that one for "should this be a different ESP32?".
