# Template — Guition JC4880P443C_I_W

A PlatformIO + ESP-IDF project that builds clean as-is and has been run on the board. Two
variants; scaffold with

```sh
variants/new-project.sh <target-dir> [--full|--minimal]
```

Nothing is generated and no absolute paths are embedded, so copying this tree by hand and
deleting what you do not want works identically.

## Verified build and run

| | PlatformIO Core 6.2.0 · `platform` pinned to **pioarduino 55.03.311** · ESP-IDF **5.5.5** · esptool 5.3.0 · `board = esp32-p4-evboard` |
|---|---|
| `--minimal` | **207,234 B** flash · **17,356 B** RAM · no component-manager dependencies |
| `--full` | **367,738 B** flash · **20,624 B** RAM · `esp_lcd_st7701` 1.1.5, `esp_lcd_touch_gt911` 1.2.1 |

Zero warnings in both. **Both were flashed and run on a rev v1.3 board**, scaffolded from
this directory with `new-project.sh`: the console output, the colour bars and text on the
panel, touch drawing, the BOOT-button brightness toggle and the minimal variant's
backlight blink were all observed.

## Files by subsystem

| File | Subsystem | In `--minimal`? |
|---|---|---|
| `platformio.ini` | pinned pioarduino platform, 16 MB flash, partitions, USB console | yes |
| `sdkconfig.defaults` | **silicon revision**, QIO + Boya flash driver, console route, 32 MB PSRAM, main stack | yes |
| `CMakeLists.txt`, `src/CMakeLists.txt` | ESP-IDF glue; `src/` globs its own `.c` files | yes |
| `include/board_pins.h` | **every pin on the board**, each marked with its source | yes |
| `src/idf_component.yml` | `esp_lcd_st7701`, `esp_lcd_touch_gt911` from the component registry | no |
| `include/app.h` | module interfaces, `RGB565()` | no |
| `include/font5x7.h` | 5×7 bitmap font (Adafruit GFX glcdfont, BSD) | no |
| `src/display.c` | LDO 3 → DSI bus → DBI/DPI → ST7701 with **this panel's init sequence**, LEDC backlight, framebuffer drawing | no |
| `src/touch.c` | GT911 with RST/INT and `driver_data` | no |
| `src/i2c_bus.c` | the one shared bus, named scan, ES8311 chip-ID read | no |
| `src/battery.c` | ADC2 channel 4 on GPIO53 through the 68K/100K divider | no |
| `src/board_report.c` | chip + revision, measured clock, flash, partitions, heaps, PSRAM/SRAM/CPU benchmarks, die temperature | no |
| `src/main.c` | self-test order, status screen, touch drawing, BOOT → brightness | replaced |
| `variants/minimal/main.c` | revision + flash + PSRAM check + backlight blink on GPIO23 | is `src/main.c` |
| `variants/new-project.sh` | scaffold | — |

## Stripping `--full` back

`src/CMakeLists.txt` globs, so dropping a module means deleting its `.c` and its calls in
`main.c`:

- **no touch** — delete `touch.c`. GPIO21/22 become free (they reach only FPC1).
- **no battery** — delete `battery.c`. GPIO53 is the divider; do not reuse it.
- **no report** — delete `board_report.c`. Keep an `esp_psram_get_size()` check somewhere.
- **no display** — delete `display.c`, `font5x7.h` and `src/idf_component.yml`'s ST7701
  line. GPIO23 has a 10K pull-down, so with nothing driving it the backlight simply
  stays off.

## Why `--minimal` blinks the backlight

The board has **no user LED**: D1 is a red power indicator on the 3V3 rail. The one light
firmware controls without any driver is the backlight: GPIO23 is the MP3202 boost's enable,
and toggling it brightens and darkens the (uninitialised) panel once a second. So
`--minimal` proves the toolchain, the silicon revision, the console route and PSRAM, with
something you can see, and no DSI code that could be the thing that is wrong.

## Third-party code

- `include/font5x7.h` — the classic 5×7 GLCD font from the Adafruit GFX library, BSD
  licence (header comment carries the attribution).
- `esp_lcd_st7701`, `esp_lcd_touch_gt911` — pulled from the Espressif component registry
  at build time (Apache-2.0), not vendored.
- The ST7701 init table in `src/display.c` is Guition's, from the vendored driver in the
  board's download archive.
