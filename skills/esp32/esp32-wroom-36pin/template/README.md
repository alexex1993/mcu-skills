# Template — ESP32-WROOM-32 devkit (36-pin)

A PlatformIO + ESP-IDF project that builds clean as-is. Two variants; scaffold with

```sh
variants/new-project.sh <target-dir> [--full|--minimal]
```

Nothing is generated and no absolute paths are embedded, so copying this tree by hand and
deleting what you do not want works identically.

## Verified build

| | PlatformIO 6.1.19 · platform-espressif32 7.0.1 · ESP-IDF 6.0.1 · `board = esp32dev` |
|---|---|
| `--minimal` | **144,493 B** flash · **13,380 B** RAM |
| `--full` | **784,417 B** flash · **37,036 B** RAM |

Zero warnings in both. The 640 KB difference is the Wi-Fi stack and mbedTLS.

## Files by subsystem

| File | Subsystem | In `--minimal`? |
|---|---|---|
| `platformio.ini` | build, upload, monitor | yes |
| `sdkconfig.defaults` | console on UART0, 40 MHz crystal, flash size/mode, brownout level | yes |
| `partitions.csv` | 1.75 MB app + 2.19 MB SPIFFS | yes |
| `CMakeLists.txt` | ESP-IDF project root | yes |
| `include/board.h` | **the only board-specific file** — 31 exposed GPIOs of which 25 are usable | yes |
| `include/app.h` | module interfaces | no |
| `src/main.c` | orchestration, LEDC heartbeat, BOOT button, NVS boot counter | replaced |
| `src/board_report.c` | chip info, reset reason, **strapping decode**, heap, partitions | no |
| `src/analog.c` | ADC1 oneshot + line-fitting calibration | no |
| `src/wifi_scan.c` | one blocking Wi-Fi scan | no |
| `src/CMakeLists.txt` | component registration | rewritten by the scaffold |
| `variants/minimal/main.c` | LED blink + console heartbeat | is `src/main.c` |
| `variants/new-project.sh` | scaffold | — |

## Stripping `--full` back

Each module is independent. To drop one, delete its `.c` file, remove it from
`src/CMakeLists.txt`'s `SRCS`, and delete the call in `run_selftest()`:

- **no Wi-Fi** — delete `wifi_scan.c`, drop `esp_wifi` from `REQUIRES`. Saves ~620 KB
  of flash and about 50 KB of RAM. This is by far the biggest win.
- **no ADC** — delete `analog.c`, drop `esp_adc`.
- **no report** — delete `board_report.c`, drop `spi_flash` and `esp_partition`. Keep
  the strapping decode somewhere; it is the fastest way to diagnose a board that will not
  boot properly.

## What to change first

`include/board.h` is the file to read before writing any code. It carries the pin map for
this header layout, including `BOARD_FLASH_GPIOS` — the six pins you must never wire, the safe/input-only/strapping/flash pin lists, and the SPI pin
choice that decides whether you get 80 MHz or 26.67 MHz.

Everything else in `src/` uses `BOARD_*` constants rather than pin literals, so moving a
project to a different ESP32 board size is a single-file swap.

## Third-party code

None. ESP-IDF components come from the framework package; nothing is vendored here.
