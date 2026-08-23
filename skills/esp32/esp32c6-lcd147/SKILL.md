---
name: esp32c6-lcd147
description: Firmware development for the Waveshare ESP32-C6-LCD-1.47 board (ESP32-C6FH4, QFN32) — its 1.47" 172x320 ST7789 LCD, WS2812 RGB LED, microSD/TF slot, USB-Serial-JTAG console, Wi-Fi 6/BLE/802.15.4 radio, and the ESP-IDF + PlatformIO setup around them. Use when working on this board or the ESP32-C6-LCD-1.47-M variant (SKU 28563 / 30381, WS-28563): project setup, platformio.ini and sdkconfig, pin mapping and strapping pins, LCD or SD bring-up on the shared SPI bus, backlight brightness, display frame rate, flashing over Type-C, or debugging why something on the board does not work.
---

# Waveshare ESP32-C6-LCD-1.47

Board-specific firmware knowledge. Most failure modes on this board are silent — the
firmware runs, the panel lights up, and something is subtly wrong — so read the
reference file rather than guessing.

- `reference/board-hardware.md` — the complete board reference: pin table, reset-time
  pin states, strapping analysis, free pins, peripheral availability, power budget,
  memory and partitions, the display's quirks (Part I), **plus** a development guide
  (Part II: §10 toolchain, §11 sdkconfig, §12 flashing, §13 peripheral cookbook,
  §14 symptom → cause → fix table).
- `reference/recipes.md` — copy-paste code: `platformio.ini`, `sdkconfig.defaults`,
  `board.h`, LCD bring-up, LEDC backlight, frame presentation, partial redraw, the
  WS2812 driver, TF card on the shared bus, ADC, the BOOT button, font generation.
- `reference/esp32c6-soc.md` — the ESP32-C6 silicon datasheet digest, for chip-level
  questions: IO MUX and LP IO tables, boot straps, memory, every peripheral's feature
  list, electrical and RF characteristics.
- `template/` — a **project that builds clean**, in two variants, plus a scaffold
  script. See `template/README.md`.

## Orientation

| | |
|---|---|
| SoC | ESP32-C6FH4, RISC-V RV32IMAC @ 160 MHz + LP core @ 20 MHz, **QFN32, 22 GPIOs** |
| Memory | **4 MB** in-package flash (1 MB default app partition), 512 KB HP SRAM (~320 KB linkable), 16 KB LP SRAM |
| Display | **ST7789 172×320** on **SPI2** — MOSI **GPIO6**, SCLK **GPIO7**, CS GPIO14, DC GPIO15, RST GPIO21, BL GPIO22. Write-only, no MISO |
| Storage | microSD on the **same** SPI2 bus, CS **GPIO4**, MISO **GPIO5**. SPI 1-bit mode only |
| LED | one WS2812-family RGB LED on **GPIO8** (a strapping pin — safe at factory eFuse settings) |
| Buttons | **BOOT** GPIO9, **pressed = low** · **RESET** acts on `CHIP_PU`, not a GPIO |
| USB | Type-C straight to the SoC's USB-Serial-JTAG (GPIO12/13). **No bridge chip, no debug header** — console, flashing and JTAG all share it |
| Free pins | GPIO0, 1, 2, 3, 18, 19, 20, 23. GPIO0–3 are also the only ADC channels and the only deep-sleep wake pins left |
| Radio | Wi-Fi 6 / BLE 5 / 802.15.4, onboard ceramic antenna, nothing to configure |
| Toolchain | PlatformIO 6.1.19 + `platform-espressif32` 7.0.1 + ESP-IDF 6.0.1, `board = esp32-c6-devkitc-1` |

## Rules that prevent the expensive mistakes

Each of these produces a failure that looks like something else.

1. **`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`** in `sdkconfig.defaults`. Without it the
   console defaults to UART0 on GPIO16/17, which is wired to nothing on this board:
   the monitor is silent while the firmware runs perfectly, and you debug blind.
2. **`board_upload.flash_size = 4MB` + `CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y`.** There is
   no PlatformIO board definition for this board, so it borrows
   `esp32-c6-devkitc-1`, which claims 8 MB. Both lines are needed — they feed
   different tools — or an oversized image links happily and fails to boot.
3. **`esp_lcd_panel_invert_color(panel, true)`.** The IPS panel ships inverted.
   Without it every colour is a photographic negative, which reads as "my RGB565
   packing is broken" and sends you off debugging the wrong thing.
4. **`esp_lcd_panel_set_gap(panel, 34, 0)`.** The 172-pixel glass is centred in the
   ST7789's 240-column RAM: (240 − 172) / 2 = 34. Without it the image is shifted and
   wraps at the edge.
5. **`.data_endian = LCD_RGB_DATA_ENDIAN_BIG`** in the panel config, and byte-swap
   RGB565 **once, at compile time** in the colour macro. Swapping per pixel at runtime
   costs more than the effect being drawn.
6. **`.miso_io_num = -1`** for the panel. It is write-only. A project that also wants
   the SD card must initialise the shared bus with `miso_io_num = 5` instead.
7. **The backlight is PWM, never `gpio_set_level()`.** Waveshare's own warning: above
   50 % duty for extended periods the panel overheats and develops **permanent dark
   shadows**. Drive GPIO22 from an LEDC channel and clamp at 50 %. This is a
   hardware-damage rule, not a style preference.
8. **The LCD and the TF card share MOSI (GPIO6) and SCLK (GPIO7)** and are separated
   only by chip select. Initialise SPI2 **once**; a second `spi_bus_initialize()`
   returns `ESP_ERR_INVALID_STATE`. Adding any SPI device means adding a third CS
   from the free pins, not a second bus.
9. **40 MHz is the pixel-clock ceiling and it is a wiring fact.** The board wires
   MOSI/SCLK onto the pads the IO MUX assigns to SCLK/MOSI, so SPI2 is routed through
   the GPIO Matrix at roughly half the 80 MHz IO-MUX ceiling. No software change
   recovers it.
10. **A full-screen flush is 22 ms → ~45 fps, hard.** 172 × 320 × 2 B = 110,080 B at
    40 MHz. If an animation looks slow this is the wall, not the CPU. See below.
11. **GPIO4–GPIO7 are gone, and they were the interesting ones.** They carry all four
    JTAG pads (so pad-JTAG is impossible — debug over the Type-C port), `ADC1_CH4-6`,
    and the fixed pins of **LP UART and LP I2C** (so neither LP peripheral is
    available while the card slot and display exist).
12. **Never burn `EFUSE_UART_PRINT_CONTROL` or `EFUSE_JTAG_SEL_ENABLE`.** Both hand a
    boot-time decision to a pin that already has a job here — the RGB LED's idle level
    (GPIO8) and the LCD's D/C line (GPIO15). eFuse bits are one-time programmable.
13. **The WS2812 wire order is GRB**, not RGB, and the LED is far brighter than its
    numbers suggest — it sits under clear acrylic beside the panel. Divide by ~10.
14. **Deep-sleep is 7 µA for the SoC, not for the board.** The LDO's quiescent draw,
    the backlight driver and the LED's standby current all add on top. Do not quote
    the datasheet figure for this board.

## When the task is display performance

The bus, not the CPU, is the limit: 110,080 bytes at 40 MHz is 22.0 ms on the wire,
and `gfx_present()` blocks on the transfer-done semaphore so that nothing scribbles
over the framebuffer mid-DMA. That gives a hard ~45 fps for full-screen redraws.

Three moves, in order of payoff:

1. **Redraw less.** `esp_lcd_panel_draw_bitmap()` takes a rectangle. A 172×40 status
   strip is 2.75 ms instead of 22 ms — an 8× win for the common case where only part
   of the UI changed. Coordinates are `(x_start, y_start, x_end, y_end)` with the end
   exclusive, and the source buffer's stride must equal the rectangle width, so
   full-width bands are just an offset into the framebuffer.
2. **Double-buffer**, if you have the RAM: another 110 KB out of ~320 KB. It overlaps
   CPU and DMA but does **not** raise the 45 fps ceiling.
3. Nothing else — do not go looking for a faster clock (rule 9).

Keep per-pixel loops integer-only. The RISC-V core here has **no FPU**; float belongs
in one-time init paths (palette and lookup-table generation), never in a frame loop.

## When the task is memory

A full framebuffer is 110,080 B — about a third of the ~320 KB the linker actually
hands out. The template's full variant links at 123 KB. A Wi-Fi stack is another
~50 KB, a second framebuffer another 110 KB, LVGL more again. **Framebuffer + Wi-Fi +
double-buffering does not fit — pick two.**

## Starting a new project

Do not hand-assemble one. `template/` builds clean; scaffold from it:

```sh
~/.claude/skills/esp32c6-lcd147/template/variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir> && pio run
```

- `--minimal` — RGB LED sweep + console heartbeat, no display, no SPI bus. 167,888 B
  flash, 11,052 B RAM. **Flash this first on a new board**: it proves the toolchain,
  the flashing route and the console while the display cannot confuse the diagnosis,
  and its two outputs (log line, LED) fail independently.
- `--full` (default) — ST7789 effect carousel, 12×24 ASCII + Cyrillic font, LEDC
  backlight, RGB LED accent. 245,214 B flash, 123,052 B RAM.

Both build as-is with ESP-IDF 6.0.1 (verified). Nothing is generated and no paths are
embedded, so copying the tree by hand works identically. `template/README.md` maps
files to subsystems so a `--full` scaffold can be stripped back cleanly.

When the user already has a project, prefer bringing it in line with the template's
`platformio.ini`, `sdkconfig.defaults` and `board.h` over rewriting their code.

## Flashing

Nothing to press:

```sh
pio run -t upload -t monitor        # or: idf.py -p <port> flash monitor
```

The USB Serial/JTAG controller supports host-controlled reset and download-mode entry,
so esptool drives the whole cycle over the Type-C cable. The board appears as CDC-ACM
(`/dev/cu.usbmodem*`, `/dev/ttyACM*`, or a COM port).

When firmware has wedged USB, or a console config change broke enumeration: **hold
BOOT, tap RESET, release BOOT** → Joint Download Boot. ROM code enumerates with no
valid application present, so **bad firmware cannot brick this board**. If it still
fails, `esptool.py -p <port> erase_flash` — a 4 MB erase takes 20–60 s, so it is
probably not hung.

There is no debug header. JTAG is the same Type-C port, via the built-in USB
Serial/JTAG controller — but not with the openocd PlatformIO installs for this board.
`platform-espressif32`'s `platform.json` pins `tool-openocd-esp32` to `~2.1100.0`
(installed: `2.1100.20220706`), and that build predates the ESP32-C6 entirely: it ships
`board/esp32c3-builtin.cfg` and `board/esp32s3-builtin.cfg` but no `esp32c6-builtin.cfg`
and no `target/esp32c6.cfg` anywhere in the package. `pio debug` / a bare
`openocd -f board/esp32c6-builtin.cfg` fails with "Can't find board/esp32c6-builtin.cfg"
on this toolchain, which reads like a typo rather than a missing chip target. Use a
current upstream `openocd-esp32` release (Espressif's fork, not PlatformIO's pinned
copy) or an ESP-IDF export'd environment's own `idf.py openocd`, either of which does
carry ESP32-C6 support.

## Reporting

Say what is verified on hardware and what is derived. In this skill the **display
path** (pin map, 40 MHz clock, gap 34, colour inversion, big-endian RGB565, the 45 fps
ceiling) ran on a real board. The **LEDC backlight** and the **WS2812 driver** are
compile-verified and derived from the vendor documents — the original firmware drove
the backlight as a plain GPIO and never touched the RGB LED. Recipes marked
"⚠︎ compile-checked only" in `reference/recipes.md` (TF card, ADC, BOOT button) are in
the same category. Everything marked **⚠︎ Inference** in
`reference/board-hardware.md` is a conclusion combining the Waveshare wiki with the
Espressif datasheet, not a printed vendor statement — flag it as such rather than
presenting it as fact.
