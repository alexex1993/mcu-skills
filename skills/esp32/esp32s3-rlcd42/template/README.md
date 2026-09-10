# Template — Waveshare ESP32-S3-RLCD-4.2

A PlatformIO + Arduino project that builds clean as-is. Two variants; scaffold with

```sh
variants/new-project.sh <target-dir> [--full|--minimal]
```

Nothing is generated and no absolute paths are embedded, so copying this tree by hand and
deleting what you do not want works identically.

## Verified build

| | PlatformIO Core 6.2.0 · `platform = espressif32` resolved to **55.3.311** (pioarduino) · framework-arduinoespressif32 **3.3.11** · U8g2 **2.36.18** · `board = esp32-s3-devkitc-1` |
|---|---|
| `--minimal` | **342,508 B** flash · **24,256 B** RAM |
| `--full` | **372,904 B** flash · **39,928 B** RAM |

Zero warnings in both. `platform` is left unpinned to match the rest of this repo. If a
stock PlatformIO install resolves `espressif32` to a 6.x release (Arduino core 2.0.17)
and you want the core 3.x that Waveshare's own RLCD examples are written against, pin it:

```ini
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.31/platform-espressif32.zip
```

The U8g2 ST7305 driver itself does not need core 3.x. About that floor: Waveshare's docs
ask for "U8g2 v2.36.19 or later" and upstream's ChangeLog lists ST7305 300×400 under
2.36.19, but the **PlatformIO registry's newest U8g2 is 2.36.18** and that package already
contains the `U8G2_ST7305_300X400_*` constructors — verified by compiling this template
against it. `^2.36.19` would fail to resolve, so the floor stays at `^2.36.18`.

**Neither variant has been run on hardware in the session that wrote this skill.** The
figures above are build output, not observed behaviour.

## Files by subsystem

| File | Subsystem | In `--minimal`? |
|---|---|---|
| `platformio.ini` | build, `qio_opi` memory type, 16 MB flash, USB CDC console | yes |
| `include/board_pins.h` | **the only board-specific file** — every pin, with its source marked | yes |
| `include/app.h` | module interfaces | no |
| `src/main.cpp` | orchestration, KEY redraw, one-minute refresh | replaced |
| `src/board_report.cpp` | chip info, **PSRAM check**, flash size, reset reason, button levels | no |
| `src/display.cpp` | ST7305 over SPI2, U8g2 full-frame buffer, HPM/LPM switching | no |
| `src/sensors.cpp` | I2C bus + scan, SHTC3, PCF85063A, battery ADC | no |
| `variants/minimal/main.cpp` | USB CDC heartbeat + PSRAM check | is `src/main.cpp` |
| `variants/new-project.sh` | scaffold | — |

## Stripping `--full` back

Each module is independent. To drop one, delete its `.cpp` and the calls to it in
`main.cpp`:

- **no display** — delete `display.cpp` and the `U8g2` line from `lib_deps`. Saves
  ~120 KB of flash and 15 KB of RAM (the frame buffer) and frees GPIO5/11/12/40/41.
- **no sensors** — delete `sensors.cpp`. Frees GPIO13/14 (the whole I2C bus) and GPIO4.
- **no report** — delete `board_report.cpp`. Keep the `psramFound()` check somewhere.

The TF card and the audio codecs are **not** in this template. Both are documented in
`../reference/recipes.md`, with the SDMMC recipe marked as untested on hardware.

## What to change first

`include/board_pins.h`. It carries every pin on the board with its source marked — `[S]`
the board schematic, `[G]` Waveshare's own example code, `[W]` the wiki table, `[E]` the
ESPHome tutorial, `[Z]` the Zephyr board port — and calls out where they disagree.
Everything in `src/` uses `PIN_*` constants, never literals.

## Third-party code

None vendored. U8g2 (olikraus, BSD-2-Clause) is pulled by `lib_deps`. The SHTC3 and
PCF85063A drivers in `sensors.cpp` are written against the datasheets, not copied from a
library, so the project has exactly one dependency.
