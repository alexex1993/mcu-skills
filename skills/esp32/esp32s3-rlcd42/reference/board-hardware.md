# Waveshare ESP32-S3-RLCD-4.2 — the board

Everything physical. The silicon is in `esp32s3-soc.md`; copy-paste code is in
`recipes.md`.

**Sources**, referred to throughout by these tags:

| Tag | Source | Trust |
|---|---|---|
| `[S]` | **Board schematic**, `files.waveshare.com/wiki/ESP32-S3-RLCD-4.2/ESP32-S3-RLCD-4.2-schematic.pdf` | **primary — net level, settles everything below** |
| `[G]` | **Waveshare's own code repo**, `github.com/waveshareteam/ESP32-S3-RLCD-4.2` — Arduino, ESP-IDF, ESPHome and XiaoZhi examples, bundled libraries, prebuilt firmware | vendor, code that runs |
| `[P]` | Waveshare peripheral tutorial `ESP32-Peripheral-Tutorials/Display/RLCD` §4 — a working Arduino + U8g2 example for this board | vendor, code |
| `[E]` | Waveshare ESPHome tutorial `ESP32-ESPHome-Tutorials/Example-RLCD-Voice` §2.2 "GPIO Pin Assignment" | vendor, config |
| `[Z]` | Zephyr board port `boards/waveshare/esp32s3_rlcd_4_2` — `*_procpu.dts` and `*-pinctrl.dtsi` | third party, schematic-derived |
| `[W]` | Waveshare wiki `docs.waveshare.com/ESP32-S3-RLCD-4.2`, §"Onboard Resources" and §"Interface Introduction" (the GPIO table is an *image*) | vendor prose — **contains errors, see §2.2** |
| `[H]` | Home Assistant community thread "Custom Component for Waveshare ESP32-S3 4.2 RLCD (ST7305)" — a user working with the board in hand | third party, empirical |
| `[D]` | Espressif *ESP32-S3 Series Datasheet* and *ESP32-S3-WROOM-1 Datasheet* | primary |

Nothing in this file was probed on hardware by its author, but every pin claim below is
traced to the schematic. Where sources disagree, the disagreement is stated.

---

## 1. What is on the board

SKU **33298** (`ESP32-S3-RLCD-4.2`) and **33507** (`ESP32-S3-RLCD-4.2-EN`). Per Waveshare's
product page the `-EN` suffix means **shipped without the 18650 cell** — 33298 includes one,
33507 does not. It is not a region or revision variant: same PCB, same pin map.

Numbering follows the wiki's §"Onboard Resources" list `[W]`:

| # | Item | Notes |
|---|---|---|
| 1 | **ESP32-S3-WROOM-1-N16R8** | Xtensa LX7 dual-core @ 240 MHz, 16 MB quad flash + 8 MB **octal** PSRAM, PCB antenna |
| 2 | **ES7210** | 4-channel audio ADC, captures the mic array, echo-cancellation front end. I2C `0x40` (some batches `0x42`) |
| 3 | **ES8311** | low-power mono audio codec, drives the speaker. I2C `0x18` |
| 4 | **BOOT** button | GPIO0, active low. Hold while powering on to enter download mode |
| 5 | **PWR** button | **not a GPIO** — `Key3` on a power-latch IC (`U3`, ECJ23001). Long press = off, single click = on. Not readable from firmware |
| 6 | **KEY** button | GPIO18, active low, free for the application |
| 7 | **SHTC3** | temperature + humidity, I2C `0x70` |
| 8 | **PCF85063A** | RTC, I2C `0x51`, INT on GPIO15 |
| 9 | **Speaker header** | MX1.25 2-pin, driven by ES8311 through the amplifier gated by GPIO46 |
| 10 | **RTC backup header** | 2-pin, `J7`. **Rechargeable cells only** — a Schottky (`D2`) trickle-charges it from 3V3. Waveshare's page says **PH1.0**; the schematic symbol says **SH1.0** — measure before ordering a pigtail |
| 11 | **Expansion header** | 2×8, 2.54 mm — see §3 |
| 12 | **18650 holder** | main battery, sensed on GPIO4 through a 1/3 divider |
| 13 | **Dual microphone array** | two analog mics into the ES7210 |
| 14 | **CHG LED** | charge indicator, hardware only. Goes **off** when full |
| 15 | **WRN LED** | stays **on** if the battery is inserted backwards. Hardware only |
| 16 | **Type-C** | ESP32-S3 **native USB** — flashing and log printing. No UART bridge chip |
| 17 | **TF card slot** | SDMMC 1-bit, FAT32 |

There is **no user LED and no WS2812** on this board. CHG and WRN are wired to the
charger, not to a GPIO — a "blink the LED" request has to become "print to the console"
or "draw on the panel".

Mechanical `[W]`: 92.50 × 70.10 mm overall, 84.80 × 63.60 mm active area, 4 × M2.50
mounting holes on an 85.50 × 62.10 grid, 13.50 mm deep with the case, 60° stand angle.

---

## 2. The GPIO map

### 2.1 The table as the vendor prints it

The wiki's §"Interface Introduction" is a single image: a matrix of GPIO rows against
subsystem columns (`LCD`, `USB`, `RTC`, `SD`, `ES8311`, `QMI8658C`, `SYS`, `OUT`).
Transcribed verbatim, blanks omitted:

| GPIO | Cell contents, as printed |
|---|---|
| 4 | `BAT_ADC` (SYS) |
| 5 | `LCD_RS` |
| 6 | `LCD_TE` |
| 7 | `TP_INT` |
| 8 | `I2S_DSDIN` (ES8311) |
| 9 | `I2S_SCLK` (ES8311) |
| 10 | `I2S_ASDOUT` (ES8311) |
| 11 | `LCD_SCL` |
| 12 | `LCD_SDA` |
| 13 | `TP_SDA` · `RTC_SDA` · `ESP32_SDA` (QMI8658C) · `ESP32_SDA` (OUT) |
| 14 | `TP_SCL` · `RTC_SCL` · `ESP32_SCL` (QMI8658C) · `ESP32_SCL` (OUT) |
| 15 | `RTC_INT` |
| 16 | `I2S_MCLK` (ES8311) |
| 17 | `GPIO17` (OUT) |
| 18 | `KEY` (SYS) |
| 19 | `USB'_N` (SYS) |
| 20 | `USB'_P` (SYS) |
| 21 | `MOSI` (SD) |
| 38 | `SCK` (SD) |
| 39 | `MISO` (SD) |
| 40 | `LCD_CS` |
| 41 | `LCD_RESET` |
| 42 | `TP_RESET` |
| 43 | `U0TXD` (OUT) |
| 44 | `U0RXD` (OUT) |
| 45 | `I2S_LRCK` (ES8311) |
| 46 | `PA_CTRL` (ES8311) |

GPIO1, GPIO2 and GPIO3 appear as rows with every cell blank.

### 2.2 What is wrong with that table

Read it as a *starting point*, not as a schematic. The schematic `[S]` settles all three
problems below — and note that **the schematic PDF carries its own copy of this same table,
and the two copies disagree**: in the schematic's version the `GPIO7` and `GPIO42` rows and
the `TP_*` cells are *blank*.

1. **The `TP_*` rows are wrong. GPIO7 and GPIO42 are connected to nothing.**

   The touch signals are real *pins on a connector*, not nets to the MCU. The panel plugs
   into `LCD1`, a 21-way FPC connector (Hirose FH35C-21S-0.3SHW) whose footprint supports a
   touch-layer variant of this panel:

   | LCD1 pin | Signal | Goes to |
   |---|---|---|
   | 8, 9, 10, 11, 12, 13 | `RESET`, `SCL`, `TE`, `SDA`, `RS`, `CS` | GPIO41, 11, **6**, 12, 5, 40 |
   | 15 | `TP_VCC` | VCC3V3 |
   | **16, 17, 18, 19** | `TP_RESET`, `TP_INT`, `TP_SDA`, `TP_SCL` | **nothing — all four marked unconnected** |

   So there is no touch controller, no touch bus, and no GPIO behind those names. Searching
   the whole schematic for nets `GPIO7` and `GPIO42` finds them **only on the module symbol**:
   they leave pin 7 and pin 35 of the WROOM-1 and terminate. Same for `GPIO47` and `GPIO48`.

   Practical consequence: **GPIO7, GPIO42, GPIO47 and GPIO48 are free** — but none of them is
   brought out to the expansion header, so reaching one means soldering to a module pad.

2. **The `QMI8658C` column is a leftover heading.** There is no IMU on this board and none in
   the schematic. That column contains nothing but `ESP32_SDA`/`ESP32_SCL` on GPIO13/14 —
   the shared I2C bus, listed a second time.

3. **The `SD` column has no CS, and that is correct.** The slot is on the ESP32-S3's **SDMMC
   host in 1-bit mode**, not on SPI. The card's `CD/D3` (its SPI chip-select) carries a 10 K
   pull-up and reaches GPIO17 only through **`R7`, which is not populated** `[S]`. Card pins
   `D1` and `D2` are explicitly unconnected, so 4-bit mode cannot work at any speed, and the
   card-detect pin is tied to ground, so there is no card-present signal. Waveshare's own
   Arduino BSP agrees down to the defaults `[G]`:

   ```cpp
   CustomSDPort(const char *SdName, int clk = 38, int cmd = 21, int d0 = 39, int width = 1);
   ```

   The table's `MOSI`/`SCK`/`MISO` are CMD/CLK/D0 relabelled with SPI names.

**`LCD_TE` on GPIO6 is right**, even though nothing uses it: net `LCD_TE` runs from GPIO6 to
`LCD1` pin 10 `[S]`. Neither Waveshare's driver `[G]` nor U8g2 nor the Zephyr port `[Z]`
reads it, but the pin is taken.

### 2.3 The map to actually use

Every row is traced to the schematic `[S]`; the extra tags say which other sources agree.

| GPIO | Function | Polarity / notes | Sources |
|---|---|---|---|
| 0 | **BOOT** button | active low, internal pull-up. Also on the header | `[E][Z]` |
| 1 | free | ADC1_CH0, TOUCH1. On the header | `[W]` |
| 2 | free | ADC1_CH1, TOUCH2. On the header | `[W]` |
| 3 | free | ADC1_CH2, TOUCH3, **JTAG-source strap, floats**. On the header | `[W][D]` |
| 4 | **battery sense** | ADC1_CH3, `R21` 200 K / `R23` 100 K, both 1% = **1/3** | `[S][G][E][Z]` |
| 5 | **LCD DC** (`RS`) | command = low, data = high | `[P][E][Z]` |
| 6 | **LCD TE** | to `LCD1` pin 10. Read by nothing, but taken | `[S][W]` |
| 7 | **unconnected** | printed `TP_INT` in the wiki table; that is wrong (§2.2). Free, but **not on the header** | `[S]` |
| 8 | **I2S DOUT** | ESP → ES8311 `DSDIN`, i.e. **speaker** | `[P][E]` |
| 9 | **I2S BCLK** | | `[E][Z]` |
| 10 | **I2S DIN** | ES7210 `ASDOUT` → ESP, i.e. **microphones** | `[E]` |
| 11 | **LCD SCLK** | SPI2 | `[P][E][Z]` |
| 12 | **LCD MOSI** | SPI2. Write-only bus: no MISO exists | `[P][E][Z]` |
| 13 | **I2C SDA** | shared by 4 slaves. On the header | `[E][Z]` |
| 14 | **I2C SCL** | shared by 4 slaves. On the header | `[E][Z]` |
| 15 | **RTC INT** | PCF85063A interrupt/alarm, active low, pull-up | `[W][Z]` |
| 16 | **I2S MCLK** | | `[E][Z]` |
| 17 | free | ADC2_CH6. On the header. Carries the **unpopulated `R7`** to the card's CS | `[S]` |
| 18 | **KEY** button | active low, internal pull-up. On the header | `[E][Z]` |
| 19 | **USB D−** | native USB. On the header as `GP19` | `[W][D]` |
| 20 | **USB D+** | native USB. On the header as `GP20` | `[W][D]` |
| 21 | **SD CMD** | SDMMC slot 0 | `[Z]` |
| 26–32 | in-package flash bus | never touch | `[D]` |
| 33–34 | flash extension | free only on quad flash — not brought out here | `[D]` |
| 35–37 | **octal PSRAM** | **fatal to drive** on an N16R8 | `[D]` |
| 38 | **SD CLK** | SDMMC slot 0. Not a JTAG pad | `[Z]` |
| 39 | **SD D0** | SDMMC slot 0. Also **MTCK** | `[Z]` |
| 40 | **LCD CS** | active low | `[P][E][Z]` |
| 41 | **LCD RESET** | active low | `[P][E][Z]` |
| 42 | **unconnected** | printed `TP_RESET` in the wiki table; that is wrong (§2.2). Free, but **not on the header**. Also MTMS | `[S]` |
| 43 | **U0TXD** | on the header as `TXD`. Console is USB, so UART0 is free | `[W][Z]` |
| 44 | **U0RXD** | on the header as `RXD` | `[W][Z]` |
| 45 | **I2S LRCK** | **also the VDD_SPI strap** — must be low at reset | `[E][Z][D]` |
| 46 | **speaker amp enable** (`PA_CTRL`) | **active high**, `R48` 0 R to the NS4150 `CTRL`, `R49` 10 K **pull-down**; also a boot strap | `[S][E][H]` |
| 47, 48 | **unconnected**, and not brought out | free only by soldering to the module pad | `[S]` |

**Free and on the expansion header:** GPIO1, GPIO2, GPIO17, and GPIO3 if you give its
floating JTAG strap a defined level at reset — **four pins**. GPIO43/44 are a fifth and
sixth as long as the console stays on USB, which it must anyway.

**Free but not on the header:** GPIO7, GPIO42, GPIO47, GPIO48 — module pads only.

That is the whole budget.

---

## 3. The 2×8 expansion header

2.54 mm pitch, female, 16 positions, sited between the Type-C socket and the battery
holder. Read from the silkscreen on the board photo (`[W]`), with the electrical meaning
resolved from §2.3:

| Top row | | Bottom row | |
|---|---|---|---|
| `VBUS` | USB 5 V, present only when the cable is in | `3V3` | regulated 3.3 V rail |
| `GND` | | `GND` | |
| `GP19` | **native USB D−** | `GP0` | **BOOT button** |
| `GP20` | **native USB D+** | `GP1` | free, ADC1_CH0 |
| `TXD` | GPIO43, UART0 TX | `GP2` | free, ADC1_CH1 |
| `RXD` | GPIO44, UART0 RX | `GP3` | free, **JTAG strap, floats** |
| `SDA` | GPIO13, **the shared I2C bus** | `GP17` | free, ADC2_CH6 |
| `SCL` | GPIO14, **the shared I2C bus** | `GP18` | **KEY button** |

Four of the sixteen positions are traps rather than expansion:

- **`GP19` / `GP20`** are the USB data lines. Wiring anything to them removes the
  console, the flashing route and USB-Serial-JTAG in one move — and the board has no
  second USB port and no bridge chip to fall back on.
- **`GP0`** is the BOOT button. Anything holding it low at reset puts the board into
  download mode, which presents as "my firmware stopped running".
- **`GP18`** is the KEY button, with the button still fitted in parallel. Driving it as an
  output fights the button; using it as an input works, but you share it.
- **`SDA` / `SCL`** are not a private bus. Four slaves already sit on them at `0x18`,
  `0x40`/`0x42`, `0x51` and `0x70`; an external device must avoid those addresses, and the
  bus already has its pull-ups.

The rail budget: `3V3` comes off the board regulator that is also feeding the module's
Wi-Fi peaks, and `VBUS` disappears the moment the board runs on the 18650.

---

## 4. Display

| | |
|---|---|
| Panel | 4.2" fully reflective monochrome LCD, **300 × 400** native, 1 bpp |
| Controller | **ST7305.** Waveshare's product spec says "Driver IC: ST7305", their own driver and U8g2 agree; only `[Z]` says ST7306 — see below |
| Bus | 4-wire SPI on SPI2, **write-only** (no MISO, no read-back) |
| Backlight | **none, and no pin for one.** The image is formed from ambient light |
| Frame | 300 × 400 ÷ 8 = **15,000 bytes** for a full 1-bpp buffer |
| Self-refresh | HPM (`0x38`) ≈ 32 Hz, LPM (`0x39`) ≈ 1 Hz, with U8g2's init sequence `[P]` |
| Host frame rate | ≈ **45 sendBuffer()/s** at 24 MHz, measured by Waveshare `[P]` |

### 4.1 ST7305 vs ST7306

The evidence is one-sided. Waveshare's **product specification table** lists
*Driver IC: ST7305, grey scale: 2*; their peripheral tutorial `[P]` uses U8g2's
`U8G2_ST7305_300X400_F_4W_HW_SPI`; and their own repo `[G]` ships a hand-written ST7305
driver (`10_U8G2_Test/ST7305_U8g2.cpp`) alongside a copy of the **ST7305 datasheet**. The
ESPHome community component is itself a port of Waveshare's ST7305 reference driver.

Only the Zephyr port `[Z]` disagrees, declaring `compatible = "sitronix,st7306"` with
`width = <312>`, `height = <400>`, `start-column = <204>`, `mipi-max-frequency =
<10000000>` and `inversion-on`. The ST7306 is a register-compatible sibling that adds
4-level greyscale, so its driver lights this panel too.

**Use the ST7305 driver.** The odd Zephyr geometry is not arbitrary — the ST7305
addresses memory in **12-pixel blocks**, so 300 pads up to 312 (26 blocks) — but those
numbers belong to that driver. Feed 312/204 to an ST7305 driver and you get a shifted
image.

### 4.2 Clock ceiling

SPI2 reaches GPIO11/12 through the **GPIO matrix**, not the IO MUX, which caps the bus at
~40 MHz instead of 80 `[P]`. The ST7305 itself specifies a minimum write-clock period of
30 ns ≈ 33 MHz.

**24 MHz is the vendor's number in two independent places:** `setBusClock(24000000)` in the
tutorial `[P]`, and `#define SPI_CLK 24000000` in their own driver `[G]` — which also
opens the bus with `SPIClass(HSPI)` and `_spi->begin(_sck, -1, _mosi, -1)`, the very call
that rule 5 of `SKILL.md` insists on. `[Z]` is far more conservative at 10 MHz.

### 4.3 Power modes

The two commands are the whole API, and U8g2 wraps neither:

```cpp
lcd.sendF("c", 0x39);  // LPM  — self-refresh ~1 Hz
lcd.sendF("c", 0x38);  // HPM  — self-refresh ~32 Hz
delay(100);            // both need a settling delay
```

`setPowerSave(1)` is **not** LPM: it sends `0x28`, display-off, and the image vanishes
`[P]`. `setPowerSave(0)` brings it back. Waveshare's own driver ends its init with
`_cmd(0x38)` (HPM) then `_cmd(0x29)` (display on) `[G]` — the state U8g2 leaves you in
too.

Writing while in LPM is allowed but the update can take up to one refresh period (~1 s)
to appear. For a fast redraw: HPM → draw → `sendBuffer()` → LPM.

---

## 5. Audio

One I2S bus, two chips, one shared I2C control bus.

| Signal | GPIO | Direction (ESP32-S3's point of view) |
|---|---|---|
| MCLK | 16 | out |
| BCLK / SCLK | 9 | out |
| LRCK / WS | 45 | out |
| DOUT | 8 | **out** — into ES8311 `DSDIN`, playback |
| DIN | 10 | **in** — from ES7210 `ASDOUT`, capture |
| PA enable | 46 | out, **active high** |

The direction is the thing people get wrong, because the vendor table labels the pins from
the *codec's* point of view (`DSDIN` = data into the codec, `ASDOUT` = data out of it). The
schematic settles it: **GPIO8 lands on ES8311 pin 9 `DSDIN`**, and **ES7210 pin 11
`SDOUT1` reaches GPIO10 through `R42` (51 R)** `[S]`. `[E]` states the same in host terms:
`i2s_dout_pin: GPIO8`, `i2s_din_pin: GPIO10`. A user on `[H]` reports getting it backwards
first — *"I had previously confused the GPIOs for speaker and mic"* — then working audio
with exactly these pins.

**The Zephyr pinctrl has these swapped** (`I2S0_O_SD_GPIO10`, `I2S0_I_SD_GPIO8`) and its
`i2s0` node is not enabled anywhere in the board DTS, so that entry is untested. Do not
use it as a source.

The amplifier is an **NS4150** gated by GPIO46 through `R48` (0 R), with `R49` 10 K pulling
`CTRL` **down** `[S]`. So the amp is muted until firmware drives GPIO46 high, which is the
single most common "the speaker does not work" cause `[E]` `[H]`. That same pull-down is
what keeps this strapping pin low through reset — see `esp32s3-soc.md` §3. `[H]` also
reports it is worth raising GPIO46 *before* initialising the ES8311.

The ES7210 answers at `0x40` on most batches and `0x42` on some; scan rather than assume.
`[H]` confirms `0x40` and ES8311 `0x18` from a live I2C scan.

---

## 6. Storage

microSD/TF, **SDMMC host, slot 0, 1-bit mode** `[Z]`:

| Signal | GPIO | Also |
|---|---|---|
| CLK | 38 | — |
| CMD | 21 | — |
| D0 | 39 | **MTCK** |

Waveshare's own Arduino BSP declares exactly this `[G]`:

```cpp
CustomSDPort(const char *SdName, int clk = 38, int cmd = 21, int d0 = 39, int width = 1);
```

built on `esp_vfs_fat_sdmmc_mount()` + `SDMMC_HOST_DEFAULT()`, with
`format_if_mount_failed = false`.

- **D1 and D2 are explicitly unconnected at the socket** `[S]`. 4-bit mode cannot work;
  pass `mode1bit = true`.
- **There is no CS pin to pass.** The card's `CD/D3` has a 10 K pull-up and reaches GPIO17
  only through **`R7`, which is not populated** `[S]`, so the card comes up in native SD
  mode. `SD.h` over a `SPI` bus is the wrong API whatever CS you invent; use `SD_MMC.h`
  with `setPins(clk, cmd, d0)`, called *before* `begin()`. (Populating `R7` would give you
  an SPI-mode CS on GPIO17 — nobody's software expects that, and it costs you GPIO17.)
- **Card-detect is tied to ground** `[S]` — there is no card-present signal to poll.
- `[Z]` caps the bus at 20 MHz.
- FAT32 only, per `[W]`.
- Mounting the card takes MTCK (GPIO39), so pad-JTAG is gone. Use the built-in
  USB-Serial-JTAG on the Type-C port instead — it costs no pins.

---

## 7. Power

| Source | Notes |
|---|---|
| USB-C `VBUS` | 5 V, also charges the 18650 through an **ETA6098** (`U7`); `LED2` is the CHG indicator on its `STAT` pin |
| 18650 cell | main battery, in the holder on the back. `M1` (8205 dual FET) is the protection pair. Sensed on GPIO4 |
| RTC backup | `J7` 2-pin header, **rechargeable cells only** — `D2` trickle-charges it from 3V3. A primary cell here will be charged and can vent |

Rails `[S]`: `VBUS`/`VBAT` → `VSYS` → **TPS63020 buck-boost** (`U13`) → `VCC3V3`, the
main rail. A separate **RT9193-33** LDO (`U2`) makes `A3V3` for the audio analog side. The
buck-boost matters: 3.3 V holds up as the cell sags, so a brownout means the cell or the
cable, not headroom.

Battery sensing `[S]`: `R21` 200 kΩ / `R23` 100 kΩ, both 1% — the ADC sees exactly
**1/3** of the cell voltage. GPIO4 is **ADC1**_CH3, so it keeps working while Wi-Fi is up
(ADC2 does not).

A full cell at 4.2 V presents ~1.40 V at the pin, which is above the ESP32-S3 ADC's
default ~0.95 V full scale: **set 12 dB attenuation** or every reading saturates at the
same number `[E]`. `analogReadMilliVolts()` applies the eFuse calibration and is worth
using over raw counts.

Useful range for an 18650: **2.5 V empty → 4.2 V full** per `[E]`; Waveshare's own
firmware is tighter, clamping at **3.0 V = 0% and 4.12 V = 100%** `[G]`. Either curve is
defensible; say which one you used.

Waveshare's BSP for reference `[G]`: `ADC_UNIT_1`, `ADC_CHANNEL_3`, `ADC_ATTEN_DB_12`,
`ADC_BITWIDTH_12`, curve-fitting calibration, then `vol = 0.001 * mV * 3`.

The CHG LED goes **off** when charging completes (not on). The WRN LED stays **on** for a
reversed cell — check it before assuming a dead board.

---

## 8. Console, flashing and recovery

There is **no UART bridge chip**. The single Type-C socket goes to the ESP32-S3's native
USB (GPIO19/20), which is USB-Serial/JTAG plus OTG.

Consequences:

- `Serial` must be the USB CDC: `-DARDUINO_USB_MODE=1` and
  `-DARDUINO_USB_CDC_ON_BOOT=1`. Without them nothing is ever printed, on any port.
- The port enumerates as `/dev/cu.usbmodem*` (macOS), `/dev/ttyACM*` (Linux), a `COM` port
  (Windows), VID `303A`.
- The first ~1 s of boot output is lost while the host enumerates. Wait on `Serial` with a
  timeout if you need the banner.
- USB-Serial/JTAG gives you a debugger with no pins spent — the only JTAG worth using
  here, since the card takes MTCK (§6).

```sh
pio run -t upload -t monitor
```

The download-mode sequence, when auto-reset fails: **hold BOOT → tap PWR (or replug USB)
→ release BOOT → upload.** Note the difference from most ESP32 boards: **there is no
EN/RST button.** The schematic confirms it — `CHIP_PU` carries only `R9` 10 K and `C13`
100 nF, with no switch on it `[S]` — so the reset half of the dance is the PWR button or
the cable. Waveshare's own wording is "press and hold the BOOT button to power on again to
enter download mode".

Both user buttons have **external 10 K pull-ups and 100 nF debounce caps** on the board
(`R8`/`C14` on GPIO0, `R17` on GPIO18) `[S]`, so plain `INPUT` is enough.

Recovery — the ROM bootloader is in mask ROM and always answers:

```sh
esptool.py --chip esp32s3 -p <port> flash_id      # confirms 16 MB
esptool.py --chip esp32s3 -p <port> erase_flash
```

The one unrecoverable mistake is burning the `VDD_SPI` eFuse to 1.8 V on a 3.3 V module.
eFuses are one-time. Do not run `espefuse.py` on this board.

---

## 9. Flash and partitions

16 MB quad SPI flash, 8 MB octal PSRAM. In `platformio.ini`:

```ini
board_build.arduino.memory_type = qio_opi
board_build.flash_mode          = qio
board_build.partitions          = default_16MB.csv
board_upload.flash_size         = 16MB
board_upload.maximum_size       = 16777216
```

`default_16MB.csv` is a dual-OTA table: **6.25 MB per app slot** (`0x640000` each) plus a
**3.375 MB** SPIFFS (`0x360000`) and a 64 KB coredump. If you do not want OTA, a single-app
table buys back 6 MB — but do not reach for `huge_app.csv`: its app slot is 3 MB, its
coredump sits at `0x3F0000`, and everything above 4 MB of a 16 MB part is simply lost.

`board = esp32-s3-devkitc-1` is an **N8 with no PSRAM** in PlatformIO's board database.
Every one of the five settings above exists to correct it.

---

## 10. Symptom → cause → fix

| Symptom | Cause | Fix |
|---|---|---|
| Nothing on the serial port at all, board seems dead | Console flags missing; there is no UART bridge to fall back on | `-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1` |
| Banner is missing but later output appears | Host had not enumerated the CDC yet | `while (!Serial && millis()-t0 < 2000) delay(10);` |
| `psramFound()` false; large allocations fail | `esp32-s3-devkitc-1` defaults to `qio_qspi` | `board_build.arduino.memory_type = qio_opi` |
| `Cache disabled but cached memory region accessed`, random `LoadProhibited` | Something drove GPIO35/36/37 (octal PSRAM) | remove it; those pins are not usable on an R8 module |
| Panel stays blank, no error | `SPI.begin()` not called with this board's pins before `u8g2.begin()` | `SPI.begin(11, -1, 12, -1)` first |
| Panel blank after `setPowerSave(1)` | That is `0x28` display-off, not LPM | `setPowerSave(0)`; use `sendF("c",0x39)` for LPM |
| Image shifted or torn | Wrong constructor/resolution, or Zephyr's 312/204 offsets applied to an ST7305 driver | use `U8G2_ST7305_300X400_*` with `U8G2_R1` |
| Redraw takes ~1 s to appear | Panel is in LPM | HPM (`0x38`) → draw → `sendBuffer()` → LPM |
| Panel is dim / hard to read | Working as designed — reflective, no backlight | more ambient light |
| Constructor does not compile | U8g2 older than 2.36.18 | raise the `lib_deps` floor |
| I2C scan finds nothing | `Wire.begin()` with no arguments → GPIO8/9, which are I2S here | `Wire.begin(13, 14)` |
| Battery reads a stuck maximum | ADC attenuation left at default (~0.95 V full scale) | 12 dB attenuation; multiply by 3 |
| Battery reads ~1/3 of reality | divider not compensated | multiply by 3 |
| Speaker silent, codec responds on I2C | GPIO46 (PA enable) low | drive GPIO46 high |
| Microphone captures silence | DIN/DOUT swapped (Zephyr's pinctrl direction) | DOUT = 8 (out), DIN = 10 (in) |
| `SD.begin()` fails whatever CS you pass | The slot is SDMMC, not SPI — there is no CS | `SD_MMC.setPins(38,21,39)` then `begin(..., true)` |
| SD mounts in 1-bit and fails in 4-bit | D1/D2/D3 are not routed | `mode1bit = true` |
| Uploads start failing after wiring the header | Something holds `GP0` low at reset | unplug `GP0` first |
| Console and flashing both disappear after wiring the header | `GP19`/`GP20` are the USB data lines | free them |
| Board looks bricked after touching `GP18`-adjacent pins at reset | GPIO45 (VDD_SPI strap) pulled high at reset → 1.8 V flash | remove the pull; power-cycle |
| Nothing on `GP7`/`GP42` although the wiki table names them | those rows are wrong; the pins are unconnected **and not on the header** | §2.2 |
| `lib_deps` fails to resolve `olikraus/U8g2@^2.36.19` | that version exists upstream but the PlatformIO registry's newest is 2.36.18 | use `^2.36.18`; it already has the ST7305 constructors |
| Bought the `-EN` SKU and there is no cell in the box | `-EN` means *without* the 18650 | supply your own protected cell |
| Resets when Wi-Fi starts or audio plays | brownout — thin cable, tired 18650, or both | `esp_reset_reason()` says `ESP_RST_BROWNOUT` |
| RTC time is nonsense, OS flag set | No cell in the PH1.0 holder, which is how these ship | fit a **rechargeable** cell, or sync from NTP each boot |
