# ESP32-C6-LCD-1.47 template project

A complete PlatformIO + ESP-IDF project for the Waveshare ESP32-C6-LCD-1.47. Builds
clean as-is; nothing is generated and no paths are embedded, so copying this tree by
hand works exactly as well as running the scaffold script.

```sh
./variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir>
pio run -t upload -t monitor
```

## Variants

| | Contents | Flash | RAM |
|---|---|---|---|
| `--full` (default) | ST7789 carousel of four demoscene effects, 12×24 ASCII + Cyrillic font, LEDC backlight, RGB LED accent, USB console | **245,214 B** — 23.4 % of the 1 MB default app partition | **123,052 B** — 37.6 % of the 327,680 B the linker offers |
| `--minimal` | RGB LED hue sweep + console heartbeat. No display, no SPI bus. | **167,888 B** — 16.0 % | **11,052 B** — 3.4 % |

Figures are what `pio run` reports for ESP-IDF 6.0.1. `--minimal` is the one to flash
first on a new board: it proves the toolchain, the flashing route and the console
without the display being able to confuse the diagnosis.

## Files by subsystem

| File | Subsystem | Needed by |
|---|---|---|
| `platformio.ini` | build config — flash size override, USB upload | both |
| `sdkconfig.defaults` | console over USB-Serial-JTAG, 4 MB flash | both |
| `CMakeLists.txt`, `src/CMakeLists.txt` | ESP-IDF project glue (globs `src/*.c`) | both |
| `include/board.h` | **the pin map** — every board pin in one place | both |
| `src/rgb_led.c`, `include/rgb_led.h` | WS2812 LED on GPIO8 over RMT | both |
| `src/gfx.c`, `include/gfx.h` | SPI + ST7789 bring-up, framebuffer, LEDC backlight, text | full |
| `include/font12x24.h` | generated 1bpp font, ASCII + Cyrillic | full |
| `src/effects.c`, `include/effects.h` | plasma / starfield / tunnel / fire, integer-only inner loops | full |
| `src/main.c` | the carousel and its title cards | full |
| `variants/minimal/main.c` | the minimal `app_main()` | minimal |
| `scripts/genfont.py` | regenerates `include/font12x24.h` from any monospaced TTF (needs Pillow) | full, optional |

To strip a `--full` scaffold back to something of your own, delete `src/effects.c`,
`include/effects.h` and `src/main.c` and keep `gfx.*` — that is the reusable half.

## What is verified on hardware

The display path — pin map, 40 MHz pixel clock, `set_gap(34, 0)`,
`invert_color(true)`, big-endian RGB565, the ~45 fps full-frame ceiling, and the
effects themselves — comes from a project that ran on the board.

Two things are **derived, not hardware-verified by the author of this skill**:

- **the LEDC backlight** (`gfx_set_backlight()` takes a percentage and clamps at
  50 %). The original firmware drove GPIO22 as a plain output, i.e. 100 % brightness,
  which contradicts Waveshare's own overheating warning. The LEDC version is the
  correct fix, and it compiles and links, but the brightness curve has not been
  eyeballed on a panel.
- **the RGB LED driver** (`src/rgb_led.c`). The original firmware never touched
  GPIO8. The WS2812 bit timings are the standard ones and the RMT configuration
  compiles, but no LED has confirmed them.

If either misbehaves, say so — that is a gap in this skill, not in your board.

## Third-party code

None. Everything here is original to this template; the only generated artefact is
`include/font12x24.h`, whose glyph bitmaps are rasterised from whichever font you
point `scripts/genfont.py` at (Menlo, an Apple system font, by default — regenerate
from a freely licensed face if you plan to redistribute the header).
