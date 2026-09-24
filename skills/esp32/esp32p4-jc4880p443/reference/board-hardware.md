# Guition JC4880P443C_I_W hardware reference

Source markers used throughout:

- **[S]** traced on Guition's schematic `5-Schematic/JC4880P443_V1.0.pdf` (the netlist is
  embedded in the PDF text, so every pin below was matched net-by-net, not read off a
  picture) and the seven PNG sheets next to it
- **[V]** taken from Guition's own demo code in `1-Demo/idf_examples/ESP-IDF_5.5.4`
- **[D]** ESP32-P4 datasheet / ESP-IDF 5.5.5 source
- **[H]** observed on a real board (rev v1.3) during the bring-up this skill was written from

The board is also sold as **JC4880P443C_I_W_Y** (same board in a shell) and appears in
shops and forums as "JC4880P443", "JC4880P443C", "guition-esp32-p4-jc4880p443" (back
silkscreen), and — through a typo that spread — "JC4880P433". It is the 4.3" member of the
same family as the 7" JC1060P470 and the 10.1" JC8012P4A1, and it carries the same
**JC-ESP32P4-M3** module as the JC-ESP32P4-M3-DEV carrier (see the `esp32p4-jc-m3-dev` skill).

---

## 1. Overview

| | |
|---|---|
| Module | **JC-ESP32P4-M3** (Guition): ESP32-P4NRW32 + 16 MB flash + ESP32-C6 on SDIO [S] |
| Silicon | ESP32-P4 **rev v1.3** on the board this was written from; ROM `esp32p4-eco2-20240710` [H]. Guition's archive also carries `P4_V3X` builds, so rev v3.x boards exist |
| Flash | 16 MB **Boya**, JEDEC `0x684018` [H] |
| PSRAM | 32 MB AP Memory HEX-PSRAM, generation 4, 200 MHz [H] |
| Panel | 4.3" IPS, 480 × 800 portrait, **ST7701S**, 2-lane MIPI-DSI, 93.60 × 56.16 mm active area |
| Touch | **GT911** capacitive, product ID `"911"`, config version 250 [H] |
| Audio | ES8311 codec, NS4150 3 W class-D amp → CN1, one analog MEMS mic (MSM381A3729H9CP) [S] |
| Storage | microSD (TF) on SDMMC slot 0, powered from on-chip LDO 4 [S][V] |
| Camera | optional, FPC2 15-pin MIPI-CSI; Guition's code expects an **OV02C10** at 0x36 [S][V] |
| RS485 | MAX485 on UART1 with automatic direction, J4 (A/B), J5 (TTL TX1/RX1) [S] |
| Battery | 1-cell Li-ion on CN4, IP5306 charger/boost, power key SW3 [S] |
| USB | **two** USB-C: full-speed (native USB-Serial/JTAG) and high-speed OTG. **No USB-UART bridge** [S][H] |
| Buttons | SW1 BOOT (GPIO35), SW2 RESET (CHIP_PU), SW3 IP5306 power key [S] |
| LEDs | D1 red power LED on the 3V3 rail, **nothing on a GPIO** [S] |
| Size | 117.01 × 69.41 mm; 5 V, ~320 mA typical (Guition spec) |

**Not on this board**, although the module's reference silkscreen suggests otherwise: no
Ethernet PHY or RJ45 (the RMII net names on JP1 are inherited labels), no CH340, no ES7210.

---

## 2. Pin map

### 2.1 Module pads, as the schematic wires them [S]

Every GPIO pad of the JC-ESP32P4-M3, and the net it lands on. "—" means the pad carries only
a net label with no second connection: the pin is not routed anywhere on this board.

| Module pin | GPIO | Net / destination |
|---|---|---|
| 4 | 1 | — |
| 5 | 2 | — |
| 6 | 3 | — |
| 8 | 4 | — |
| 9 | 5 | **FPC1 pin 23 — panel reset** (Guition: `BSP_LCD_RST = GPIO_NUM_5`) |
| 10 | 6 | `C6_IO2` — to the ESP32-C6 |
| 11 | 7 | I2C SDA (`ES_I2C_SDA` = `RTC_DAT/SDA1`) |
| 12 | 8 | I2C SCL (`ES_I2C_SCL` = `RTC_CLK/SCL1`) |
| 13 | 9 | `CODEC_I2S0_DSDIN` — I2S data **to** the codec |
| 14 | 10 | `CODEC_I2S0_LRCK` |
| 15 | 11 | `PA_CTRL` — NS4150 enable |
| 16-18 | — | `C6_U0RXD`, `C6_U0TXD`, `C6_IO9` (C6 pins, also on JP1) |
| 19 | 12 | `CODEC_I2S0_SCLK` (bit clock) |
| 20 | 13 | `CODEC_I2S0_MCLK` |
| 21 | 20 | — |
| 22 | 21 | `TOUCH_INT` → FPC1 pin 28 |
| 23 | 22 | `TOUCH_RST` → FPC1 pin 25 |
| 24 | 23 | `LCD_PWM` → R69 0R → MP3202 EN (R72 10K to GND) |
| 26-31 | — | DSI clock + 2 lanes → FPC1 |
| 33-38 | — | CSI clock + 2 lanes → FPC2 |
| 39/40 | — | `ESP_USB_N/P` → high-speed USB-C (USB3) |
| 42/43 | 24/25 | `USB1P1_N/P` → full-speed USB-C (USB2) — USB-Serial/JTAG |
| 45 | 26 | `TX1` → J5 pin 2, MAX485 D, direction one-shot |
| 46 | 27 | `RX1` → J5 pin 3, MAX485 R |
| 47-50 | 28-31 | JP1 pins 21, 14, 12, 10 |
| 51-52 | 32-33 | JP1 pins 19, 8 |
| 53 | 34 | JP1 pin 17 |
| 54 | 35 | `BOOTMODE`: SW1 to GND, R34 10K pull-up, JP1 pin 15 |
| 55 | 36 | R44 10K pull-up to 3V3, nothing else |
| 56 | 37 | `UART0_TXD` → R33 0R → CN2 pin 2 |
| 57 | 38 | `UART0_RXD` → R35 0R → CN2 pin 3 |
| 58 | — | `ESP_LDO_VO4` → TF card power switch |
| 59-64 | 39-44 | SD D0, D1, D2, D3, CLK(43), CMD(44) |
| 65 | 45 | R10 → TF power switch gate, **R10 not populated** |
| 66-67 | 46-47 | — |
| 68 | 48 | `ES7210_SDOUT` — actually the ES8311's ADC output (see §8) |
| 69-72 | 49-52 | JP1 pins 13, 11, 9, 7 |
| 73 | 53 | R55 0R → battery divider mid-point |
| 74 | 54 | `C6_CHIP_PU` — the C6's enable, R81 10K pull-up, C64 |
| 75 | — | `CHIP_PU`: SW2, R37 10K, C38 1 µF |
| 76 | 0 | — (module-internal) |

GPIO14-19 are the SDIO link to the C6 and never reach a module pad [V][D].

### 2.2 Every P4 GPIO, by what it costs you

| GPIO | Status on this board |
|---|---|
| **28, 29, 30, 31, 32, 33, 34, 49, 50, 51, 52** | **free, on JP1.** No strap, no on-board load |
| 35 | on JP1 too, but it is **the boot strap and SW1**. Usable as an input after boot; anything that pulls it low through reset enters download mode |
| 1, 2, 3, 4, 20, 46, 47 | not routed — module pads only, unreachable without soldering to the module |
| 36 | 10K pull-up, not routed |
| 45 | not routed (R10 unpopulated) |
| 5 | panel reset |
| 21, 22 | touch INT / RST |
| 23 | backlight enable |
| 7, 8 | the only I2C bus |
| 9, 10, 11, 12, 13, 48 | audio |
| 24, 25 | USB-Serial/JTAG — the console and flashing port |
| 26, 27 | RS485 / J5 UART1 |
| 37, 38 | UART0 on CN2 |
| 39-44 | microSD |
| 53 | battery sense |
| 6, 14-19, 54 | ESP32-C6 link — never touch |

**Compared with the JC-ESP32P4-M3-DEV** (same module): there, 28-31, 34, 35, 49-52 are
Ethernet RMII and 1-5, 20, 45-47 are on the header. Here it is the other way round. Code
written for one board's free pins drives the other board's Ethernet or nothing at all.

### 2.3 JP1, the 2×13 2.54 mm header [S]

```
          3V3   1  2   5V
          3V3   3  4   5V
          GND   5  6   GND
       GPIO52   7  8   GPIO33
       GPIO51   9 10   GPIO31
       GPIO50  11 12   GPIO30
       GPIO49  13 14   GPIO29
 GPIO35/BOOT   15 16   GND
       GPIO34  17 18   3V3
       GPIO32  19 20   C6_U0RXD     ┐
       GPIO28  21 22   C6_U0TXD     │ ESP32-C6, not the P4:
  I2C SDA (7)  23 24   C6_IO9       │ brought out to reflash it.
  I2C SCL (8)  25 26   C6_CHIP_PU   ┘ Driving them breaks Wi-Fi.
```

The schematic symbol labels pins 7-21 with RMII names (`MDIO`, `PHY_RSTN`, `RMII_CLK`,
`RMII_TXEN`, `RMII_TXD0/1`, `RMII_RXD0/1`, `RMII_RXDV`, `MDC`) because the header was drawn
for Ethernet add-ons. There is no PHY on this board; they are plain GPIOs.

---

## 3. Connectors

Numbering follows the spec sheet's interface drawing; designators are the schematic's.

| # | Connector | Pinout / notes [S] |
|---|---|---|
| 1 | MIC1 | on-board analog MEMS mic into ES8311 MIC1 (differential, MICBIAS) |
| 2 | **CN1**, MX1.25 2P | speaker, NS4150 bridge output. **Nothing is bundled** — "the speaker needs to be connected externally" (Guition) |
| 3 | **CN4**, MX1.25 2P | Li-ion cell, BAT+ / BAT− |
| 4 | **USB2**, USB-C "full-speed" | GPIO24/25 → **USB-Serial/JTAG**, `303A:1001`. The flashing and console port. Guition's burn notes call it "USB2" |
| 5 | **USB3**, USB-C "high-speed" | USB 2.0 OTG HS PHY. Power only until firmware starts TinyUSB/USB host |
| 6 | **JP1**, 2×13 2.54 mm | §2.3 |
| 7 | **CN2**, MX1.25 4P | `VIN` (5 V in, through Q2 AO3401 reverse protection), `TX0` (GPIO37), `RX0` (GPIO38), `GND` |
| 8 | **FPC2**, 15-pin 0.3 mm flip-down | MIPI-CSI camera: 2 lanes + clock, SCCB on the shared I2C through R30/R32 0R, 3V3, `CSI_IO0` (10K pull-up, not to the P4), `CSI_IO1` (not to the P4) |
| 9 | **J5**, MX1.25 4P | `USB5V_IN`, `TX1`, `RX1`, `GND` — UART1 at 3.3 V TTL, in parallel with the MAX485 |
| 10 | **J4**, MX1.25 4P | `USB5V_IN`, `A`, `B`, `GND` — RS485, 0.1 A PTC fuses F1/F2, SMAJ6.5CA TVS, 5K1 bias R73 |
| 11 | **CN3** | I2C: `GND`, `3V3`, `SCL`, `SDA` on the shared bus. (The spec sheet calls it "HS 1.0 2P"; the schematic symbol has four signal pins) |
| — | **FPC1**, 30-pin 0.5 mm | panel + touch: `LEDK` 1, `LEDA` 2, DSI lanes 6-13, `3V3` 4/21/29, `TE` 22 (not routed to the P4), `GPIO5` 23 (reset), `TOUCH_RST` 25, `SDA` 26, `SCL` 27, `TOUCH_INT` 28 |
| — | ANT1 | the C6's antenna, u.FL/IPEX style `CA-C03` with a π-match (L4 2.2 nH) |

---

## 4. Power tree [S]

```
USB2 / USB3 VBUS ─┬─ USB5V_IN ──► IP5306 VIN ──► VOUT-BAT (5 V boost out, or battery)
CN2 VIN ── Q2 ────┘                  │
                                     └─ BAT ◄──► CN4 Li-ion cell
                                               └─ R52 68K / R57 100K ─► GPIO53

VOUT-BAT ─┬─ R22 (1 A fuse) ─► VCC5V ─► TLV62569 buck ─► VCC3V3 = ESP_3V3 = VDDA
          ├─ NS4150 speaker amplifier
          └─ MP3202 boost (EN = GPIO23) ─► LEDA / LEDK (panel backlight)
USB5V_IN ─► MAX485 + 74LVC1G132 (RS485 is powered from USB/VIN only, not from the battery)
ESP_LDO_VO4 (on-chip LDO 4) ─► AO3401 Q1 (always on) ─► TF_VCC
on-chip LDO 3 (2.5 V) ─► VDD_MIPI_DPHY
```

- **The backlight and the speaker hang off VOUT-BAT**, the IP5306's output. A weak USB
  supply shows up as flicker, audio distortion or resets under backlight load before it
  looks like a power problem. Guition's burn notes ask for "over 600 mA".
- **Battery voltage without a battery is not zero.** With no cell on CN4 the IP5306 still
  drives BAT, and GPIO53 reads **~4.2 V** [H]. The reading cannot tell you a cell is present.
- **RS485 goes dead on battery power**: the MAX485 is on USB5V_IN, not VOUT-BAT.
- The backlight current is set by R66 3R9 ∥ R67 2R2 on the MP3202 FB pin.

---

## 5. Display [S][V][H]

### 5.1 The signal chain

| Step | Value | If you get it wrong |
|---|---|---|
| DSI PHY power | on-chip **LDO channel 3 @ 2500 mV** | `esp_lcd_new_dsi_bus` fails or the panel stays dark |
| DSI bus | 2 lanes, **750 Mbps** per lane (Guition BSP) | ESPHome's model uses 500 Mbps with a 34 MHz pixel clock — a second working set, untested here |
| Command IO | DBI, 8-bit cmd/param, virtual channel 0 | — |
| Video | DPI, RGB565, **28 MHz**; H 480 + 12 sync + 42 back + 42 front; V 800 + 2 sync + 8 back + **166** front ≈ 49.8 Hz | the registry macro `ST7701_480_360_PANEL_60HZ_DPI_CONFIG` is **480 × 360 @ 15 MHz** for another panel |
| Controller | ST7701S, **Guition's init table** (§5.2) | registry default table → **black screen with no error** [H] |
| Reset | **GPIO5**, active low | Guition's BSP has it only under the confusingly named `CONFIG_BSP_LCD_TYPE_1024_600` branch |
| Backlight | **GPIO23** → MP3202 EN; LEDC 5 kHz, 10-bit PWM dims it [H] | 10K pull-down: dark until driven |

`esp_lcd_st7701` reads the panel ID over DSI at init and always logs
`LCD ID: FF FF FF` on this board [H], with the vendor firmware too. It is harmless — DSI
read-back is not wired up on this panel — and it is **not** a sign the panel is missing.

### 5.2 Where the init sequence lives, and why that matters

Guition did not pass the panel's init sequence in `st7701_vendor_config_t.init_cmds`. They
**edited the `vendor_specific_init_default[]` table inside a vendored copy** of
`esp_lcd_st7701` 1.1.3 (`common_components/espressif__esp_lcd_st7701`), and the same edit is
in the Arduino examples' `src/lcd`. Their BSP then calls the driver with `init_cmds` unset.

Consequence: any project that takes Guition's *calling code* but pulls `esp_lcd_st7701` from
the component registry gets Espressif's default table, for a different glass. The DSI bus
comes up, every call returns `ESP_OK`, the backlight lights — and the screen is black.
Verified on hardware. The template passes the table explicitly (`src/display.c`); it matches
ESPHome's `JC4880P443` model byte for byte.

Guition's vendored copy also **redefined** `ST7701_480_360_PANEL_60HZ_DPI_CONFIG` to
480 × 800 @ 28 MHz. Upstream's macro of the same name is 480 × 360 @ 15 MHz. Never use the
macro; spell the timing out.

### 5.3 Drawing without LVGL

`esp_lcd_dpi_panel_get_frame_buffer(panel, 1, &fb)` returns the DPI driver's own PSRAM
framebuffer (`0x48000xxx` [H]). Write RGB565 into it, then call
`esp_lcd_panel_draw_bitmap(panel, 0, 0, 480, 800, fb)` **with that same pointer** — the
driver recognises its own buffer and only writes the cache back. Skip that call and the
panel shows stale pixels, because the CPU's writes are still in the L2 cache.

The panel is portrait, 480 wide. For landscape, rotate in software (LVGL `sw_rotate`, or
the PPA) — the ST7701 over DSI has no hardware swap-XY; Guition's firmware logs
`esp_lcd_panel_swap_xy(50): swap_xy is not supported by this panel` [H].

---

## 6. Touch — GT911 [S][V][H]

| | |
|---|---|
| Bus | the shared I2C, 400 kHz is fine |
| Address | a scan shows **both 0x5D and 0x14** acknowledging, with or without the reset sequence [H]. Use 0x5D |
| RST | **GPIO22** → FPC1 25 |
| INT | **GPIO21** → FPC1 28 |
| Coordinates | native 480 × 800, same orientation as the panel; no swap/mirror needed [H] |
| Identity | `TouchPad_ID:0x39,0x31,0x31` ("911"), config version 250 [H] |

Guition's BSP sets RST and INT to `GPIO_NUM_NC`. That works with `esp_lcd_touch_gt911`
because it then skips reset entirely. If you *do* give the driver RST/INT, you must also
set `esp_lcd_touch_config_t.driver_data` to an `esp_lcd_touch_io_gt911_config_t`:

- **with** `driver_data`: the driver holds INT, pulses RST, releases, waits 10 + 50 ms, reads
  the config — works [H].
- **without** it: "I2C address initialization procedure skipped", a bare 10 ms reset, then
  an immediate read → `GT911 read error!`, `ESP_ERR_INVALID_STATE` [H]. The chip is simply
  not ready 10 ms after reset.

---

## 7. I2C — one bus for everything [S][H]

SDA **GPIO7**, SCL **GPIO8**. Pull-ups: R65/R71 5K1 on the panel side **in parallel with**
R29/R31 2K2 on the camera side — about 1.5 kΩ to 3V3. Fast mode is comfortable.

| Address | Device | Seen on a bare board [H] |
|---|---|---|
| 0x18 | ES8311 codec (chip ID regs 0xFD/0xFE = `83 11`) | yes |
| 0x5D, 0x14 | GT911 touch | yes, both |
| 0x36 | OV02C10 camera (optional) | only with a camera fitted |

Also on the bus: CN3 and JP1 23/25. `i2c_new_master_bus()` logs
`W i2c.master: Please check pull-up resistances…` because the template leaves the internal
pull-ups off. The external ones are there; the warning is informational.

Open the bus **once** and hand the handle to the touch driver, the codec and your own
devices. Do not create a second bus on GPIO7/8 for a library that "wants its own" — two
controllers driving the same pads corrupt each other's transactions.

---

## 8. Audio [S][V]

| Signal | GPIO | Note |
|---|---|---|
| MCLK | 13 | |
| BCLK | 12 | schematic `CODEC_I2S0_SCLK` |
| LRCK | 10 | |
| **DOUT** (P4 → codec) | **9** | schematic `CODEC_I2S0_DSDIN` — named from the codec's side |
| **DIN** (codec → P4) | **48** | schematic `ES7210_SDOUT` — **there is no ES7210**; it is the ES8311's ASDOUT |
| PA enable | 11 | NS4150 `STD`, **active high**, R19 10K pull-down |

One codec, one mic, one speaker, one I2S port — so playback and recording share the port;
the ESPHome community notes that the mic cannot listen for a wake word while music plays.
Users also report a pop at the start and end of playback; raise GPIO11 only after the codec
is configured and the I2S clock is running, and drop it before stopping I2S.

**Not verified here** (no speaker was attached): only the codec's I2C presence and chip ID
were checked on hardware. The factory firmware does initialise the ES8311 in slave mode at
44.1 kHz / 16-bit stereo and plays MP3s [H, log only].

---

## 9. Silicon revision — the single most expensive thing to get wrong [D][H]

| Board label | esptool says | sdkconfig | Arduino core (Guition) |
|---|---|---|---|
| P4 V1.3 (this one) | `revision v1.3` | `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` + `CONFIG_ESP32P4_REV_MIN_100=y` | 3.2.1 |
| P4 V3.x (`P4_V3X`) | `revision v3.x` | `CONFIG_ESP32P4_REV_MIN_301=y` (the IDF 5.5 default) | 3.3.11; IDF demos for it are on 6.0.2 |

ESP-IDF 5.5 defaults to rev ≥ 3.1 and states that support for < 3.0 and ≥ 3.0 is mutually
exclusive. A rev-3 build on a v1.3 chip dies right after `entry 0x…` with
`Guru Meditation Error: Illegal instruction` — which looks like corrupt flash. Guition's
archive splits every firmware and demo by revision for this reason
(`P4-JC4880P443C_I_W_V2.3(P4_V3X).bin` vs the others).

Find out before building: `esptool --chip esp32p4 -p <port> flash-id`.

---

## 10. Storage — microSD [S][V]

SDMMC **slot 0** (IO MUX), 4-bit: CLK 43, CMD 44, D0-D3 39-42, 10K pull-ups in R2.
No card detect, no write protect. **Card VDD is on-chip LDO channel 4** through Q1, whose
gate is held on by R13 — so the card is unpowered until firmware acquires LDO 4, e.g.
`sd_pwr_ctrl_new_on_chip_ldo(&(sd_pwr_ctrl_ldo_config_t){.ldo_chan_id = 4}, …)` and
assigns it to `host.pwr_ctrl_handle`. Guition's instructions: FAT32, ≤ 32 GB.

With no card inserted, the factory firmware logs `sdmmc_init_ocr: send_op_cond (1)
returned 0x107` / `sdmmc_card_init failed (0x107)` [H] — ESP_ERR_TIMEOUT. That is what "no
card" looks like, and also what "LDO 4 not acquired" looks like.

**Not verified here** — no card was available.

---

## 11. Memory, clocks and measured numbers [H]

From the template's self-test on a rev v1.3 board:

| | |
|---|---|
| Core clock | 360 MHz configured, **360.0 MHz measured**; 40 MHz crystal |
| Internal RAM | 612 KB heap total, 557 KB free after boot (full template) |
| PSRAM | 32 768 KB mapped to the heap; DPI framebuffer at `0x48000fc0` |
| PSRAM bandwidth | 82.5 MB/s memset, 82.2 MB/s 32-bit scalar read (4 MB buffer) |
| SRAM bandwidth | 904 MB/s memset, 196 MB/s scalar read |
| CPU | 36.0 M LCG iterations/s |
| Die temperature | ~30 °C idle on the desk |
| Flash | 16 MB Boya, QIO 80 MHz once configured (§12) |

A full-screen RGB565 frame is 768 000 B. At ~80 MB/s a CPU full-frame fill costs about
10 ms; use the 2D-DMA / PPA (`use_dma2d = true` is set) or LVGL partial refresh for anything
animated.

---

## 12. Vendor material, and what is wrong with each piece

The archive is `JC4880P443C_I_W.zip` (≈ 478 MB) from Guition's download centre,
`http://pan.jczn1688.com/directlink/1/HMI%20display/JC4880P443C_I_W.zip`. That URL returns
an HTML page; the file itself is ten sha256-checked 48 MB shards listed at
`https://pan.jczn1688.com/api/manifest/HMI%20display/JC4880P443C_I_W.zip`, fetched with
header `X-Gdisk-Fetch: 1` and concatenated in `seq` order.

| Item | Content | Problem |
|---|---|---|
| `1-Demo/idf_examples/ESP-IDF_5.5.4` | Brookesia phone UI, LVGL 8/9, MJPEG video, RS485 echo/send, draw demo, xiaozhi | ST7701 init baked into a vendored driver's default (§5.2); BSP is Espressif's `esp32_p4_function_ev_board`, patched: `CONFIG_BSP_LCD_TYPE_1024_600` logs "Install EK79007 LCD control panel" and then creates an ST7701 480 × 800 |
| `1-Demo/idf_examples/ESP-IDF_6.0.2(P4_3X)` | same demos for rev v3.x silicon | — |
| `1-Demo/arduino_examples` (+ `P4_V3X_DEMO`) | adc_test, lvgl_demo_v8, lvgl_v9_sw_rotation, mp3_player, Wifi_scan | READMEs say the sample uses "Waveshare 10.1-DSI-TOUCH-A" — copy-paste from another product. `pins_config.h` sets `LCD_RST -1`, but `st7701_lcd.cpp` actually uses GPIO5 |
| `2-Specification` | 7-page spec | claims the main controller has "integrated Wi-Fi and Bluetooth" — the P4 has neither; the C6 does |
| `4-Driver_IC_Data_Sheet` | OV02C10 datasheet, **Chinese** ESP32-P4 datasheet v0.4.1 | no ST7701S, GT911 or ES8311 datasheets |
| `5-Schematic` | PDF + 7 PNG sheets, V1.0 dated 2025-09-27 | the only reliable source; §2 is transcribed from it |
| `6-User_Manual/Getting started…pdf` | Arduino IDE setup | written for the **JC1060P470** and says so on page 2; the IDE steps still apply |
| `8-Burn operation` | factory images, flash_download_tool 3.9.9 | `JC-C6-slave_v2.3.2.bin` is the C6 slave image; the board here shipped with slave **2.3.0** [H] |

Factory firmware on arrival [H]: `esp_brookesia_demo` 1.0.1, ESP-IDF v5.5.1, compiled
2026-03-10, 7 MB `factory` app at 0x20000 + 6 MB SPIFFS `storage`, NVS at 0xB000, its
bootloader enabling QIO. It persists settings in NVS (`wifi_en`, `brightness`, `volume`).
Back it up before the first flash: `esptool --chip esp32p4 -p <port> -b 921600 read-flash 0 0x1000000 factory.bin`.

---

# Part II — development guide

## 13. Toolchain

Verified: PlatformIO Core **6.2.0**, `platform =` pioarduino **55.03.311** (ESP-IDF
**5.5.5**, esptool 5.3.0), `board = esp32-p4-evboard`, `framework = espidf`.

- The official `platformio/espressif32` has no P4 support at any version:
  `Unknown board ID 'esp32-p4-evboard'`.
- **SCons conflict** (from the `esp32p4-jc-m3-dev` skill, not re-observed here because the
  host was already patched): Core 6.2.0 wants `tool-scons ~4.41101.0`, pioarduino
  55.03.311 pins `4.40801.0` and deletes the installed SCons mid-build →
  `ModuleNotFoundError: No module named 'SCons.Tool.FortranCommon'`. Fix:
  `sed -i.bak 's/"4.40801.0"/"4.41101.0"/' ~/.platformio/platforms/espressif32/platform.json`
  (confirm the version string first), or use pioarduino's own Core.
- **`board_build.flash_mode = qio` does not reach sdkconfig** under `framework = espidf`.
  The 2nd-stage bootloader switches to QIO, the app does not: the boot log says
  `spi_flash: flash io: dio` [H]. `CONFIG_ESPTOOLPY_FLASHMODE_QIO=y` fixes it
  (`flash io: qio`) [H].
- **Boya flash**: without `CONFIG_SPI_FLASH_SUPPORT_BOYA_CHIP=y` the app logs
  `Detected boya flash chip but using generic driver` and runs the generic driver [H].
- **PlatformIO ignores a component's `WHOLE_ARCHIVE` property.** Components that start
  themselves from `__attribute__((constructor))` — `esp_hosted` is the one that matters
  here — are silently dropped by the linker (§15).
- `board_build.partitions` in `platformio.ini` is what PlatformIO uses; the
  `CONFIG_PARTITION_TABLE_*` line in sdkconfig only keeps the config consistent. The
  template's `partitions_singleapp_large.csv` gives a 1 500 KB app — see `recipes.md` §10
  for a 16 MB layout.

## 14. Console, flashing and recovery [H]

- Plug into the **full-speed** USB-C. It enumerates `303A:1001` as `/dev/cu.usbmodem*`
  (macOS) or `/dev/ttyACM*` (Linux); no driver needed. `pio run -t upload -t monitor`
  resets into the ROM loader over USB and back — **no buttons needed**.
- The high-speed USB-C carries no console. There is no CH340 on this board.
- UART0 is on CN2 (MX1.25 4P) for a USB-TTL adapter [S]; not tried here.
- If upload fails (application wedged USB-Serial/JTAG, e.g. reconfigured GPIO24/25 or
  slept): **hold SW1 (BOOT) → tap SW2 (RST) → release SW1 → upload.**
- Bad firmware cannot brick the board; the ROM loader is mask ROM. Recovery:
  `esptool --chip esp32p4 -p <port> erase-flash`, then flash.
- Restore the factory image: `esptool --chip esp32p4 -p <port> -b 921600 write-flash 0 factory.bin`.
- Never run `espefuse`: burning `EFUSE_0PXA_TIEH_SEL_0` moves VDDO_FLASH to 1.8 V
  permanently.

## 15. Wi-Fi and Bluetooth — through the ESP32-C6 [V][H]

The P4 has no radio. The C6 inside the module runs **esp_hosted slave** firmware and the P4
talks to it over **SDIO slot 1, 4-bit, 40 MHz**: CLK 18, CMD 19, D0-D3 14-17, slave reset
GPIO54 **active high**. `esp_wifi_remote` re-exports the ordinary `esp_wifi_*` API on top.

What was observed on this board [H]:

| | |
|---|---|
| Slave firmware on the C6 | **esp_hosted 2.3.0**, capabilities 0xd (WLAN, BLE over HCI-SDIO) |
| Host that works | `espressif/esp_hosted` **2.11.7** + `espressif/esp_wifi_remote` **1.6.5** — the versions Guition's 5.5.4 demos pin (`2.11.*`, `*`) |
| Host that does not | `esp_hosted` 2.0.17 + `esp_wifi_remote` 0.14.5: link comes up, then `assert failed: netif_add … (netif already added)` |
| Version warning | `Version mismatch: Host [2.11.0] > Co-proc [2.3.0] ==> Upgrade co-proc to avoid RPC timeouts` — printed every boot; scan, connect and DHCP work regardless |
| C6 STA MAC | differs from the P4's base MAC (here `14:c1:9f:…` vs `80:f1:b2:…`) |

Four traps, each seen on hardware:

1. **Nothing starts under PlatformIO.** esp_hosted brings itself up from a constructor and
   relies on `WHOLE_ARCHIVE` to survive linking; PlatformIO drops it, so the first
   `esp_wifi_init()` logs `H_API: Transport not initialized, call esp_hosted_init() first`
   and `ESP-Hosted link not yet up`, then aborts. Call `esp_hosted_init()` and
   `esp_hosted_connect_to_slave()` yourself, before any `esp_wifi_*`.
2. **Main-task stack overflow** during that bring-up with the default 3.5 KB:
   `Stack protection fault` in task `main`. `CONFIG_ESP_MAIN_TASK_STACK_SIZE=10240`
   (Guition's value).
3. **The C6 has its own NVS and auto-connects.** Credentials entered in the factory UI live
   on the C6, survive erasing the P4, and the C6 associates by itself some time after
   `esp_wifi_start()` — the P4 gets a DHCP lease it never asked for. While that connect is
   in progress a scan is either **refused** with `ESP_ERR_WIFI_STATE` (RPC response code
   12294) or **started and aborted**: `SCAN_DONE` arrives ~20 ms later with `status` 1 and
   `get_ap_records()` returns `ESP_FAIL`. A single `esp_wifi_disconnect()` right after
   start loses the race; so does an empty STA config set before start. What works every
   time: disconnect → 500 ms → scan, and retry until `SCAN_DONE.status == 0` (the second
   attempt succeeded in each of four runs).
4. **`esp_wifi_scan_get_ap_records()` over the remote does not shrink `*number`** to the
   real count: asking for 20 returns 20, the tail zero-filled. Take the count from
   `esp_wifi_scan_get_ap_num()`.

Working code: `recipes.md` §9. Updating the C6 slave (to silence the mismatch or for BLE)
is done over JP1 pins 20/22/24/26 with a USB-TTL adapter, or via esp_hosted's OTA; the
archive's `JC-C6-slave_v2.3.2.bin` is Guition's image. Not attempted here.

---

## 16. Pitfall table

| Symptom | Cause | Fix |
|---|---|---|
| Bootloader dies after `entry 0x…` with `Illegal instruction` | built for rev ≥ 3, chip is v1.3 | §9 sdkconfig lines |
| `Unknown board ID 'esp32-p4-evboard'` | official PlatformIO platform | pin the pioarduino URL |
| `No module named 'SCons.Tool.FortranCommon'` | tool-scons version conflict | §13 |
| Boot log `flash io: dio` despite `flash_mode = qio` | PlatformIO option not in sdkconfig | `CONFIG_ESPTOOLPY_FLASHMODE_QIO=y` |
| `Detected boya flash chip but using generic driver` | Boya support off | `CONFIG_SPI_FLASH_SUPPORT_BOYA_CHIP=y` |
| Panel black, backlight on, every call `ESP_OK` | registry ST7701 default init table | pass Guition's `init_cmds` (§5.2) |
| Panel black, backlight off | GPIO23 never driven (10K pull-down) | LEDC or `gpio_set_level(23, 1)` |
| Panel garbled / wrong size | `ST7701_480_360_PANEL_60HZ_DPI_CONFIG` | explicit 480 × 800 timing (§5.1) |
| DSI bus create fails / dark panel, no error | LDO 3 not acquired | `esp_ldo_acquire_channel(3, 2500 mV)` first |
| Drawn pixels do not appear | framebuffer written, cache not flushed | `draw_bitmap` with the fb pointer (§5.3) |
| `LCD ID: FF FF FF` | normal on this panel | ignore |
| `swap_xy is not supported by this panel` | ST7701 DSI has no HW rotation | software / PPA rotation |
| `GT911 read error!` / `ESP_ERR_INVALID_STATE` | RST/INT given without `driver_data` | set `driver_data` (§6) or pass RST/INT = NC |
| I2C scan shows 0x14 **and** 0x5D | how this GT911 behaves | use 0x5D |
| `Please check pull-up resistances…` | internal pull-ups disabled, external present | ignore |
| TF mount `0x107` timeout | no card, or LDO 4 not acquired | §10 |
| Speaker silent, codec answers on I2C | GPIO11 low, or DOUT/DIN swapped | §8 |
| vbat reads ~4.2 V with no battery | IP5306 drives BAT anyway | not a presence test |
| `Transport not initialized, call esp_hosted_init() first` | constructor dropped by PlatformIO | call `esp_hosted_init()` (§15) |
| `Stack protection fault` in `main` during Wi-Fi init | 3.5 KB main stack | `CONFIG_ESP_MAIN_TASK_STACK_SIZE=10240` |
| `esp_wifi_scan_start` → `ESP_ERR_WIFI_STATE`, or `SCAN_DONE` in 20 ms then `ESP_FAIL` | C6 auto-connecting from its own NVS | disconnect + retry until `SCAN_DONE.status == 0` (`recipes.md` §9) |
| Scan says 20 APs, most are `ch 0, 0 dBm` | remote does not update `*number` | `esp_wifi_scan_get_ap_num()` |
| `netif already added` assert | esp_hosted 2.0.x host | esp_hosted 2.11.x |
| Board prints nothing, ROM banner absent | wrong USB-C (high-speed) or cable | use the full-speed socket |
| GPIO on JP1 "does nothing" after porting from M3-DEV | pin sets are swapped between the boards | §2.2 |
