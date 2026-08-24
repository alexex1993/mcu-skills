# ESP32-C3 0.42" OLED template project

A complete PlatformIO + ESP-IDF project for the ABRobot / 01Space "ESP32-C3 0.42 OLED"
board. Builds clean as-is; nothing is generated at scaffold time and no paths are
embedded, so copying this tree by hand works exactly as well as running the script.

```sh
./variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir>
pio run -t upload -t monitor
```

## Variants

| | Contents | Flash | RAM |
|---|---|---|---|
| `--full` (default) | OLED marquee on the 72×40 panel, full printable-ASCII 5×7 font, blue LED heartbeat, USB console | **163,356 B** — 15.6 % of the 1 MB default app partition | **10,608 B** — 3.2 % of the 327,680 B the linker offers |
| `--minimal` | LED blink + console heartbeat. No display, no I2C. | **140,080 B** — 13.4 % | **10,216 B** — 3.1 % |

Figures are what `pio run` reports for `platform-espressif32` 7.0.1 / ESP-IDF 6.0.1.
The floor of ~140 KB is the IDF baseline (bootloader-independent app image with
FreeRTOS, the console and the flash driver); the display half of the project costs
about 23 KB on top of it.

`--minimal` is the one to flash first on a board you have not used before. Its two
outputs — the log line and the LED — fail independently, so between them they tell
you which half of the chain is broken. `variants/minimal/main.c` documents which
combination means what.

## Files by subsystem

| File | Subsystem | Needed by |
|---|---|---|
| `platformio.ini` | build config — board choice, DIO flash mode, USB upload | both |
| `sdkconfig.defaults` | console over USB Serial/JTAG, 4 MB flash size | both |
| `CMakeLists.txt`, `src/CMakeLists.txt` | ESP-IDF project glue (globs `src/*.c`, adds `include/`) | both |
| `include/board.h` | **the pin map** — every board-specific number in one place | both |
| `src/oled.c`, `include/oled.h` | SSD1306 bring-up for the 72×40 window, framebuffer, text | full |
| `include/font5x7.h` | generated 5×7 font, printable ASCII 0x20–0x7E | full |
| `src/main.c` | the marquee, the LED heartbeat and the console heartbeat | full |
| `variants/minimal/main.c` | the minimal `app_main()` | minimal |
| `scripts/genfont.py` | regenerates `include/font5x7.h`; `--preview` prints every glyph as ASCII art | full, optional |

To strip a `--full` scaffold back to something of your own, delete `src/main.c` and
keep `oled.*` plus `board.h` — that is the reusable half. `oled.h` is the whole API:
`oled_init()`, then framebuffer calls, then `oled_flush()`.

## What is verified on hardware

The display path came from firmware that ran on a real board: the GPIO5/GPIO6 pin
assignment, the 0x3C address, 400 kHz, multiplex ratio 40, the **column offset of
28**, page addressing, the full-GDDRAM wipe at init, and holding the panel off until
the first clean frame has been pushed.

Two things here are **not** hardware-verified by the author of this skill:

- **the 5×7 font** (`include/font5x7.h` and its generator). Every glyph was drawn for
  this template and checked by rendering it back to ASCII art, but only the subset in
  `"Hello, world!"` has been photographed on glass.
- **the blue LED on GPIO8.** The polarity (LOW = lit) comes from the board schematic
  and from an independent bring-up report, not from this template running.

If either misbehaves, say so — that is a gap in this skill, not in your board.

## Third-party code

None. Everything here is original to this template.
