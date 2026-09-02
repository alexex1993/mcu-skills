---
name: esp32c6-touch-lcd147
description: Firmware development for the Waveshare ESP32-C6-Touch-LCD-1.47 board (ESP32-C6FH8, QFN32, 8 MB flash, SKU 31203 / 31201-M) — its 1.47" 172x320 JD9853 IPS panel, AXS5106L capacitive touch controller on I2C 0x63, QMI8658A IMU on I2C 0x6B, microSD/TF slot on the shared SPI2 bus, ETA6098 battery charger with VBAT sensing, USB-Serial-JTAG console, Wi-Fi 6/BLE/802.15.4 radio, and the ESP-IDF + PlatformIO setup around them. Use when working on this board: project setup, platformio.ini and sdkconfig, pin mapping and strapping pins, JD9853 bring-up, touch coordinates and calibration, LVGL memory budgeting, backlight PWM, TF card on the shared bus, battery voltage, flashing over Type-C, or debugging why something on the board does not work. This is NOT the non-touch ESP32-C6-LCD-1.47 (ST7789, 4 MB) — the two share a name and almost no pins.
---

# Waveshare ESP32-C6-Touch-LCD-1.47

Board-specific firmware knowledge. Most failure modes here are silent — the firmware
runs, the panel lights up, the touch controller ACKs, and something is subtly wrong — so
read the reference file rather than guessing.

- `reference/board-hardware.md` — the complete board reference: master pin table, the
  22-pin header, reset-time pin states, strapping analysis, what few pins are free,
  peripheral availability, power and battery, memory and partitions, the display's
  quirks (Part I), **plus** a development guide (Part II: §10 toolchain, §11 sdkconfig,
  §12 flashing and recovery, §13 cookbook index, §14 symptom → cause → fix, §15 a
  line-by-line diff against the non-touch board).
- `reference/touch-axs5106l.md` — the touch controller in full: register map, the four
  protocol rules, coordinate mapping per rotation, the three-point calibration, polling
  vs. interrupt, LVGL wiring, and which registry components to avoid.
- `reference/recipes.md` — copy-paste code: `platformio.ini`, `sdkconfig.defaults`,
  `board_pins.h`, SPI + JD9853 bring-up, safe rectangle flushing, backlight PWM, I²C
  scan, touch, calibration in NVS, BOOT button, battery voltage, the IMU, TF card.
- `reference/esp32c6-soc.md` — the ESP32-C6 silicon datasheet digest, for chip-level
  questions: IO MUX and LP IO tables, boot straps, memory, every peripheral's feature
  list, electrical and RF characteristics.
- `reference/esp32-family.md` — the rest of the family, for "should this be a different
  ESP32?" questions: what does and does not port between chips, radio and USB capability
  per chip, deep-sleep memory and ULP/LP-core availability, a chip-selection table.
- `template/` — a **project that builds clean**, in two variants, plus a scaffold script.
  See `template/README.md`.

## Confirm the board first

Waveshare sells two 1.47" ESP32-C6 boards with nearly the same name. Getting this wrong
costs a full debugging session, because the wrong pin map produces a dark screen, not an
error.

| | ESP32-C6-**LCD**-1.47 | ESP32-C6-**Touch**-LCD-1.47 (this skill) |
|---|---|---|
| Screen responds to a finger | no | **yes** |
| Visible extras | WS2812 RGB LED next to the panel | **no LED**; two status LEDs by the USB port |
| SKU | 28563 / 30381 | **31203 / 31201** |
| Header | 18-pin (`-M` variant) | **22-pin** |
| Battery pad | none | **`VBAT`** |
| Panel controller | ST7789 | **JD9853** |
| Flash | 4 MB (C6FH4) | **8 MB (C6FH8)** |

For the other one use the `esp32c6-lcd147` skill. Do **not** port pin numbers between
them — `reference/board-hardware.md` §15 lists every difference.

## Orientation

| | |
|---|---|
| SoC | ESP32-C6FH8, RISC-V RV32IMAC @ 160 MHz + LP core @ 20 MHz, **QFN32, 22 GPIOs** |
| Memory | **8 MB** in-package flash (**1 MB default app partition**), 512 KB HP SRAM (327,680 B linkable), 16 KB LP SRAM. **No PSRAM** |
| Display | **JD9853 172×320** on **SPI2** — SCLK **GPIO1**, MOSI **GPIO2**, CS GPIO14, DC GPIO15, RST GPIO22. Write-only, no MISO |
| Backlight | **GPIO23**, active high, into an SS8050 NPN base through 1 kΩ. LEDC PWM |
| Touch | **AXS5106L**, I²C **0x63** — SDA **GPIO18**, SCL **GPIO19**, RST GPIO20, INT GPIO21 (active low) |
| IMU | **QMI8658A**, same I²C bus, **0x6B** (SA0 grounded). INT1 GPIO5, INT2 GPIO6 |
| Storage | microSD on the **same SPI2 bus**, CS **GPIO4**, MISO **GPIO3**. SPI 1-bit only, FAT32 |
| Buttons | **BOOT GPIO9, pressed = low** (*not* GPIO8 — the wiki is wrong) · **RESET** acts on `CHIP_PU` |
| Battery | ETA6098 charger, `VBAT` pad; sense on **GPIO0** through 200 k/100 k → `VBAT = 3 × V(GPIO0)` |
| USB | Type-C straight to the SoC's USB-Serial-JTAG (GPIO12/13). **No bridge chip, no debug header** |
| Free pins | **GPIO7 and GPIO8 only.** No ADC channel is free; GPIO7 is the only LP-IO/wake pin left |
| Radio | Wi-Fi 6 / BLE 5 / 802.15.4, onboard ceramic antenna |
| Toolchain | PlatformIO 6.1.19 + `platform-espressif32` **7.1.0** + ESP-IDF **6.1.0**, `board = esp32-c6-devkitc-1` |

## Rules that prevent the expensive mistakes

Each of these produces a failure that looks like something else.

1. **The BOOT button is GPIO9, not GPIO8.** The vendor pinout table says GPIO8; the
   schematic routes Key2 to QFN32 pin 15, which is GPIO9. GPIO8 reaches only header
   pin 20. Firmware that polls GPIO8 for "is BOOT held" reads a floating pin and sees
   *not pressed* forever — the feature simply never fires, with no error.
2. **`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`** in `sdkconfig.defaults`. Without it the
   console defaults to UART0 on GPIO16/17, which only reach the header: the monitor is
   silent while the firmware runs perfectly, and you debug blind.
3. **Pin `monitor_port` and `upload_port` to `/dev/cu.usbmodem*`.** Unpinned, PlatformIO
   takes the first `/dev/cu.*` it finds — on macOS a Bluetooth or AirPlay pseudo-port —
   and attaches to nothing. Same silent-monitor symptom as rule 2, different cause.
4. **Write your own JD9853 driver, or use `template/components/jd9853/`.** ESP-IDF has no
   JD9853. The registry component `mydazy/esp_lcd_jd9853` does not compile on IDF 6.x
   (`rgb_endian` → `rgb_ele_order`) and its init sequence targets a 240×284 BOE panel.
5. **Send the vendor init batch verbatim, and nothing else.** `0xDF 98 53` unlocks the
   vendor registers, `0xDE` selects the register page, `0xC8` is 32 bytes of gamma. It
   already contains `SLPOUT`, `COLMOD`, `MADCTL` and ends with **`INVON` (0x21)** —
   this panel is inverted. Calling `esp_lcd_panel_invert_color()` or resending
   `COLMOD`/`MADCTL` on top upsets the register paging.
6. **`esp_lcd_panel_set_gap(panel, 34, 0)`.** The 172-pixel glass is centred in the
   controller's 240-column RAM: (240 − 172) / 2 = 34. Without it the image is shifted
   sideways and wraps at the edge.
7. **`esp_lcd_panel_draw_bitmap()` is asynchronous.** It queues a DMA transfer and
   returns. Compose the next tile into the same buffer and the panel gets half-old,
   half-new bytes — the screen fills with coloured garbage, *intermittently*, which
   reads as a flaky panel or a bad ribbon. Register `on_color_trans_done`, give a
   semaphore from it, take it after every `draw_bitmap`. This one costs a day.
8. **Byte-swap RGB565 once, at compile time, in the colour macro.** The panel is
   big-endian and the CPU is not. Swapping per pixel in a loop costs more than whatever
   you are drawing.
9. **Park TF `CS` (GPIO4) high before opening the SPI bus.** The card and the panel share
   SCLK (GPIO1) and MOSI (GPIO2) and are separated only by chip select. An unparked card
   corrupts the display's traffic while doing nothing itself.
10. **Initialise SPI2 once.** A second `spi_bus_initialize()` returns
    `ESP_ERR_INVALID_STATE`. Adding a device means a third CS — and GPIO7 and GPIO8 are
    the only pins available for it.
11. **Claim GPIO23 with LEDC at 0 % duty before touching the panel.** GPIO23 has the
    SoC's internal weak pull-up at reset, which is enough base drive to switch the
    backlight transistor on: the panel is lit from power-on, showing whatever its RAM
    held. Raise the duty only after the first full flush.
12. **40 MHz is the pixel-clock ceiling and it is a wiring fact.** The board puts SCLK on
    GPIO1 — which has *no* FSPI IO-MUX function — and MOSI on GPIO2, whose IO-MUX slot is
    MISO. Both go through the GPIO Matrix at roughly half the 80 MHz IO-MUX ceiling. No
    software change recovers it. A full-screen flush is 22 ms → **~45 fps, hard**.
13. **8 MB of flash is still a 1 MB app partition.** `CONFIG_PARTITION_TABLE_SINGLE_APP`
    is the default and leaves 7 MB unpartitioned. Nothing warns you until the image
    overflows; add a custom `partitions.csv` before planning on the space.
14. **The AXS5106L needs the whole 14-byte frame, in two transactions.** Short reads leave
    it misaligned and every following frame comes back shifted; a repeated START
    (`i2c_master_transmit_receive()`) fails every read while the address still ACKs.
    Detect presence by **address ACK**, never by chip ID — some revisions report all
    zeros while working. See `reference/touch-axs5106l.md`.
15. **`TP_RST` low for 200 ms, then wait 300 ms.** Shorter pulses look fine — the chip
    ACKs — and then it reports nonsense coordinates.
16. **The board has two free GPIOs and no free ADC channel.** All seven ADC channels
    (GPIO0–GPIO6) are used, and neither free pin has an analog function. Before promising
    a user an analog input, decide what they are giving up: the IMU interrupts (GPIO5/6)
    or the card slot (GPIO3/4).
17. **Never pull GPIO8 low across a reset, and never burn `EFUSE_UART_PRINT_CONTROL` or
    `EFUSE_JTAG_SEL_ENABLE`.** GPIO8 floats here and is the boot-mode strap; the eFuses
    hand boot-time decisions to a floating pin and to the LCD's D/C line, permanently.

## When the task is touch

Read `reference/touch-axs5106l.md` first — it is 190 lines and every one of them was a
failure at some point.

The mapping for this panel at rotation 0 is **mirror X, leave Y alone**:

```
screen_x = 171 - raw_x
screen_y = raw_y
```

If the dot is mirrored, you skipped the mirror. If it is in the opposite corner, both
axes got flipped. If it moves along the wrong axis, X and Y got swapped. If it tracks but
compressed, the raw full scale is not the panel resolution — scale, do not clamp.

Do not hard-code a guess for a panel that misbehaves. The template derives the mapping
from three taps (origin, displaced along screen X, displaced along screen Y) and stores
it in NVS; that is exactly enough information for all eight orientations plus a different
raw range. Hold **BOOT (GPIO9)** while pressing RESET to run it.

## When the task is display performance or memory

The bus is the limit, not the CPU: 110,080 bytes at 40 MHz is 22.0 ms on the wire, so
full-screen redraws cap at ~45 fps however cheap the drawing is.

The template does **not** keep a framebuffer. It renders straight from the scene into a
172 × 32 DMA staging buffer (11,008 B) band by band, which is why the full variant links
at 12.9 KB of static RAM instead of 123 KB. The trade is CPU for RAM, and it is the right
one here: 110 KB of DRAM buys nothing when the bus caps you at 45 fps anyway.

What actually buys speed, in order of payoff:

1. **Redraw less.** `esp_lcd_panel_draw_bitmap()` takes a rectangle; the end coordinates
   are exclusive and the source stride must equal the rectangle width. A 172 × 40 status
   strip is 2.75 ms instead of 22 ms.
2. **Double-buffer**, if you are keeping framebuffers at all — 110 KB each out of
   327,680 B. It overlaps CPU and DMA but does not raise the 45 fps ceiling.
3. Nothing else. Do not go looking for a faster clock (rule 12).

Keep per-pixel loops integer-only: this RISC-V core has **no FPU**. Float belongs in
one-time init paths, never in a redraw.

For LVGL: two full framebuffers plus LVGL plus a Wi-Fi stack (~50 KB) does not fit in
327,680 B. Use partial buffers of a few dozen lines — the same shape as the template's
tile — and budget before writing code.

## Starting a new project

Do not hand-assemble one. `template/` builds clean; scaffold from it:

```sh
~/.claude/skills/esp32c6-touch-lcd147/template/variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir> && pio run
```

- `--minimal` — console heartbeat, backlight PWM breathing and an I²C bus scan. No
  display, no SPI bus. **192,320 B flash, 12,040 B static RAM.** Flash this first on a
  new board: it proves the toolchain, the flashing route, the console and the I²C bus
  while the display cannot confuse the diagnosis, and its three outputs (log line,
  backlight, scan result) fail independently. A healthy board answers at 0x63 and 0x6B.
- `--full` (default) — JD9853 display, AXS5106L touch with a live dot and coordinates,
  three-point calibration in NVS, 5 × 7 font, tile renderer. **271,242 B flash
  (25.9 % of the 1 MB default app partition), 12,892 B static RAM** plus an 11,008 B
  DMA tile buffer on the heap.

Both build as-is with ESP-IDF 6.1.0 (verified). Nothing is generated and no paths are
embedded, so copying the tree by hand works identically. `template/README.md` maps files
to subsystems so a `--full` scaffold can be stripped back cleanly.

When the user already has a project, prefer bringing it in line with the template's
`platformio.ini`, `sdkconfig.defaults` and `board_pins.h` over rewriting their code.

## Flashing

Nothing to press:

```sh
pio run -t upload -t monitor        # or: idf.py -p <port> flash monitor
```

The USB Serial/JTAG controller supports host-controlled reset and download-mode entry, so
esptool drives the whole cycle over the Type-C cable. The board enumerates as CDC-ACM
(`/dev/cu.usbmodem*`, `/dev/ttyACM*`, or a COM port). **Close the monitor before
uploading** — `Could not open … port is busy` means one is still holding it.

Recovery, in order:

1. Nothing enumerates at all → almost always a charge-only cable. The ROM creates the USB
   device on a blank chip, so **bad firmware cannot brick this board**.
2. Firmware wedged USB, or a console change broke enumeration → **hold BOOT, tap RESET,
   release BOOT**.
3. Still nothing → **jumper header pin 20 (GPIO8) to 3V3 (pin 8) and repeat.** Joint
   Download Boot needs GPIO8 = 1 while GPIO9 = 0, and GPIO8 floats on this board. This is
   what Waveshare's FAQ means by "temporarily connect GPIO8 to 3V3".
4. Still stuck → `esptool.py -p <port> erase_flash`. An 8 MB erase takes a while; it is
   probably not hung.

There is no debug header. JTAG is the same Type-C port via the built-in controller — but
not with the openocd PlatformIO installs here: `platform-espressif32` pins
`tool-openocd-esp32` to `~2.1100.0`, which predates the ESP32-C6 and ships no
`board/esp32c6-builtin.cfg`, so `pio debug` fails with what looks like a typo. Use a
current upstream `openocd-esp32` from Espressif, or an ESP-IDF export'd environment's
`idf.py openocd`.

## Reporting

Say what is verified on hardware and what is derived.

**✅ Ran on a real board:** the display path (pin map, JD9853 vendor batch, 40 MHz clock,
`set_gap(34, 0)`, `INVON`, big-endian RGB565, the async-`draw_bitmap` semaphore, the tile
renderer), the AXS5106L touch path (14-byte frame, no repeated START, 200/300 ms reset,
presence by ACK, the mirror-X mapping) and the three-point calibration, the LEDC
backlight at 90 %, the USB console, and both template variants building on ESP-IDF 6.1.0.

**⚠︎ Schematic- or datasheet-derived, never exercised:** the **BOOT = GPIO9** correction
(netlist, unambiguous, but the template's calibration hatch has not been re-tested on
hardware since the fix), the backlight-lit-at-reset conclusion, the QMI8658A address
0x6B, the battery-voltage recipe, the TF-card recipe, the header's physical row layout,
and the 40 MHz GPIO-Matrix ceiling (the number is conventional, not measured here).
Everything marked **⚠︎ Inference** in `reference/board-hardware.md` is in this category —
flag it as such rather than presenting it as fact.

**Not covered at all:** Wi-Fi/BLE/802.15.4 bring-up beyond memory budgeting, LVGL beyond
the wiring sketch in `reference/touch-axs5106l.md` §9, and deep sleep.
