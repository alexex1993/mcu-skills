---
name: esp32p4-jc4880p443
description: Firmware development for the Guition JC4880P443C_I_W (JC4880P443, JC4880P443C, often mistyped JC4880P433) — a 4.3" 480x800 IPS touch display board on the JC-ESP32P4-M3 module (ESP32-P4NRW32, 32 MB PSRAM, 16 MB flash, ESP32-C6 Wi-Fi co-processor on SDIO) — its ST7701S MIPI-DSI panel, GT911 touch, backlight, ES8311 codec with speaker amp and mic, microSD, RS485, battery charger, 2x13 header, and the PlatformIO + ESP-IDF (pioarduino) setup around them. Use when working on this board: project setup, platformio.ini and sdkconfig.defaults, the P4 silicon revision and an "Illegal instruction" boot, a black DSI screen, ST7701 init sequence and timings, GT911 init errors, backlight PWM, choosing a free GPIO, Wi-Fi through esp_hosted and esp_wifi_remote, SD card, audio, flashing over USB-C, or debugging why something on the board does not work.
---

# Guition JC4880P443C_I_W (ESP32-P4, 4.3" 480×800)

A 4.3" portrait IPS panel with capacitive touch, bolted to the same **JC-ESP32P4-M3** module
Guition uses on its P4 dev boards: an ESP32-P4 with no radio, plus an ESP32-C6 that does the
Wi-Fi over SDIO.

Three things shape almost every task on this board. First, **the panel only works with
Guition's ST7701 init sequence, and Guition hid it** inside a patched copy of the driver:
take their calling code with the registry driver and you get a black screen with every
call returning `ESP_OK`. Second, **the silicon comes in two incompatible revisions** and
ESP-IDF defaults to the one this board does not have — the symptom is a bootloader that
dies with `Illegal instruction`. Third, **Wi-Fi is a second chip with its own firmware and
its own NVS**, and under PlatformIO its host driver does not even start by itself.

- `reference/board-hardware.md` — the board: every module pad and where it goes, the GPIO
  budget, JP1, connectors, power tree, display chain and why the init table matters, touch,
  I2C, audio, SD, the silicon-revision table, measured memory/clock figures, the vendor
  archive and what is wrong in it, toolchain traps, flashing and recovery, Wi-Fi through the
  C6, and a symptom → cause → fix table.
- `reference/recipes.md` — code: `platformio.ini`, `sdkconfig.defaults`, the DSI panel with
  its full init table, framebuffer drawing, backlight, GT911, the I2C bus, ES8311 ID,
  battery, Wi-Fi scan through the C6, a 16 MB partition table, microSD, ES8311 audio + mic,
  Arduino settings. Each says whether it ran on hardware.
- `reference/esp32p4-soc.md` — the silicon: GPIO inventory, strapping, ADC, the three USB
  controllers, MIPI/SDMMC/LDO blocks. Shared with the `esp32p4-jc-m3-dev` skill.
- `reference/esp32-family.md` — the rest of the ESP32 line, for "should this be a different
  chip?" questions.
- `template/` — a project that **builds clean and has run on the board**, in two variants,
  plus a scaffold script. See `template/README.md`.

## Confirm the board first

| | This board | Not this board |
|---|---|---|
| Label | back silkscreen `JC4880P443`; module lid `JC-ESP32P4-M3 · GUITION · ESP32P4NRW32` | Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3 — also ST7701 + GT911 at 480 × 800, but a different board with its own pinout; do not mix their code |
| Screen | 4.3", **480 × 800 portrait**, 117 × 69 mm module | JC1060P470 (7", 1024×600), JC8012P4A1 (10.1", 800×1280) |
| Ports | **two** USB-C (full-speed + high-speed), 2×13 header, MX1.25 plugs for speaker, battery, UART0, UART1, RS485 | JC-ESP32P4-M3-DEV: three USB-C, RJ45, no panel |

Then find the **silicon revision**, because it changes the build:

```sh
esptool --chip esp32p4 -p <port> flash-id   # "Chip type: ESP32-P4 (revision v1.3)" and 16MB
```

Everything below assumes **rev v1.3** and says where rev ≥ 3.0 differs.

## Orientation

| | |
|---|---|
| MCU | ESP32-P4NRW32, 2 × RISC-V HP @ **360 MHz** (rev v1.3; 400 on rev ≥ 3) + LP core, 40 MHz crystal |
| Memory | **16 MB** Boya QSPI flash (JEDEC `0x684018`) · **32 MB** HEX PSRAM @ 200 MHz · 768 KB L2MEM |
| Panel | **ST7701S**, 2-lane MIPI-DSI @ 750 Mbps, DPI **28 MHz**, RGB565, 480 × 800 portrait. PHY power = **on-chip LDO 3 @ 2500 mV**. Reset **GPIO5** |
| Backlight | **GPIO23** → MP3202 boost enable, **10K pull-down** (dark until driven), LEDC PWM dims it |
| Touch | **GT911** on the shared I2C, answers at **0x5D and 0x14**; RST **GPIO22**, INT **GPIO21** |
| I2C | **one bus**: SDA **7**, SCL **8**, ~1.5K pull-ups on board. ES8311 `0x18`, GT911, camera SCCB `0x36`, CN3, JP1 23/25 |
| Audio | ES8311: MCLK 13 · BCLK 12 · LRCK 10 · **DOUT 9** · **DIN 48** · amp enable **GPIO11 active high** (10K pull-down). Speaker is external on CN1 |
| Storage | microSD on **SDMMC slot 0**, 4-bit: CLK 43, CMD 44, D0-D3 39-42; card VDD = **on-chip LDO 4** |
| Radio | **none on the P4.** ESP32-C6 on SDIO slot 1: CLK 18, CMD 19, D0-D3 14-17, reset **GPIO54 active high**. Ships with esp_hosted slave **2.3.0** |
| Free GPIO | **28, 29, 30, 31, 32, 33, 34, 49, 50, 51, 52** on JP1 (+35 = BOOT strap). GPIO1-4, 20, 45-47 are **not routed** |
| Buttons | SW1 BOOT = **GPIO35**, active low · SW2 = reset · SW3 = IP5306 power key |
| LEDs | **none on a GPIO** — D1 is the 3V3 power LED |
| UARTs | UART0 37/38 on CN2 · UART1 26/27 on J5 + MAX485 (auto-direction) on J4 |
| Battery | IP5306; sense **GPIO53** (ADC2_CH4) via 68K/100K, × 1.68 |
| USB | full-speed USB-C = **USB-Serial/JTAG `303A:1001`** (flash + console) · high-speed USB-C = OTG. **No USB-UART chip** |
| Toolchain | PlatformIO Core 6.2.0 + **pioarduino 55.03.311** (ESP-IDF **5.5.5**) + `board = esp32-p4-evboard` + `framework = espidf` |

## Rules that prevent the expensive mistakes

Each of these produces a failure that looks like something else.

1. **Pass Guition's ST7701 init table explicitly in `st7701_vendor_config_t.init_cmds`.**
   Guition patched it into the *default* table of a vendored `esp_lcd_st7701` and then
   called the driver with `init_cmds` unset. Copy their calling code with the registry
   driver and the DSI bus comes up, every call returns `ESP_OK`, the backlight is on — and
   the screen stays **black** (tested). The table is in `recipes.md` §4 and
   `template/src/display.c`.

2. **Never use `ST7701_480_360_PANEL_60HZ_DPI_CONFIG`.** Upstream it is 480 × **360** @ 15 MHz;
   Guition's vendored copy silently redefines the same name as 480 × 800 @ 28 MHz. Spell out
   the timing: 28 MHz, H 12/42/42, V 2/8/**166** (sync/back/front), 2 lanes @ 750 Mbps.

3. **`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` + `CONFIG_ESP32P4_REV_MIN_100=y`, or a v1.3
   board does not boot.** ESP-IDF 5.5 defaults to rev ≥ 3.1 and the two are mutually
   exclusive; the 2nd-stage bootloader dies right after `entry 0x…` with
   `Guru Meditation Error: Illegal instruction`, which looks like corrupt flash. Rev ≥ 3.0
   boards (Guition's `P4_V3X`) need `CONFIG_ESP32P4_REV_MIN_301=y` instead.

4. **Acquire on-chip LDO channel 3 at 2500 mV before any DSI call, and drive GPIO23 after
   the first frame.** LDO 3 powers the DSI PHY. GPIO23 has a 10K pull-down: an initialised,
   drawing panel with the backlight never enabled is just as black as a broken one.

5. **Write the framebuffer, then call `esp_lcd_panel_draw_bitmap()` with the framebuffer's
   own pointer.** The DPI framebuffer is in PSRAM behind the L2 cache; that call is what
   writes the cache back. Without it the panel shows stale pixels. `LCD ID: FF FF FF` in the
   log is normal for this panel, and there is no hardware swap-XY — rotate in software.

6. **Giving the GT911 driver RST/INT requires `driver_data` as well.** RST is GPIO22 and INT
   GPIO21, both wired even though Guition's BSP says `NC`. With RST/INT but no
   `.driver_data = &(esp_lcd_touch_io_gt911_config_t){.dev_addr = 0x5D}` the driver does a
   bare 10 ms reset and reads immediately → `GT911 read error!`, `ESP_ERR_INVALID_STATE`.
   With it, the driver waits 60 ms and works. Seeing both 0x5D and 0x14 in a scan is normal.

7. **`platform = espressif32` cannot build for this board — pin the pioarduino URL.** The
   official platform has no P4 at any version: `Unknown board ID 'esp32-p4-evboard'`. If the
   link then fails with `No module named 'SCons.Tool.FortranCommon'`, that is PlatformIO
   Core 6.2.0 vs. pioarduino's tool-scons pin — `board-hardware.md` §13.

8. **`board_build.flash_mode = qio` does not reach the application.** Under
   `framework = espidf` the app still runs its flash in DIO (`spi_flash: flash io: dio` in
   the boot log). Put `CONFIG_ESPTOOLPY_FLASHMODE_QIO=y` in `sdkconfig.defaults`, and
   `CONFIG_SPI_FLASH_SUPPORT_BOYA_CHIP=y` for the Boya part. Likewise the partition table
   comes from `board_build.partitions`, not from `CONFIG_PARTITION_TABLE_*`.

9. **Under PlatformIO, esp_hosted never starts on its own — call `esp_hosted_init()` and
   `esp_hosted_connect_to_slave()` before any `esp_wifi_*`.** esp_hosted brings itself up
   from a constructor kept alive by `WHOLE_ARCHIVE`; PlatformIO ignores that property, the
   linker drops it, and `esp_wifi_init()` aborts with `Transport not initialized, call
   esp_hosted_init() first`. Also raise `CONFIG_ESP_MAIN_TASK_STACK_SIZE` to **10240**, or
   the bring-up dies with `Stack protection fault` in `main`.

10. **The C6 keeps its own Wi-Fi credentials and auto-connects.** Networks saved in the
    factory UI live in the C6's flash, survive erasing the P4, and the C6 joins them some
    time after `esp_wifi_start()`. Meanwhile scans are refused (`ESP_ERR_WIFI_STATE`) or
    aborted (`SCAN_DONE` status 1, then `ESP_FAIL`). Disconnect, wait, retry until
    `SCAN_DONE.status == 0`, and take the count from `esp_wifi_scan_get_ap_num()` —
    `get_ap_records()` over the remote does not shrink `*number`. Use host `esp_hosted`
    **2.11.x**; 2.0.x asserts `netif already added`. Working code: `recipes.md` §9.

11. **The JP1 GPIOs are not the ones on the JC-ESP32P4-M3-DEV, even though the module is
    the same.** Here 28-34 and 49-52 are free and 1-5, 20, 45-47 go nowhere; on the DEV
    board it is the other way round (those are Ethernet there). The RMII names printed next
    to JP1 are inherited labels — this board has no Ethernet. Code ported from the DEV skill
    drives nothing.

12. **JP1 pins 20, 22, 24, 26 belong to the ESP32-C6** (its UART0, IO9 strap and enable).
    Driving them drops Wi-Fi; pin 26 low holds the C6 in reset. **JP1 pin 15 is GPIO35, the
    boot strap**: anything holding it low through reset looks like "the firmware stopped
    running".

13. **Open the I2C bus on GPIO7/8 once and share the handle.** The codec, the touch
    controller, the camera and anything on CN3/JP1 are all on it. The
    `Please check pull-up resistances` warning is expected — the pull-ups are external.

14. **For audio: DOUT is GPIO9, DIN is GPIO48, the amp enable GPIO11 is active high, and
    `ES8311_CODEC_DEFAULT_ADDR` is 0x30.** The schematic names I2S nets from the codec's
    side and calls GPIO48 `ES7210_SDOUT` though no ES7210 exists. `esp_codec_dev` wants the
    8-bit address 0x30, not the 0x18 a scan shows. The mic arrives in the **left** channel.

15. **The microSD card is powered by on-chip LDO 4.** Without
    `sd_pwr_ctrl_new_on_chip_ldo(.ldo_chan_id = 4)` on the host the card is unpowered and the
    mount fails with `0x107` — identical to "no card inserted". Slot 0 is IO MUX: no pins in
    the slot config, no CD, no WP.

16. **The battery reading proves nothing about a battery.** With no cell on CN4 the IP5306
    still drives BAT and GPIO53 reads ~4.2 V. And RS485 is powered from USB/VIN only — it is
    dead on battery.

## When the task is the display

Order, each step depending on the previous one (`template/src/display.c` is the working
code): LEDC on GPIO23 at 0 % → LDO 3 @ 2500 mV → DSI bus (2 lanes, 750 Mbps) → DBI IO →
DPI config with explicit timing → `esp_lcd_new_panel_st7701()` with Guition's `init_cmds`
and reset GPIO5 → `reset` → `init` → draw → flush → backlight on.

Numbers that decide design: the framebuffer is 480 × 800 × 2 = **768 000 B** in PSRAM;
measured PSRAM bandwidth is ~**82 MB/s** for plain CPU code, so a full-screen CPU redraw costs
~10 ms and a 60 fps full-frame animation by CPU alone is not available. Use LVGL with
partial refresh, the 2D-DMA (`use_dma2d = true` is already set) or the PPA. The panel is
portrait; landscape means software/PPA rotation (`esp_lvgl_port` `sw_rotate`), since
`swap_xy` is not supported over DSI.

## When the task is choosing a pin

Free and unencumbered, all on JP1: **28, 29, 30, 31, 32, 33, 34, 49, 50, 51, 52** — eleven
pins. **35** is also on JP1 but is the boot strap and SW1: fine as an input after boot.

Bought by giving something up: **37/38** cost the CN2 UART · **26/27** cost RS485 and J5 ·
**21/22** cost touch · **39-44** cost the card · **9-13, 48** cost audio · **7/8** are the
shared bus (use them *as* I2C, e.g. an expander on CN3).

Never: **24/25** (the USB console), **5** (panel reset), **23** (backlight), **53**
(battery), **6, 14-19, 54** (the C6 link). Not reachable at all: **1-4, 20, 36, 45-47**.

More pins than eleven means an I2C expander on CN3 or JP1 23/25, at an address other than
0x14, 0x18, 0x36, 0x5D.

## When the task is "the board does not work"

Diagnose in this order.

1. **ROM banner (`ESP-ROM:esp32p4-eco2…`) on the console?** No → wrong USB-C socket (use the
   full-speed one, `303A:1001`), cable, or GPIO35 held low.
2. **Dies after `entry 0x…` with `Illegal instruction`?** Rule 3. Stop here.
3. **Your output never appears but the banner does?** `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`
   missing, or the app reconfigured GPIO24/25.
4. **`esp_psram_get_size()` 0, or it does not link?** `CONFIG_SPIRAM` missing.
5. **Screen black?** Backlight first (rule 4 — does GPIO23 get driven?), then LDO 3, then
   the init table (rule 1), then timing (rule 2). `LCD ID: FF FF FF` is not the problem.
6. **Screen lit but wrong content?** Cache flush (rule 5) or wrong timing.
7. **Touch fails to init?** Rule 6. Touch works but coordinates are off? Only after a
   rotation — the native frame matches the panel.
8. **Wi-Fi fails?** Rule 9 (transport), then rule 10 (C6 state), then component versions.
9. **Card will not mount?** Rule 15, then FAT32 ≤ 32 GB, then another card.
10. **Silent speaker?** Is one plugged into CN1? Then GPIO11, then DOUT/DIN (rule 14).
11. **Still lost?** Flash `--minimal`: if the console prints and the backlight blinks, the
    build chain is sound and the problem is in peripheral code.

`--full` prints revision, measured clock, flash JEDEC, PSRAM, an I2C scan and the ES8311 ID
in one paste — steps 2-4 and half of 5-7 at once.

## Starting a new project

Do not hand-assemble one: the platform URL cannot be guessed, the sdkconfig has lines
without which the board does not boot or runs its flash slowly, and the panel needs a
39-entry init table. Scaffold from `template/`:

```sh
~/.claude/skills/esp32p4-jc4880p443/template/variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir> && pio run -t upload -t monitor
```

- `--minimal` — revision, flash and PSRAM check on the console, and the **backlight
  blinking on GPIO23** (the board's only controllable light), no display driver, no
  component dependencies. **207,234 B flash, 17,356 B RAM.** Flash this first on a board you
  have not used before.
- `--full` (default) — board passport (revision, measured clock, flash, heaps, PSRAM/SRAM/CPU
  benchmarks, die temperature), I2C scan, ES8311 chip ID, then on the panel: colour bars, a
  live status block and a touch drawing area; SW1 toggles the backlight 100 % ↔ 20 %.
  **367,738 B flash, 20,624 B RAM.** Pulls `esp_lcd_st7701` 1.1.5 and `esp_lcd_touch_gt911`
  1.2.1 from the component registry on first build.

Both build with zero warnings and both were flashed and observed on a rev v1.3 board.
Nothing is generated; copying `template/` by hand works identically. `template/README.md`
maps files to subsystems and says how to strip `--full` back.

Wi-Fi, SD, audio and the 16 MB partition table are **not** in the template — each is a
tested recipe in `reference/recipes.md` (§9-12). Add them to a scaffolded project rather than
starting from Guition's demos, which depend on their patched vendored drivers.

When the user already has a project, bring its `platformio.ini`, `sdkconfig.defaults` and
display init in line with the template before touching application code.

Arduino works too (board "ESP32P4 Dev Module", arduino-esp32 **3.2.1** for rev v1.3, 3.3.11
for rev v3.x, 16 MB QIO, PSRAM enabled) — `recipes.md` §13. Rules 1, 2, 4 and all pins apply
unchanged; rules 8 and 9 are PlatformIO-specific.

## Flashing

```sh
pio run -t upload -t monitor
```

Use the USB-C socket for **full-speed USB** (USB2 on the schematic): the P4's native
USB-Serial/JTAG, `303A:1001`, `/dev/cu.usbmodem*` / `/dev/ttyACM*`, no driver. Upload
resets into the ROM loader and back by itself — no buttons needed in normal use (verified).
The high-speed USB-C carries no console, and there is no CH340 anywhere on the board.

If upload cannot connect (the app broke USB-Serial/JTAG, or is sleeping):

> **hold SW1 (BOOT) → tap SW2 (RST) → release SW1 → upload**

The ROM loader is mask ROM; bad firmware cannot brick the board.

```sh
esptool --chip esp32p4 -p <port> flash-id                                  # identify
esptool --chip esp32p4 -p <port> -b 921600 read-flash 0 0x1000000 factory.bin  # back up the factory demo first
esptool --chip esp32p4 -p <port> erase-flash                               # recover
esptool --chip esp32p4 -p <port> -b 921600 write-flash 0 factory.bin       # restore it
```

esptool 5.3.0 (bundled with the pinned platform) uses `esptool` and hyphenated commands;
`esptool.py` and `flash_id` still work with a deprecation warning. Never run `espefuse`:
burning `EFUSE_0PXA_TIEH_SEL_0` switches the flash supply to 1.8 V permanently.

Erasing the P4 does **not** touch the C6 or its stored Wi-Fi credentials (rule 10).

## Reporting

Say what is verified and what is derived.

**Verified on hardware** (one rev v1.3 board, esp_hosted slave 2.3.0 on its C6, bring-up
that produced this skill): chip revision, 16 MB Boya flash, 32 MB PSRAM, 360 MHz measured;
the rev-v1.3 sdkconfig; QIO only via sdkconfig (DIO otherwise, observed in the boot log);
Boya driver option; both template variants built from `new-project.sh` and running; the
panel showing colour bars and text with Guition's init table, and **black with the registry
default** (rule 1); backlight on GPIO23 by GPIO and by LEDC at 100 %/20 %; GT911 working with
RST 22 / INT 21 + `driver_data` and failing without `driver_data` (rule 6); the double I2C
address; touch coordinates matching the panel; SW1 on GPIO35; ES8311 ID `83 11`; die
temperature; ~4.2 V battery reading with no battery; the 16 MB partition table; SD mount
path reaching `0x107` with no card; ES8311 opening through `esp_codec_dev`, a tone written
and mic samples read (left channel only); the whole esp_hosted sequence of rules 9-10 —
constructor dropped, stack overflow, auto-connect refusal and abort, `*number` not updated,
2.0.17 `netif already added` — ending in working scans (9-15 networks) and a DHCP lease.

**Schematic-derived, high confidence.** Every pin in `board-hardware.md` §2 is matched
against the netlist embedded in Guition's `JC4880P443_V1.0.pdf`: the JP1 map and its four
C6 pins, the unrouted GPIOs, GPIO5 → FPC1 23, touch RST/INT, the backlight pull-down, the
TF card LDO switch, the I2C pull-ups, UART0 on CN2, UART1/RS485, the battery divider.

**Vendor code.** DSI lane rate, DPI timing, LDO channels and voltages, the SDIO link
parameters and esp_hosted version come from Guition's ESP-IDF 5.5.4 demos; the init table
matches ESPHome's `JC4880P443` model byte for byte.

**Not verified.** No speaker, microSD card, battery, camera or RS485 device was attached:
audio output was never heard, the mic was not tested against a known sound, no card was
mounted, charging was not observed, the camera and RS485 were not exercised, and nothing
was tried on a rev v3.x board or under Arduino. The SCons conflict of rule 7 comes from the
`esp32p4-jc-m3-dev` skill; this host already carried its fix. Say so rather than presenting
those parts as certain.

**Known contradictions in Guition's material — quote them as such:** the spec sheet says
the main controller has "integrated Wi-Fi and Bluetooth" (the P4 has none; the C6 does); the
Arduino READMEs say the sample is for a Waveshare 10.1" screen; `pins_config.h` says
`LCD_RST -1` while the driver it ships uses GPIO5; the BSP logs "Install EK79007 LCD control
panel" and creates an ST7701; the Getting Started PDF is for the JC1060P470; the spec calls
CN3 a "2P" connector while the schematic gives it four signals.
