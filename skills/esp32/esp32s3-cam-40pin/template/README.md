# Template — ESP32-S3-WROOM CAM board, 40-pin

A PlatformIO + Arduino project that builds clean as-is. Two variants; scaffold with

```sh
variants/new-project.sh <target-dir> [--full|--minimal]
```

Nothing is generated and no absolute paths are embedded, so copying this tree by hand and
deleting what you do not want works identically.

## Verified build

| | PlatformIO 6.1.19 · platform-espressif32 7.0.1 · framework-arduinoespressif32 3.20017 (Arduino core 2.0.17) · `board = esp32-s3-devkitc-1` |
|---|---|
| `--minimal` | **280,785 B** flash · **19,052 B** RAM |
| `--full` | **425,641 B** flash · **24,008 B** RAM |

Zero warnings in both. Both figures are the static build; the camera's frame buffers are
allocated from PSRAM at runtime and do not appear in the RAM number — SVGA JPEG with
`fb_count = 2` costs roughly 200 KB of the 8 MB PSRAM.

## Files by subsystem

| File | Subsystem | In `--minimal`? |
|---|---|---|
| `platformio.ini` | build, `qio_opi` memory type, flash size, upload, monitor | yes |
| `partitions.csv` | 4 MB app + 3.875 MB SPIFFS on an 8 MB flash | yes |
| `include/board.h` | **the only board-specific file** — camera, SD, LED, strap and free-pin map | yes |
| `include/app.h` | module interfaces | no |
| `src/main.cpp` | orchestration, WS2812 status, BOOT button capture, heartbeat | replaced |
| `src/board_report.cpp` | chip info, **PSRAM check**, flash size, reset reason, strap levels | no |
| `src/camera.cpp` | `esp_camera_init()` with this board's pins, capture to SD | no |
| `src/sdcard.cpp` | SDMMC 1-bit mount, file write, next-index scan | no |
| `variants/minimal/main.cpp` | LED + WS2812 blink, console heartbeat, PSRAM check | is `src/main.cpp` |
| `variants/new-project.sh` | scaffold | — |

## Stripping `--full` back

Each module is independent. To drop one, delete its `.cpp` and the calls to it in
`main.cpp`:

- **no camera** — delete `camera.cpp`. Saves ~100 KB of flash and frees GPIO4-13 and
  GPIO15-18, which is most of the header's left-hand side.
- **no SD** — delete `sdcard.cpp`. Frees GPIO38/39/40 and restores pad-JTAG.
- **no report** — delete `board_report.cpp`. Keep the `psramFound()` check somewhere: it
  is the difference between "the camera is broken" and "PSRAM was never mapped".

## What to change first

`include/board.h`. It carries the camera and SDMMC pin maps, the three PSRAM pins that
look free on the header and are not, the four strapping pins, and the short list of GPIOs
that are genuinely free once the camera and card are both in use.

Everything in `src/` uses `BOARD_*` constants rather than pin literals.

## Third-party code

None vendored. `esp_camera.h` and `SD_MMC.h` come from the Arduino ESP32 framework
package; the esp32-camera driver is bundled with it and needs no `lib_deps` entry.
