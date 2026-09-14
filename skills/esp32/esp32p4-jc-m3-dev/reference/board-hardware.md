# Guition JC-ESP32P4-M3-DEV hardware reference

> **Sources.** Board schematic, eight sheets, shipped by Guition as PNGs in the product
> archive `JC-ESP32P4-M3-DEV/5-Schematic/` (`1_PWR&SPEAKER`, `2_EXPAND_IO&BAT`,
> `3_8311&TFCARD`, `4_ESP32P4`, `5_USB&485`, `6_MIPI_DSI&MIPI_CSI`, `7_RJ45`, `8_OTHER`) —
> **the only authority for what is wired**. Silicon facts from *ESP32-P4 Series Datasheet
> v0.5* (same archive, `4-Driver_IC_Data_Sheet/esp32-p4_datasheet_en.pdf`). Behaviour and
> driver parameters from Guition's own demos in `1-Demo/` (ESP-IDF 5.5 and Arduino), and
> from Espressif's `esp32_p4_function_ev_board` BSP that those demos vendor unmodified.
>
> Board revision: the archive splits every demo into **`P4_V1.3`** and **`P4_V3X`**. That
> is the *silicon* revision, not a PCB revision — see §9.
>
> Marked throughout: **[S]** schematic · **[D]** ESP32-P4 datasheet · **[V]** Guition's
> demo code or the BSP it ships · **[?]** inference, said so explicitly.

---

## 1. Overview

A 92 × 62 mm carrier for the **JC-ESP32P4-M3** module. The module's own lid reads
`SOC: ESP32P4NRW32 · Memory: 32M PSRAM · Flash: 16M · WiFi: ESP32-C6`.

What that means in practice:

- **ESP32-P4** — two RISC-V HP cores plus one LP core, 360 MHz on this silicon revision.
  **No radio of any kind.**
- **An ESP32-C6 inside the same module**, reachable only over SDIO. It is the radio, and it
  is driven with `esp_hosted` / `esp_wifi_remote`, not with `esp_wifi`. (The module lid says
  only `WiFi: ESP32-C6`; nothing in Guition's material names the exact C6 variant.)
- **32 MB PSRAM in the P4's package** (16-line HEX, 200 MHz) and **16 MB NOR flash on the
  module**. Neither costs a GPIO: both are on dedicated silicon pins. [D]
- A MIPI-DSI panel connector, a MIPI-CSI camera connector, 100 Mbit Ethernet with an
  IP101 PHY, an ES8311 codec with a speaker amplifier and an on-board microphone, a
  microSD slot, RS485, a Li-ion charger, **three USB-C sockets**, and a 26-pin expansion
  header.

What it does **not** have: a user LED (only a red power indicator), a display, a camera, a
battery, or any Wi-Fi antenna work you can do from the P4 side.

---

## 2. Pin map

### 2.1 The module's pin list, as the schematic draws it [S]

The module is not an ESP32-P4 breakout: several P4 GPIOs never reach a module pin, and
several module pins are not P4 GPIOs at all. Sheet `4_ESP32P4` is the authority.

| Module pin | Name on the module | Net on the board |
|---|---|---|
| 1, 3 | GND | GND |
| 2 | `LAN_OUT` | an **RF antenna port**: a 2.2 nH π-match to the `ANT1` chip antenna, with GND on pins 1 and 3. Despite the name this is not Ethernet — the P4 has no radio, so it can only be the C6's [S] + [?] |
| 4-6 | GPIO1-3 | `GPIO1`, `GPIO2`, `GPIO3` → JP1 |
| 8, 9 | GPIO4, GPIO5 | `GPIO4`, `GPIO5` → JP1 |
| 10 | GPIO6 | **`C6_IO2`** — wired to the ESP32-C6, not free |
| 11, 12 | GPIO7, GPIO8 | `ES_I2C_SDA`, `ES_I2C_SCL` — the one I2C bus |
| 13, 14 | GPIO9, GPIO10 | I2S `DOUT`, I2S `LRCK` |
| 15 | GPIO11 | **`PA_CTRL`** — speaker amplifier enable |
| 16, 17, 18 | `C6_U0RXD`, `C6_U0TXD`, `C6_IO9` | the **C6's** UART0 and boot strap → JP1 |
| 19, 20 | GPIO12, GPIO13 | I2S `BCLK`, I2S `MCLK` |
| 21-24 | GPIO20-23 | `GPIO20` → JP1 · `TOUCH_INT` · `TOUCH_RST` · `LCD_PWM` |
| 26-31 | DSI lanes | to J2 |
| 33-38 | CSI lanes | to J3 |
| 39, 40 | `ESP_USB_N/P` | **high-speed USB OTG**, dedicated PHY → socket USB3 |
| 42, 43 | `USB1_P1_N/P` | a **full-speed USB PHY** → socket USB2, labelled "Full Speed USB". It is the **GPIO24/GPIO25** pair [?] — GPIO26/GPIO27, the other pair, come out separately on pins 45/46 as the RS485 UART — and by silicon default that pair is the USB-Serial/JTAG controller [D] |
| 45, 46 | GPIO26, GPIO27 | RS485 `TX1`, `RX1` |
| 47-50 | GPIO28-31 | Ethernet `RXDV`, `RXD0`, `RXD1`, `MDC` |
| 51, 52 | GPIO32, GPIO33 | → JP1 |
| 53, 54 | GPIO34, GPIO35 | Ethernet `TXD0`, `TXD1`; **GPIO35 is also `BOOTMODE` (SW1)** |
| 55 | GPIO36 | 10 K pull-up to 3V3 (R44), otherwise unused |
| 56, 57 | GPIO37, GPIO38 | `UART0_TXD`, `UART0_RXD` |
| 58 | `ESP_LDO_VO4` | the P4's **on-chip LDO channel 4** → the TF card's VDD |
| 59-64 | GPIO39-44 | microSD: `D0 D1 D2 D3 CLK CMD` |
| 65-68 | GPIO45-48 | GPIO45/46/47 → JP1 · **GPIO48 = I2S DIN** |
| 69-73 | GPIO49-53 | Ethernet `TXEN`, `RMII_CLK`, `PHY_RSTN`, `MDIO` · **GPIO53 = battery sense** |
| 74 | GPIO54 | **`C6_CHIP_PU`** — the C6's enable, active high |
| 75 | `CHIP_PU` | reset, SW2, 10 K pull-up (R37) |
| 76 | GPIO0 | **wire stub with no net label — reaches nothing on the board** [S] |
| 77, 78 | `ESP_3V3` | 3V3 |

**GPIO14 through GPIO19 do not appear on the module at all.** They are the SDIO link to
the C6 inside the package. [S][V]

### 2.2 Every P4 GPIO, and what this board did with it

`free?` means: available to your application on a board as sold, without giving something
up. `header` means it is on JP1 and reachable with a jumper wire.

| GPIO | This board | free? | Silicon notes [D] |
|---|---|---|---|
| 0 | module pin 76, connected to nothing | no | LP GPIO, `XTAL_32K_N` |
| 1 | **JP1 pin 7** | **yes, header** | LP GPIO, `XTAL_32K_P`, analog |
| 2 | **JP1 pin 9** | **yes, header** | LP GPIO, `MTCK` at reset, `TOUCH_CHANNEL1` |
| 3 | **JP1 pin 11** | **yes, header** | LP GPIO, `MTDI` at reset, `TOUCH_CHANNEL2` |
| 4 | **JP1 pin 13** | **yes, header** | LP GPIO, `MTMS` at reset, `TOUCH_CHANNEL3` |
| 5 | **JP1 pin 15** | **yes, header** | LP GPIO, `MTDO` at reset, `TOUCH_CHANNEL4` |
| 6 | `C6_IO2` | no | LP GPIO, `TOUCH_CHANNEL5` |
| 7 | I2C SDA | shared bus | LP GPIO, `TOUCH_CHANNEL6` |
| 8 | I2C SCL | shared bus | LP GPIO, `TOUCH_CHANNEL7` |
| 9 | I2S DOUT → speaker | costs audio | LP GPIO, `TOUCH_CHANNEL8` |
| 10 | I2S LRCK | costs audio | LP GPIO, `TOUCH_CHANNEL9` |
| 11 | `PA_CTRL` amp enable | costs audio | LP GPIO, `TOUCH_CHANNEL10` |
| 12 | I2S BCLK | costs audio | LP GPIO, `TOUCH_CHANNEL11` |
| 13 | I2S MCLK | costs audio | LP GPIO, `TOUCH_CHANNEL12` |
| 14-17 | C6 SDIO `D0`-`D3` | **no — not on the module** | LP GPIO; 14/15 are `LP_UART` |
| 18, 19 | C6 SDIO `CLK`, `CMD` | **no — not on the module** | |
| 20 | **JP1 pin 17** | **yes, header** | `ADC1_CHANNEL4` |
| 21 | `TOUCH_INT` → J2 | free if no touch panel | `ADC1_CHANNEL5` |
| 22 | `TOUCH_RST` → J2 | free if no touch panel | `ADC1_CHANNEL6` |
| 23 | `LCD_PWM` → MP3202 | free if no backlight | `ADC1_CHANNEL7`, `REF_50M_CLK` |
| 24, 25 | USB-Serial/JTAG D−/D+ | **no — the console** | full-speed USB PHY 0 |
| 26, 27 | RS485 TX1/RX1 | costs RS485 | can be full-speed USB PHY 1 |
| 28-31 | Ethernet `RXDV RXD0 RXD1 MDC` | costs Ethernet | also SPI2 `CS D CK Q` |
| 32, 33 | **JP1 pins 19, 21** | **yes, header** | SPI2 `HOLD`/`WP`; alternate RMII pads |
| 34 | Ethernet `TXD0` | costs Ethernet | **JTAG-source strap**, floats at reset |
| 35 | Ethernet `TXD1` **and SW1** | costs Ethernet | **boot strap**, weak pull-up |
| 36 | 10 K pull-up only | free, not on header | **ROM-print strap** |
| 37, 38 | UART0 to CH340C + CN2 | costs the UART console | boot straps |
| 39-44 | microSD `D0 D1 D2 D3 CLK CMD` | costs the card | SDMMC slot 0 IO MUX pads |
| 45, 46, 47 | **JP1 pins 14, 12, 10** | **yes, header** | SD1 `D4 D5 D6` — 8-bit SD only |
| 48 | I2S DIN ← microphone | costs the mic | SD1 `D7` |
| 49-52 | Ethernet `TXEN CLK RSTN MDIO` | costs Ethernet | `ADC2_CHANNEL0..3` |
| 53 | **battery sense divider** | no | `ADC2_CHANNEL4` |
| 54 | `C6_CHIP_PU` | **no** | `ADC2_CHANNEL5` |

**Eleven pins are free on a fully-populated board and all eleven are on JP1:**
`1, 2, 3, 4, 5, 20, 32, 33, 45, 46, 47`. Plus `21, 22, 23` if you never fit a panel, and
`36` if you solder to a module pad.

> ⚠︎ The sample project this skill grew out of stated that GPIO **28, 29, 30, 34** were
> free JP1 pins and blinked an LED on GPIO28. They are not on JP1: they are the Ethernet
> RMII data lines, and GPIO28 in particular is `CRS_DV`, which is an **output of the PHY** —
> driving it from the SoC puts the two in contention whenever the PHY is active.
> The mistake is invisible in a self-test, because in `GPIO_MODE_INPUT_OUTPUT` the input
> buffer reads the **pad**, and an unloaded pad follows its own driver. A passing read-back
> proves the pin drives; it never proves the pad reaches anything. Sheet `4_ESP32P4`
> (transcribed in §5) settles where these pins actually go.

### 2.3 JP1, the 26-pin expansion header [S]

2 × 13, 2.54 mm, on the left edge. Odd pins are the outer row.

| | | | |
|---|---|---|---|
| 1 | `VCC3V3` | 2 | `VCC5V` |
| 3 | `VCC3V3` | 4 | `VCC5V` |
| 5 | `GND` | 6 | `GND` |
| 7 | **`GPIO1`** | 8 | *not connected* |
| 9 | **`GPIO2`** | 10 | **`GPIO47`** |
| 11 | **`GPIO3`** | 12 | **`GPIO46`** |
| 13 | **`GPIO4`** | 14 | **`GPIO45`** |
| 15 | **`GPIO5`** | 16 | `GND` |
| 17 | **`GPIO20`** | 18 | `VCC3V3` |
| 19 | **`GPIO32`** | 20 | `C6_U0RXD` |
| 21 | **`GPIO33`** | 22 | `C6_U0TXD` |
| 23 | `ES_I2C_SDA` (GPIO7) | 24 | `C6_IO9` |
| 25 | `ES_I2C_SCL` (GPIO8) | 26 | `C6_CHIP_PU` (GPIO54) |

Four positions are traps rather than expansion: **20, 22, 24, 26 belong to the ESP32-C6.**
They are the radio's own UART0, its IO9 boot strap and its enable, exposed so the C6 can
be reflashed with a USB-TTL adapter. Driving any of them while `esp_hosted` is running
takes the radio down; pin 26 in particular holds the C6 in reset.

Pins 23/25 are the shared I2C bus of §6 — an added device must avoid `0x18`, `0x45`,
`0x5D` and `0x14`.

---

## 3. Connectors

| Ref | What | Notes |
|---|---|---|
| **USB2** | USB-C, "Full Speed USB" | GPIO24/25 → the P4's **USB-Serial/JTAG**. Flashing *and* console. Enumerates `303A:1001`, "USB JTAG/serial debug unit" |
| **USB3** | USB-C, "High speed USB" | the dedicated **USB 2.0 OTG HS** PHY. Enumerates nothing until firmware configures it |
| **USB1** | USB-C, "USB-TTL / 串口烧录" | **CH340C** bridge onto UART0 (GPIO37/38), with an RTS/DTR transistor pair onto `BOOTMODE`/`CHIP_PU` — this is the socket with classic ESP auto-reset |
| **RJ1** | RJ45 with magnetics | IP101 PHY, two link LEDs off `PHY_AD3`/`PHY_AD0` |
| **J1** | microSD / TF | push-push, SDMMC 4-bit |
| **J2** | 15-way 0.5 mm FPC | MIPI-DSI: 2 lanes + shared I2C + 3V3. **No reset and no backlight pin** |
| **J3** | 15-way 0.5 mm FPC | MIPI-CSI: 2 lanes + shared I2C (SCCB) + 3V3 + `CSI_IO0`/`CSI_IO1` pulled up and otherwise unconnected |
| **FPC1** | 6-way 0.5 mm FPC | capacitive touch: 3V3, GND, `SDA`, `SCL`, `TOUCH_INT`, `TOUCH_RST` — the same I2C bus |
| **CN1** | 2-pin JST | speaker, `SPEAKER_P`/`SPEAKER_N` off the NS4150 |
| **CN2** | MX1.25 4-pin | `VIN`, `UART0_TXD`, `RX0_1`, `GND` — power in **and** the UART0 console |
| **CN3** | 4-pin | `GND`, `ESP_3V3`, `ES_I2C_SCL`, `ES_I2C_SDA` — an I2C breakout |
| **CN4** | 2-pin JST | Li-ion battery, `BAT+`/`BAT−` |
| **CN5** | 4-pin | backlight `LEDA`/`LEDK` from the MP3202 boost |
| **J4 / J5** | 4-pin each | RS485 `A`/`B` out, and the `TX1`/`RX1` tap |
| **JP1** | 2 × 13, 2.54 mm | §2.3 |
| **SW1** | tactile | pulls `BOOTMODE` (GPIO35) low. 10 K pull-up, R34 |
| **SW2** | tactile | pulls `CHIP_PU` low — reset. 10 K pull-up R37 + C38 |
| **D1** | red LED | **power indicator only**, on the 3V3 rail through R21. Not on a GPIO |

---

## 4. Power tree [S]

```
USB 5 V (any of the three sockets)  ──┐
CN2 pin 1  VIN  ──────────────────────┴── AO3401 (Q2) ── USB5V_IN
                                                            │
CN4 Li-ion ── IP5306 (charge + 5 V boost) ── VOUT-BAT ── R22 1 A fuse ── VCC5V
                                                            │
                                            TLV62569 buck ──┴── VCC3V3 / ESP_3V3 / VDDA
                                                                 │
                                                       ESP_LDO_VO4 (on-chip) ── TF_VCC
                                                       LDO_VO3    (on-chip) ── VDD_MIPI_DPHY
```

Three things follow from this drawing:

1. **The microSD card is powered by the P4's own LDO channel 4**, through an AO3401 switch
   whose gate is held on permanently (`R10` on the GPIO45 control path is **not
   populated**; `R13` 10 K pulls the gate low). So GPIO45 is free, but the card still
   needs `sd_pwr_ctrl_new_on_chip_ldo(.ldo_chan_id = 4)` before it will answer. [S][V]
2. **The MIPI DSI PHY is powered by on-chip LDO channel 3 at 2500 mV** and must be
   acquired with `esp_ldo_acquire_channel()` before any DSI call. [V][D]
3. **The speaker amplifier and the backlight boost run from `VOUT-BAT`, not 3V3.** With no
   battery fitted that rail is the IP5306's 5 V boost off USB, so a thin cable shows up as
   audio distortion and brownouts before it shows up as anything else. Guition's own
   burning instructions ask for **more than 500 mA**.

---

## 5. Ethernet [S][V]

Internal EMAC, RMII, external PHY with its own 25 MHz crystal (X1).

> **The PHY part number is not on the schematic** — the symbol is labelled only `U4`. It is
> an **IP101** according to Guition's own demo sdkconfig (`CONFIG_EXAMPLE_ETH_PHY_IP101=y`),
> and the symbol's pin names (`MDI_TP/TN`, `REGOUT`, `ISET`, `FX_HEN/CRS_DV/RXDV`,
> `TXER/FXSD`, `COL/RMII`) are IP101 names. **PHY address `1`** likewise comes from that
> sdkconfig, not from the drawing; the schematic does show 5.1 K straps on `COL`, `RXER`,
> `CRS`, `PHY_AD0` and `PHY_AD3`, but which of those encode the address is not stated. [V][S]

| Signal | GPIO on this board | How it is routed |
|---|---|---|
| `RMII_CRS_DV` | 28 | IO MUX pad — choose from **28, 45, 51** |
| `RMII_RXD0` | 29 | IO MUX pad — **29, 46, 52** |
| `RMII_RXD1` | 30 | IO MUX pad — **30, 47, 53** |
| `RMII_TXD0` | 34 | IO MUX pad — **34, 41** |
| `RMII_TXD1` | **35** | IO MUX pad — **35, 42** |
| `RMII_TXEN` | 49 | IO MUX pad — **33, 40, 49** |
| `RMII_CLK` in (PHY → P4) | 50 | IO MUX pad — **32, 44, 50** |
| `MDC` | 31 | GPIO matrix — any GPIO |
| `MDIO` | 52 | GPIO matrix — any GPIO |
| `PHY_RSTN` | 51 | plain GPIO — any GPIO |

The alternatives come from `soc/esp32p4/emac_periph.c` in ESP-IDF 5.5.5, which is the
authority here; `SOC_EMAC_USE_MULTI_IO_MUX` is 1 for the P4. So the RMII pins **are**
configurable, through `eth_esp32_emac_config_t.emac_dataif_gpio.rmii` — but only among the
pads listed above, never to an arbitrary GPIO. (The datasheet's own "three pad groups"
framing is close but not exact: `TXD0`, `TXD1` and `TX_ER` have only two options each.)

**The useful consequence: ESP-IDF 5.5.5's `ETH_ESP32_EMAC_DEFAULT_CONFIG()` for the P4
already defaults to exactly this board's wiring** — `mdc 31`, `mdio 52`, clock
`EMAC_CLK_EXT_IN` on GPIO50, `tx_en 49`, `txd0 34`, `txd1 35`, `crs_dv 28`, `rxd0 29`,
`rxd1 30`. That is why Guition's demo sets only MDC and MDIO, to the values they already
have. On this board Ethernet needs no pin configuration at all.

**`RMII_TXD1` is GPIO35, which is also the chip's boot strapping pin and SW1.** Holding
SW1 down at reset puts the board in download mode, which is what you want; the EMAC
driving GPIO35 afterwards is fine because the strap is latched at reset and released. [D]

Guition's demo config: `CONFIG_EXAMPLE_ETH_PHY_IP101=y`, `MDC_GPIO=31`, `MDIO_GPIO=52`,
`PHY_RST_GPIO=51`, `PHY_ADDR=1`, `ETH_PHY_INTERFACE_RMII`.

---

## 6. I2C — one bus, four kinds of device [S][V]

`SDA = GPIO7`, `SCL = GPIO8`, 5.1 K pull-ups to 3V3 on board (R65, R71).

| Address | Device | Present when |
|---|---|---|
| `0x18` | ES8311 audio codec | always |
| `0x45` | brightness register of Guition's 10.1" DSI panel | that panel attached |
| `0x5D` or `0x14` | GT911 touch controller | a touch panel attached |
| — | camera SCCB | a CSI module attached |

The same two wires reach **FPC1** (touch), **J2** (DSI), **J3** (CSI), **CN3** and **JP1
pins 23/25**. There is no second bus anywhere on the board. Anything you add must avoid
those addresses, and if you put a long cable on CN3 you are extending a bus that already
has four potential participants.

---

## 7. Audio [S][V]

**ES8311** codec on I2S0, plus an **NS4150** mono class-D amplifier and an on-board analog
microphone (`MSM381A3729H9CP`) into the codec's differential MIC1 input with MICBIAS.

| Signal | GPIO | Direction |
|---|---|---|
| `MCLK` | 13 | P4 → codec |
| `BCLK` (schematic: `CODEC_I2S0_SCLK`) | 12 | P4 → codec |
| `LRCK` | 10 | P4 → codec |
| **`DOUT`** (schematic: `CODEC_I2S0_DSDIN`) | **9** | **P4 → codec → speaker** |
| **`DIN`** (schematic: `ES7210_SDOUT`) | **48** | **microphone → codec → P4** |
| `PA_CTRL` | 11 | amplifier enable, **active high, 10 K pull-down (R19)** |

Two naming traps live in that table:

- The schematic names the data lines from the **codec's** point of view, so `DSDIN` — data
  *in* to the codec — is the host's **output**. Read backwards, you get a silent speaker
  and a microphone that records nothing, with both directions reporting no error.
- The net carrying the codec's `ASDOUT` is called **`ES7210_SDOUT`**, and **there is no
  ES7210 on this board.** Sheet `3_8311&TFCARD` shows pin 7 of the ES8311 on that net.
  Searching Espressif's board files for `ES7210` will lead you to a two-microphone ADC
  this board does not have.

`PA_CTRL` low means silence whatever else is right: R19 holds it down through reset, so
the amplifier is muted until firmware raises GPIO11. Raise it *before* configuring the
codec, or the first frames pop.

Speaker output is `SPEAKER_P`/`SPEAKER_N` on CN1, driven bridge-tied from `VOUT-BAT`.

---

## 8. Storage — microSD [S][V]

**SDMMC slot 0**, the IO MUX slot, 4-bit.

| Signal | GPIO |
|---|---|
| `CLK` | 43 |
| `CMD` | 44 |
| `D0`-`D3` | 39, 40, 41, 42 |

- **There is no chip select** and its absence is not an omission — this is SDMMC, not SPI.
- **There is no card-detect and no write-protect line** reaching the SoC. The socket's
  pin 9 (`K`, the card-detect switch) is drawn as a bare stub with nothing on it. Pass
  `SDMMC_SLOT_NO_CD` and `SDMMC_SLOT_NO_WP`.
- Pull-ups to `TF_VCC` are on board: a 10 K array (`R2`) on `D2`, `D3`, `CMD` **and `CLK`**,
  and 5.1 K (`R11`, `R12`) on `D0` and `D1`.
- **`VDD` comes from the P4's on-chip LDO channel 4** (§4). Without
  `sd_pwr_ctrl_new_on_chip_ldo` the card never powers up and mounting fails with a timeout
  that looks like a bad card.
- `D4`-`D7` of the 8-bit mode are GPIO45-48, which this board uses for JP1 and the
  microphone. 8-bit SD is not available.

Because slot 0 is an IO MUX slot, the pins are not passed in the slot config at all —
Espressif's BSP simply says *"SD card is connected to Slot 0 pins. Slot 0 uses IO MUX, so
not specifying the pins here"*, and Guition ships that BSP unchanged. [V]

---

## 9. Silicon revision — the single most expensive thing to get wrong

Guition ships **two complete copies of every demo**, `P4_V1.3` and `P4_V3X`, and the split
is not cosmetic. `components/esp_hw_support/port/esp32p4/Kconfig.hw_support` states:
*"Support of ESP32-P4 rev. <3.0 and >=3.0 is mutually exclusive"*. A binary built for one
does not run on the other.

| | rev < 3.0 (boards labelled `V1.3`) | rev ≥ 3.0 (boards labelled `V3X`) |
|---|---|---|
| `esptool` prints | `revision v1.3`, ROM `esp32p4-eco2` | `revision v3.x` |
| ESP-IDF used by Guition | 5.5 | 6.0.2 |
| sdkconfig | `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` + `CONFIG_ESP32P4_REV_MIN_100=y` (or Guition's `REV_MIN_1`), giving `REV_MAX_FULL=199` | `CONFIG_ESP32P4_REV_MIN_301=y`, `REV_MAX_FULL=399` |
| PlatformIO board | `esp32-p4-evboard` (`chip_variant: esp32p4_es`) | `esp32-p4_r3-evboard` (`chip_variant: esp32p4`) |
| Arduino core | 3.2 / 3.2.1 | 3.3.11 |
| Core clock | 360 MHz | 400 MHz |
| PSRAM 250 MHz | no | yes |

ESP-IDF 5.5 **defaults to building for rev ≥ 3.1**. On a `V1.3` board that default produces
a second-stage bootloader that dies immediately after the `entry 0x...` line with
`Guru Meditation Error: Illegal instruction` — a failure that looks like corrupt flash,
not like a configuration setting. [Hardware-verified on a rev v1.3 board.]

Find out which one you have before anything else:

```sh
esptool --chip esp32p4 -p <port> flash-id       # prints "revision vX.Y" and the flash size
```

(esptool **5.3.0** ships with the pinned platform. It uses hyphenated commands and the name
`esptool`; `esptool.py` and `chip_id`/`flash_id`/`erase_flash` still work but print a
deprecation warning. `flash-id` is the safer probe: every command prints the chip banner
with the revision, and `flash-id` adds the flash size in the same breath.)

---

## 10. Memory map and clocks

| | |
|---|---|
| Main crystal | 40 MHz (`CONFIG_XTAL_FREQ=40`) |
| CPU | 360 MHz on rev < 3.0, 400 MHz on rev ≥ 3.0 |
| Internal SRAM | 768 KB L2MEM. At runtime a small IDF app reports ~625 KB of internal heap with ~575 KB free |
| HP core RAM (TCM) | 8 KB |
| LP SRAM | 32 KB |
| Flash | 16 MB NOR on the module, on dedicated `FLASH_*` pins |
| PSRAM | 32 MB in-package, HEX (16-line), 200 MHz, dedicated pins |
| ROM | 128 KB HP + 16 KB LP |

Measured on a rev v1.3 board at 360 MHz, by the benchmark this template's `--full` variant
carries: **PSRAM 82.6 MB/s write, 82.2 MB/s read** against **903 MB/s write, 196 MB/s read**
for internal SRAM (memset, and a 32-bit scalar sweep — plain-code speed, not peak bus
bandwidth). Roughly an order of magnitude. Put frame buffers and codec scratch in PSRAM;
keep anything in a hot loop in internal RAM. The same run reported **36.0 Mops/s** on the
integer CPU loop and a die temperature of 33 °C idle.

**Partition table.** PlatformIO builds the partition image from `board_build.partitions` in
`platformio.ini`, reading `board.build.partitions` in the builder —
`CONFIG_PARTITION_TABLE_*` in sdkconfig does **not** control it. Keep both in agreement.
And note what the sane-sounding default actually gives you on a 16 MB part: measured from
the built `partitions.bin`, `partitions_singleapp_large.csv` is `nvs` 24 K + `phy_init`
4 K + **`factory` 1,536 K** — 14 MB of the flash is unreachable. §12 of
`recipes.md` has a 16 MB CSV that builds.

---

## 11. Vendor material, and what is wrong with each piece

| Source | Use it for | Watch out for |
|---|---|---|
| `5-Schematic/*.png` | **everything about wiring** | none found — it is the authority here |
| `4-Driver_IC_Data_Sheet/esp32-p4_datasheet_en.pdf` (v0.5) | pin functions, straps, ADC channels, boot modes | marked PRELIMINARY |
| `1-Demo/IDF-DEMO/P4_V1.3/NoDisplay/*` | Ethernet, RS485, battery ADC, Wi-Fi via esp_hosted, UVC camera — all with working sdkconfigs | built against IDF 5.5; the `P4_V3X` copies are IDF 6.0.2 and **not interchangeable** |
| `1-Demo/IDF-DEMO/*/common_components/espressif__esp32_p4_function_ev_board` | the SD + LDO + I2C + DSI bring-up this board actually uses | it is **Espressif's EV-board BSP, vendored unmodified** — its display section assumes the EV board's panel |
| `1-Demo/ARDUINO-DEMO/*` | LVGL 8 and 9 ports, MP3 player, ADC | each README pins a different arduino-esp32 version; they are not mixable |
| `1-Demo/IDF-DEMO/*/xiaozhi-esp32/main/boards/guition-jc-esp32p4-m3-dev/` | the audio + DSI + touch pin set, confirmed against the schematic | `BOOT_BUTTON_GPIO` is defined as **21**, which is `TOUCH_INT`, not the SW1 boot button (GPIO35). Also carries a `CustomBacklight` that writes register `0x96` at I2C `0x45` — panel-specific |
| `6-User_Manual/Getting started …pdf` | Arduino IDE setup, board `ESP32P4 Dev Module`, programmer `esptool` | it is written for a different Guition board (`JC1060P470`) and says so in its own text |
| `2-Specification/… Specifications-EN.pdf` | dimensions, memory sizes | claims "up to 400 MHz"; a `V1.3` board runs 360 |
| `8-Burn operation/` | prebuilt `.bin`s including the **C6 slave firmware**, and a Windows flash tool | Windows-only tool; the `.bin`s are for Guition's own demo app |

---

# Part II — development guide

## 12. Toolchain

**The official PlatformIO platform cannot build for this board.**
`platformio/platform-espressif32`, up to and including 7.1.x, ships Arduino core 2.0.17 and
has no ESP32-P4 board definition at all. Use the **pioarduino** fork, pinned by URL:

```ini
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip
board     = esp32-p4-evboard     ; rev < 3.0.  Use esp32-p4_r3-evboard for rev >= 3.0
framework = espidf
```

55.03.311 carries ESP-IDF **5.5.5**, arduino-esp32 **3.3.11**, riscv32-esp-elf **14.2.0**
and esptool **5.3.0**.

With `framework = espidf` only `build.mcu` (`esp32p4`) and the flash parameters are taken
from the board file, which is why the `es` / `_r3` split does not matter for an IDF
project — the revision comes from `sdkconfig.defaults` instead (§9). For
`framework = arduino` the board file *is* the revision selector, through `chip_variant`.

### 12.1 The SCons breakage on PlatformIO Core 6.2.0

Symptom, at link time, on a build that compiled fine a moment ago:

```
*** [.pio/build/<env>/firmware.elf] ModuleNotFoundError : No module named 'SCons.Tool.FortranCommon'
```

Cause: Core 6.2.0 wants `tool-scons ~4.41101.0`; pioarduino's `platform.json` pins
`"package-version": "4.40801.0"`. The platform sees the mismatch and **deletes
`~/.platformio/packages/tool-scons` during the build**, pulling SCons out from under the
running process. Upstream: [pioarduino#529](https://github.com/pioarduino/platform-espressif32/issues/529).

Two fixes. Either install pioarduino's own core:

```sh
uv pip install pioarduino       # their 6.1.19, instead of official 6.2.0
```

or edit one word in the platform manifest:

```sh
cd ~/.platformio/platforms/espressif32
cp platform.json platform.json.orig
sed -i '' 's/"package-version": "4.40801.0"/"package-version": "4.41101.0"/' platform.json
```

The edit is undone by any platform reinstall or update; redo it then.

## 13. Console and flashing

Three USB-C sockets, two of which can flash the board.

| | native USB-Serial/JTAG (`USB2`) | CH340C (`USB1`) | HS OTG (`USB3`) |
|---|---|---|---|
| Enumerates as | `303A:1001`, `/dev/cu.usbmodem*`, `/dev/ttyACM*` | `1A86:*`, `/dev/cu.usbserial*`, `/dev/ttyUSB*` | nothing, until firmware configures it |
| Console | `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` | `CONFIG_ESP_CONSOLE_UART_DEFAULT` on UART0 | — |
| Auto-reset into download | yes, over the USB-Serial/JTAG protocol | yes, RTS/DTR → `BOOTMODE`/`CHIP_PU` transistors | — |
| Needs a driver | no | **CH340 driver on the host** [V] | — |
| GPIO cost | GPIO24, GPIO25 | GPIO37, GPIO38 | none — dedicated pins |

Manual download sequence, when auto-reset fails:

> **hold SW1 (`BOOTMODE`) → tap SW2 (reset) → release SW1 → upload**

Recovery: the boot ROM is mask ROM and always answers that sequence, so bad firmware
cannot brick the board.

```sh
esptool --chip esp32p4 -p <port> flash-id       # chip banner with the revision, and 16 MB
esptool --chip esp32p4 -p <port> chip-id
esptool --chip esp32p4 -p <port> erase-flash
```

The one irreversible mistake is `EFUSE_0PXA_TIEH_SEL_0`, which switches `VDDO_FLASH` from
3.3 V to 1.8 V permanently. [D] Do not run `espefuse` on this board.

## 14. Wi-Fi and Bluetooth — through the ESP32-C6

The P4 has no radio. `esp_wifi_init()` does not exist for it; the application talks to the
C6 over SDIO through `esp_hosted` and the `esp_wifi_remote` shim, which re-exports the
familiar `esp_wifi_*` API. Guition's `wifi_scan` demo resolves
`espressif/esp_wifi_remote 0.14.2` → `espressif/esp_hosted 2.0.13` on IDF 5.5.0. [V]

The link, from that demo's sdkconfig, all of it schematic-confirmed:

```
CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y
CONFIG_ESP_HOSTED_IDF_SLAVE_TARGET="esp32c6"
CONFIG_ESP_HOSTED_SDIO_SLOT=1
CONFIG_ESP_HOSTED_SDIO_4_BIT_BUS=y
CONFIG_ESP_HOSTED_SDIO_CLOCK_FREQ_KHZ=40000
CONFIG_ESP_HOSTED_SDIO_PIN_CLK=18
CONFIG_ESP_HOSTED_SDIO_PIN_CMD=19
CONFIG_ESP_HOSTED_SDIO_PIN_D0=14
CONFIG_ESP_HOSTED_SDIO_PIN_D1=15
CONFIG_ESP_HOSTED_SDIO_PIN_D2=16
CONFIG_ESP_HOSTED_SDIO_PIN_D3=17
CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE=54
CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_HIGH=y
```

**The C6 needs its own firmware.** A blank C6 makes every `esp_wifi_*` call fail at the
transport, which surfaces as a timeout during `esp_hosted` init rather than as a Wi-Fi
error. Guition ships a slave image as `8-Burn operation/Burn files/JC1060P470_C6.bin`;
`esp_hosted` can also build and flash the slave through the P4. Flash it over JP1 pins
20/22/24/26 with a USB-TTL adapter if you have to do it the hard way.

## 15. Pitfall table

| Symptom | Cause | Fix |
|---|---|---|
| `Guru Meditation Error: Illegal instruction` right after `entry 0x...` | built for rev ≥ 3.0 silicon, board is rev v1.3 | `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` + `CONFIG_ESP32P4_REV_MIN_100=y` (§9) |
| `ModuleNotFoundError: No module named 'SCons.Tool.FortranCommon'` at link | PlatformIO Core 6.2.0 vs pioarduino's pinned `tool-scons` | §12.1 |
| `Error: Unknown board ID 'esp32-p4-evboard'` | official `platform = espressif32` | pin the pioarduino URL (§12) |
| Board flashes and runs, nothing on the serial monitor | console built for UART0 but you are on the native USB socket, or the reverse | match `CONFIG_ESP_CONSOLE_*` to the socket (§13) |
| `undefined reference to esp_psram_get_size` | `sdkconfig.defaults` missing or overwritten — `CONFIG_SPIRAM` never set | restore it; PSRAM is not on by default |
| `psram` reports 0 MB | same | same |
| `temperature_sensor_install()` returns `ESP_ERR_INVALID_ARG`, log says "Out of testing range" | the requested range is not contained in any one row of the P4 range table | use a range inside one row, e.g. `(-10, 80)` |
| LED never lights, but the self-test says the pin toggles | `INPUT_OUTPUT` reads the pad, and an unloaded pad follows its own driver — the test proves the driver, not the wiring | pick from the eleven JP1 pins in §2.2 |
| Card mount times out, no error on the pins | LDO channel 4 not acquired | `sd_pwr_ctrl_new_on_chip_ldo(.ldo_chan_id = 4)` (§8) |
| Card mount fails with an SPI-looking error | you used `SPI` + a chip select | there is no CS; use SDMMC slot 0 (§8) |
| Any DSI call fails or the panel stays dark | LDO channel 3 not acquired at 2500 mV | `esp_ldo_acquire_channel()` before `esp_lcd_new_dsi_bus` |
| Speaker silent, ES8311 answers on I2C | GPIO11 still low | raise `PA_CTRL` before initialising the codec (§7) |
| Microphone records silence and the speaker works, or the reverse | DOUT/DIN swapped — the schematic names them from the codec's side | DOUT = GPIO9, DIN = GPIO48 (§7) |
| I2C scan finds nothing | wrong pins — nothing on this board defaults to GPIO7/8 | `SDA = 7`, `SCL = 8` (§6) |
| Ethernet link never comes up | wrong MDC/MDIO, or PHY address ≠ 1 | MDC 31, MDIO 52, reset 51, addr 1 (§5) |
| Firmware "stops running" after a reset | something on JP1 pin 26 or SW1 held at reset | GPIO54 is the C6 enable; GPIO35 is the boot strap (§2.3) |
| Wi-Fi API times out during init | C6 has no slave firmware, or is held in reset | §14 |
| Brownouts, audio distortion, random resets | supply under ~500 mA, or a thin cable | Guition's own note asks for >500 mA (§4) |
| `factory` partition is 1,536 KB on a 16 MB board | `partitions_singleapp_large.csv` | custom CSV, `recipes.md` §12 (§10) |
| Partition table does not match sdkconfig | PlatformIO reads `board_build.partitions`, not `CONFIG_PARTITION_TABLE_*` | keep both in agreement (§10) |
