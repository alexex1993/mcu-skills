---
name: esp32s3-rlcd42
description: Firmware development for the Waveshare ESP32-S3-RLCD-4.2 (SKU 33298 / 33507, and the -EN variant) — an ESP32-S3-WROOM-1-N16R8 AIoT board built around a 4.2" fully reflective 300x400 monochrome LCD on a Sitronix ST7305/ST7306, with no backlight, plus an ES8311 speaker codec, an ES7210 dual-microphone ADC, an SHTC3 temperature/humidity sensor, a PCF85063A RTC, a microSD/TF slot on SDMMC, an 18650 holder with battery sensing, BOOT/KEY/PWR buttons and a 2x8 2.54 mm expansion header. Use when working on this board: project setup, platformio.ini, the qio_opi PSRAM memory type, the pin map and which header pins are traps, U8g2 and the ST7305 constructor, a blank or shifted panel, ST7305 low-power mode HPM/LPM, the shared I2C bus and its four slaves, I2S audio directions and the speaker amplifier enable, mounting the TF card, battery voltage, USB CDC console with no UART bridge, flashing over Type-C, deep sleep, or debugging why something on the board does not work.
---

# Waveshare ESP32-S3-RLCD-4.2

A 4.2" reflective panel with **no backlight**, an audio front end, four I2C slaves and an
18650, on an N16R8 module — and **four free GPIOs on the header** when you are done.

Two things shape almost every task here. First, the panel is 1 bpp and reflective: a full
frame is 15,000 bytes, it holds its image with the SoC asleep, and it is invisible in the
dark. Second, the board has **no UART bridge chip and no user LED** — the single Type-C
socket is the S3's native USB, so if the console flags are wrong there is no fallback
route and no LED to blink. A board that "does nothing" is usually a board that is running
fine and cannot tell you.

- `reference/board-hardware.md` — the board: the vendor's "Interface Introduction" GPIO
  table transcribed **and corrected**, the full 2×8 header with the four trap positions,
  the display/audio/storage/power sections, console and recovery, flash layout, and a
  symptom → cause → fix table.
- `reference/esp32s3-soc.md` — the silicon: which GPIOs do not exist, strapping semantics,
  ADC1/ADC2 and touch maps, power-up glitches, memory map and PSRAM addressing, deep
  sleep and RTC wake pins.
- `reference/esp32-family.md` — the rest of the family, for "should this be a different
  ESP32?" questions.
- `reference/recipes.md` — code that builds: `platformio.ini`, USB console, ST7305
  bring-up and HPM/LPM, the I2C bus with SHTC3 and PCF85063A drivers, battery, SDMMC,
  the audio pin/parameter reference, deep sleep.
- `template/` — a **project that builds clean**, in two variants, plus a scaffold script.
  See `template/README.md`.

## Confirm the board first

Waveshare sells several 4.2" ESP32 boards whose pin maps do not port. Check two things:

| | This board | Not this board |
|---|---|---|
| Panel | **reflective, monochrome, no backlight**, 300×400, ST7305/ST7306 over SPI | ESP32-S3-Touch-LCD-4.3 (parallel RGB, colour, capacitive touch); any e-Paper board |
| Sockets | **one** Type-C, an 18650 holder on the back, a 2×8 header, a TF slot | boards with two USB-C, or a UART bridge chip near the socket |

If the vendor table you are reading mentions **touch** (`TP_SDA`, `TP_INT`) or a
**QMI8658C** IMU, that is not evidence this board has them — it does not, and the
schematic shows those connector pins unconnected. See rule 2.

## Orientation

| | |
|---|---|
| Module | ESP32-S3-WROOM-1-**N16R8**. ESP32-S3, Xtensa LX7 dual-core @ 240 MHz |
| Memory | **16 MB** quad SPI flash · **8 MB OCTAL** SPI PSRAM · 512 KB SRAM |
| Panel | 4.2" **reflective monochrome**, 300×400 native (400×300 under `U8G2_R1`), 1 bpp, **no backlight pin and none needed**. Full frame = **15,000 B** |
| Panel controller | **ST7305** — Waveshare's product spec, their own driver, and U8g2 all say so. Only the Zephyr port says ST7306 (register-compatible sibling); its 312/204 geometry does not transfer |
| Panel pins | SCLK **11** · MOSI **12** · CS **40** · DC/RS **5** · RST **41**. Write-only: **no MISO** |
| Panel speed | 24 MHz (GPIO matrix caps SPI2 at ~40 MHz; ST7305 wants ≥30 ns writes) · HPM `0x38` ≈32 Hz · LPM `0x39` ≈1 Hz · ~45 `sendBuffer()`/s |
| I2C | SDA **13** · SCL **14**, **one bus, four slaves**: ES8311 `0x18` · ES7210 `0x40`/`0x42` · PCF85063A `0x51` · SHTC3 `0x70` |
| Audio | MCLK **16** · BCLK **9** · LRCK **45** · DOUT **8** (→ speaker) · DIN **10** (← mics) · **PA enable 46, active high** |
| Storage | microSD on **SDMMC, 1-bit**: CLK **38** · CMD **21** · D0 **39**. **No CS** (`R7` to GPIO17 unpopulated), no card-detect |
| Buttons | **BOOT** = GPIO0 · **KEY** = GPIO18 · both active low with **external** 10 K pull-ups · **PWR** = power-latch IC, **not a GPIO, not readable** · **no EN/RST button** |
| LEDs | **none on a GPIO.** CHG and WRN are wired to the charger. No WS2812 |
| Battery | 18650, sensed on **GPIO4** (ADC1_CH3) through a **1/3** divider (200 K/100 K, 1%), 12 dB atten. 2.5→4.2 V per ESPHome; Waveshare's own firmware uses 3.0→4.12 V |
| USB | **one Type-C, native USB only** (GPIO19/20). No bridge chip. Console = USB CDC |
| Header | 2×8, 2.54 mm: `VBUS GND GP19 GP20 TXD RXD SDA SCL` / `3V3 GND GP0 GP1 GP2 GP3 GP17 GP18` |
| Free GPIO | on the header: **1, 2, 17**, plus **3** if you handle its floating strap, plus 43/44 (UART0). Unconnected but **not** on the header: 7, 42, 47, 48 |
| Unusable | **35, 36, 37** octal PSRAM, fatal to drive · **26-32** flash bus · **22-25** do not exist |
| Strapping | **0** (boot) · **3** (JTAG source, floats) · **45** (VDD_SPI — *also I2S LRCK*) · **46** (boot/ROM log — *also the amp enable*) |
| Toolchain | PlatformIO Core 6.2.0 + platform-espressif32 55.3.311 + Arduino core 3.3.11 + U8g2 **2.36.18** (the registry's newest; do **not** ask for `^2.36.19`), `board = esp32-s3-devkitc-1` |

## Rules that prevent the expensive mistakes

Each of these produces a failure that looks like something else.

1. **`-DARDUINO_USB_MODE=1` and `-DARDUINO_USB_CDC_ON_BOOT=1`, or nothing is ever
   printed.** There is **no UART bridge chip on this board** — the one Type-C socket is
   the S3's native USB. Without the flags `Serial` goes to UART0 on GPIO43/44, which
   reaches a header nobody has a cable on. The board enumerates, flashes, runs, and looks
   dead. There is no LED to check instead. Then wait for the host to enumerate before
   printing, or the boot banner is lost:
   `uint32_t t0 = millis(); while (!Serial && millis()-t0 < 2000) delay(10);`

2. **The wiki's "Interface Introduction" table has wrong rows; the schematic is the
   authority.** It lists `TP_INT` (GPIO7), `TP_SDA`/`TP_SCL` (13/14), `TP_RESET` (GPIO42)
   and a `QMI8658C` column. **This board has no touch panel and no IMU.** The touch signals
   are real *pins on the LCD's 21-way FPC connector* — `TP_RESET`, `TP_INT`, `TP_SDA`,
   `TP_SCL` on pins 16-19 — and **all four are marked unconnected**; the footprint merely
   supports a touch variant of the panel that this board does not fit. Search the schematic
   for nets `GPIO7` and `GPIO42` and they appear *only* on the module symbol.

   So **GPIO7, GPIO42, GPIO47 and GPIO48 are genuinely free** — but none of them reaches
   the expansion header, so using one means soldering to a module pad. Note also that the
   schematic PDF carries its own copy of this table in which those cells are *blank*: the
   two vendor copies disagree with each other. `board-hardware.md` §2.2 has the full diff.

3. **The TF slot is SDMMC, not SPI — there is no CS pin, and its absence is not an
   omission.** The card's `CD/D3` has a 10 K pull-up and reaches GPIO17 only through `R7`,
   which is **not populated**, so the card comes up in native SD mode; `D1` and `D2` are
   unconnected at the socket, and card-detect is tied to ground. Waveshare's own BSP is
   `CustomSDPort(name, clk = 38, cmd = 21, d0 = 39, width = 1)` over
   `esp_vfs_fat_sdmmc_mount()`. `SD.h` on a `SPI` bus cannot work whatever CS you invent.
   In Arduino use `SD_MMC.setPins(38, 21, 39)` **before** `SD_MMC.begin()` — called after,
   `setPins()` returns true, changes nothing, and `begin()` falls back to the S3 defaults,
   which overlap the octal PSRAM pins of rule 4 and boot-loop the board. Pass
   `mode1bit = true`: 4-bit mode cannot work at any speed.

4. **`board_build.arduino.memory_type = qio_opi`, or the 8 MB PSRAM is never mapped.**
   `esp32-s3-devkitc-1` is an **N8 with no PSRAM** in PlatformIO's database and defaults to
   `qio_qspi`. Nothing reports the mismatch: `psramFound()` simply returns false and large
   allocations fail later, somewhere else. Check `psramFound()` first, always. GPIO35, 36
   and 37 are that PSRAM — driving one corrupts the bus the CPU reads its cached data
   from, and the board panics with `Cache disabled but cached memory region accessed`,
   pointing anywhere but the pin you touched.

5. **`SPI.begin(11, -1, 12, -1)` before `u8g2.begin()`, or the panel stays blank with no
   error.** U8g2's hardware-SPI constructor takes CS, DC and RST but never assigns SCLK
   and MOSI — it inherits whatever the global `SPI` object has, and the ESP32-S3 defaults
   are not this board's pins. Claiming the bus first makes U8g2's own `SPI.begin()` a
   no-op. This is the single most common "the display does not work" cause, and it
   produces no message anywhere.

6. **GPIO45 and GPIO46 are strapping pins that this board also uses for audio.** GPIO45 is
   `VDD_SPI`: high at reset selects a 1.8 V flash rail and the module stops reading its
   own flash — a board that looks bricked and is fine as soon as the pull comes off. It is
   also I2S LRCK. GPIO46 is a boot/ROM-log strap and also the speaker amplifier enable;
   the board's own `R49` 10 K pull-down holds it low through reset, which is exactly what
   the strap wants. The ESP drives both itself, so ordinary use is safe; the trap is adding
   an external pull-up, or wiring a peripheral that idles either line high.

7. **`Wire.begin()` with no arguments lands on GPIO8/9 — the I2S data and bit clock.**
   The S3 Arduino variant defaults `Wire` to SDA 8, SCL 9. The symptom is not an I2C
   error, it is a scan that finds nothing while the codecs are sitting right there.
   Always `Wire.begin(13, 14)`.

8. **The I2C bus is not yours.** Four slaves already answer at `0x18`, `0x40`/`0x42`,
   `0x51` and `0x70`, and the bus already has its pull-ups. An external device on the
   header's `SDA`/`SCL` must avoid those five addresses. Scan before you assume — the
   ES7210 is at `0x40` on most batches and `0x42` on some.

9. **`setPowerSave(1)` is not the ST7305 low-power mode.** It sends `0x28`, display-off,
   and the image disappears. LPM is `sendF("c", 0x39)` and HPM is `sendF("c", 0x38)`, both
   followed by a settling delay; U8g2 wraps neither. Writing while in LPM is allowed but
   the update can take a full refresh period (~1 s) to appear — so a fast redraw is
   HPM → draw → `sendBuffer()` → LPM.

10. **I2S DOUT is GPIO8 and DIN is GPIO10, from the host's point of view.** The vendor
    table names them from the *codec's* side (`DSDIN` into the codec, `ASDOUT` out of it),
    which reads backwards. The schematic is unambiguous: GPIO8 lands on ES8311 pin 9
    `DSDIN`, and ES7210 pin 11 `SDOUT1` reaches GPIO10 through `R42` (51 R). **The Zephyr
    pinctrl has them genuinely swapped** and its `i2s0` node is never enabled, so that
    source is untested — do not copy it. Getting this wrong gives you a silent speaker
    and a microphone that captures nothing, with both chips answering normally on I2C —
    which is what one user reported hitting before fixing it.

11. **GPIO46 low means silence, whatever else is correct.** The NS4150 amplifier's `CTRL`
    is active high and held low by a 10 K pull-down, so it is muted until firmware raises
    it. First thing to check when the ES8311 answers on I2C and nothing comes out. Raise
    it *before* initialising the codec.

12. **Four of the sixteen header positions are traps, not expansion.** `GP19`/`GP20` are
    the native USB D−/D+: using them removes the console, the flashing route and
    USB-Serial-JTAG at once, and there is no second port. `GP0` is the BOOT button —
    anything holding it low at reset puts the board in download mode, which presents as
    "my firmware stopped running". `GP18` is the KEY button with the button still fitted
    in parallel. `SDA`/`SCL` are the shared bus of rule 8.

13. **Set 12 dB ADC attenuation on GPIO4 or the battery reads a stuck maximum.** A full
    18650 through the 1/3 divider presents ~1.40 V, well above the S3 ADC's default
    ~0.95 V full scale. `analogSetPinAttenuation(4, ADC_11db)`, then multiply the reading
    by 3. GPIO4 is ADC1_CH3, so it keeps working with Wi-Fi up — ADC2 does not.

14. **The RTC ships with no cell, so its oscillator-stop flag is set out of the box.**
    Bit 7 of the PCF85063A's Seconds register means "the time is not trustworthy", and on
    a new board it will be. That is not a fault: it is an empty PH1.0 holder. Treat it as
    "sync from NTP now". Only **rechargeable** cells belong in that holder — the board
    charges it, and a primary cell there can vent.

15. **There is no EN/RST button** — `CHIP_PU` carries only an RC network, no switch. The
    manual download sequence is **hold BOOT → tap PWR (or replug USB) → release BOOT →
    upload**, not the BOOT/EN dance printed in every other ESP32 tutorial. PWR is `Key3`
    on a power-latch IC: long press off, single click on, and firmware cannot read it.

16. **Do not use `huge_app.csv`.** Its app slot is 3 MB and its coredump sits at
    `0x3F0000`, so everything above 4 MB of a 16 MB part is silently stranded.
    `default_16MB.csv` is the board's sane default (dual OTA, 6.25 MB per slot, 3.375 MB
    SPIFFS); drop to a single-app table if you want the 6 MB back.

## When the task is the display

Start from `template/src/display.cpp` — it is a working bring-up for this exact panel.

The mechanism worth carrying: this is a **1-bit-per-pixel reflective panel with its own
self-refresh**. The host writes a 15,000-byte frame into the controller's memory; the
controller then holds and refreshes that image on its own, at 32 Hz in HPM or 1 Hz in LPM,
with no further host involvement. So:

- **U8g2 is the right library**, and the full-frame `_F_` buffer is affordable — 15 KB in
  internal SRAM, no `firstPage()`/`nextPage()` loop, no PSRAM needed. The constructor is
  `U8G2_ST7305_300X400_F_4W_HW_SPI(U8G2_R1, 40, 5, 41)`. **Ask for `^2.36.18`, not
  `^2.36.19`.** Waveshare's docs say "v2.36.19 or later" and upstream's ChangeLog does list
  ST7305 300×400 under 2.36.19 — but the **PlatformIO registry's newest U8g2 is 2.36.18**,
  and that package already carries the constructors (verified by compiling against it).
  Asking for `^2.36.19` in `lib_deps` simply fails to resolve. `U8G2_R1` gives the 400×300
  landscape the board is read in; `U8G2_R3` if it comes out upside down in your enclosure.
- **Redrawing on a timer is usually wrong.** The image costs nothing to *hold*, only to
  *write*. Redraw when the content changes. A dashboard that updates once a minute is a
  reasonable default; ESPHome's own config for this board ships `update_interval: 1min`.
- **~45 host frames/s at 24 MHz** is the ceiling, and the panel still self-refreshes at
  32 Hz, so writing faster than that changes nothing you can see.
- **It is invisible in the dark and that is not a fault.** No backlight pin exists. "The
  display is dim" is answered with ambient light, not with code.
- **Do not port the Zephyr numbers.** That port drives the same panel as an ST7306 with
  `width = 312` and `start-column = 204`. Those are not arbitrary — the ST7305 addresses
  memory in 12-pixel blocks, so 300 pads to 312 — but they belong to that driver; feeding
  them to an ST7305 driver gives a shifted image. Follow Waveshare's ST7305 route.
- **Waveshare ships a working driver of their own**, `10_U8G2_Test/ST7305_U8g2.cpp` in
  `github.com/waveshareteam/ESP32-S3-RLCD-4.2`, alongside ESP-IDF, ESPHome, LVGL and
  XiaoZhi examples and prebuilt firmware. Read it before hand-writing an init sequence.

## When the task is choosing a pin

Count first. With the panel, the audio, the I2C bus, the card and the battery all wired —
which is the board as sold — the free list is:

| GPIO | Note |
|---|---|
| 1, 2 | ADC1_CH0/CH1, TOUCH1/2, RTC-capable. The two clean pins on the header |
| 17 | ADC2_CH6 — dead while Wi-Fi is up. Otherwise clean; carries the unpopulated `R7` to the card's CS |
| 3 | free, but the **JTAG-source strap and it floats** — give it a defined level at reset |
| 43, 44 | UART0. Free because the console is on USB (rule 1) and must stay there |

Then the ones you buy by giving something up: **38/21/39** cost you the card,
**8/9/10/16/45/46** cost you audio, **11/12/40/41/5** cost you the panel (**6** is the
panel's TE line and is taken even though nothing reads it), **19/20** cost you the only USB
port. **35/36/37 are never available** (rule 4).

**GPIO7, 42, 47 and 48 are connected to nothing** (rule 2) but do not appear on the header,
so they are reachable only by soldering to a module pad. Count them for a respin, not for a
jumper wire.

If four is not enough, the answer is an **I2C expander on the header's SDA/SCL** at an
address outside `0x18`/`0x40`/`0x42`/`0x51`/`0x70` — not one of the PSRAM pins.

## When the task is "the board does not work"

Diagnose in this order; each step only depends on the ones before it.

1. **Does anything print?** If not, this is rule 1 nine times out of ten — check the two
   USB build flags before anything else. The port is `/dev/cu.usbmodem*`, `/dev/ttyACM*`
   or a `COM` port, VID `303A`. There is no second port and no LED to fall back on.
2. **Does the ROM banner (`ESP-ROM:esp32s3-...`) appear?** It comes out before any of
   your configuration applies. No banner ⇒ cable, port, or the board is held in download
   mode by something on `GP0` (rule 12).
3. **Does `psramFound()` return true?** If not, stop: rule 4. Everything downstream is a
   consequence.
4. **Is anything wired to GPIO35/36/37, GP0, GP19/GP20 or GPIO45?** Unplug it (rules 4,
   6, 12).
5. **Does it boot-loop?** Read `esp_reset_reason()`. `ESP_RST_BROWNOUT` ⇒ thin cable or a
   tired 18650, and check the WRN LED for a reversed cell. `Cache disabled but cached
   memory region accessed` ⇒ rule 4.
6. **Panel blank?** In order: `SPI.begin()` before `u8g2.begin()` (rule 5), then U8g2
   version ≥ 2.36.18, then `setPowerSave` (rule 9), then ambient light.
7. **I2C scan empty?** Rule 7 — the pins. If some addresses answer and one does not, it is
   that chip, not the bus.
8. **Audio silent?** GPIO46 first (rule 11), then the DOUT/DIN direction (rule 10).
9. **Card will not mount?** Rule 3, in that order: SDMMC not SPI, `setPins()` before
   `begin()`, `mode1bit = true`, then FAT32, then another card.
10. **Still lost?** Flash `--minimal`: it touches only the toolchain, the flashing route,
    the console and the PSRAM mapping, so it separates "wrong build flags" from
    "wrong peripheral code" without guessing.

`template/src/board_report.cpp` prints the PSRAM state, flash size, reset reason, button
levels and battery voltage, which answers most of steps 3-5 in one paste.

## Starting a new project

Do not hand-assemble one — the `platformio.ini` has five settings that are wrong by
default and silent when wrong. Scaffold from `template/`:

```sh
~/.claude/skills/esp32s3-rlcd42/template/variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir> && pio run
```

- `--minimal` — USB CDC console heartbeat, PSRAM check, button levels. **342,508 B flash,
  24,256 B RAM.** Flash this first on a board you have not used before: on a board with no
  LED it is the only way to separate "wrong USB flags" from "wrong peripheral code".
- `--full` (default) — board self-test: report, I2C scan, SHTC3, PCF85063A, battery, and a
  dashboard on the panel with KEY to redraw. **372,904 B flash, 39,928 B RAM.**

Both build clean with zero warnings at the toolchain versions in the orientation table.
Nothing is generated and no paths are embedded, so copying `template/` by hand works
identically. `template/README.md` maps files to subsystems.

The TF card and the audio codecs are deliberately **not** in the template — see
`reference/recipes.md` §6 and §7, which say plainly which parts are untested.

When the user already has a project, prefer bringing their `platformio.ini` and pin header
in line with the template over rewriting their sketch.

ESP-IDF is a reasonable alternative and every pin rule above applies unchanged; for audio,
Espressif's `esp_codec_dev` component ships the ES8311 and ES7210 drivers, and ESPHome
(framework `esp-idf`) is the path with the most published evidence behind it for this
board.

## Flashing

```sh
pio run -t upload -t monitor
```

One Type-C socket, native USB, no bridge chip. It enumerates as `/dev/cu.usbmodem*`
(macOS), `/dev/ttyACM*` (Linux) or a `COM` port (Windows), VID `303A`.

When upload fails with `Wrong boot mode detected` or `No serial data received`, check
`GP0` before the cable (rule 12). Then the manual sequence — **note that there is no
EN/RST button on this board**:

> **hold BOOT → tap PWR (or replug the USB cable) → release BOOT → upload**

**Bad firmware cannot brick this board.** The ROM bootloader is mask ROM and always
answers that sequence:

```sh
esptool.py --chip esp32s3 -p <port> flash_id      # confirms 16 MB
esptool.py --chip esp32s3 -p <port> erase_flash
```

The one genuinely unrecoverable mistake is burning the `VDD_SPI` eFuse to 1.8 V on a 3.3 V
module. eFuses are one-time. Do not run `espefuse.py` on this board.

Physical handling, from the vendor's own warning: **do not push on the screen** when
inserting the Type-C cable or an 18650 — support the board by its frame. Screen cracks
from that are explicitly outside the warranty.

## Reporting

Say what is verified and what is derived. This skill was written from the board's schematic,
the vendor's own source code, and a build — not from a board on a desk.

**Schematic-derived, high confidence.** Every pin in `board-hardware.md` §2.3 is traced to
`ESP32-S3-RLCD-4.2-schematic.pdf` (`files.waveshare.com/wiki/ESP32-S3-RLCD-4.2/`). That
includes the facts most likely to be got wrong elsewhere: the SD slot is SDMMC with `R7`
(its would-be CS to GPIO17) unpopulated and card-detect grounded; `LCD_TE` really is GPIO6;
GPIO7/42/47/48 are connected to nothing; the battery divider is 200 K/100 K 1%; the amp
enable has a 10 K pull-down; and `CHIP_PU` has no reset button on it.

**Vendor code, high confidence.** `github.com/waveshareteam/ESP32-S3-RLCD-4.2` is
Waveshare's own repo for this board (Arduino, ESP-IDF, ESPHome and XiaoZhi examples plus
prebuilt firmware). The SD pins and 1-bit width, the ADC unit/channel/attenuation and ×3
factor, the 24 MHz SPI clock, the `SPI.begin(sck, -1, mosi, -1)` call, and `0x38` as the
end of the display init all come from code in that repo. Their peripheral tutorial supplies
the HPM/LPM behaviour and the ~45 fps figure.

**Corroborated by a third party.** The audio pin directions, ES8311 `0x18` and ES7210 `0x40`
are confirmed by a Home Assistant community user working with the board in hand — who
reports having had DOUT/DIN backwards at first, which is rule 10.

**Primary-source.** The strapping tables, the GPIO35/36/37 octal-PSRAM restriction, the
ADC1/ADC2 maps and the power-up glitch list in `reference/esp32s3-soc.md` are from the
Espressif ESP32-S3 and ESP32-S3-WROOM-1 datasheets.

**Known contradictions between sources — quote them as contradictions, not as facts:**
- **The wiki's GPIO table is wrong** about `TP_INT`/`TP_RESET`/`TP_SDA`/`TP_SCL` and carries
  a stray `QMI8658C` column. The schematic PDF's own copy of the same table has those cells
  blank. Rule 2.
- **ST7305 vs ST7306.** Waveshare's product spec, their driver, their datasheet copy and
  U8g2 all say ST7305; only the Zephyr board port says ST7306. Use ST7305.
- **U8g2 version.** Waveshare says "2.36.19 or later"; the PlatformIO registry stops at
  2.36.18, which already has the constructors. Pin `^2.36.18` on PlatformIO.
- **Battery curve.** ESPHome tutorial 2.5–4.2 V; Waveshare's own firmware 3.0–4.12 V.
- **RTC backup connector.** The product page says PH1.0; the schematic symbol says SH1.0.
  Tell the user to measure before ordering a pigtail.

**Verified in this session:** both template variants build clean, zero warnings, with
PlatformIO Core 6.2.0 / platform-espressif32 55.3.311 / Arduino core 3.3.11 / U8g2 2.36.18,
at the flash and RAM figures quoted above.

**Not verified:** nothing was flashed or run. No panel was lit, no card mounted, no voltage
measured, no frame rate observed, and no audio played — the ES8311/ES7210 register
sequences are deliberately absent from `template/` for that reason. Say so rather than
presenting any of it as certain.
