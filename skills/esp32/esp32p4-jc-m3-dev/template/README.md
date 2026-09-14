# Template — Guition JC-ESP32P4-M3-DEV

A PlatformIO + ESP-IDF project that builds clean as-is. Two variants; scaffold with

```sh
variants/new-project.sh <target-dir> [--full|--minimal]
```

Nothing is generated and no absolute paths are embedded, so copying this tree by hand and
deleting what you do not want works identically.

## Verified build

| | PlatformIO Core 6.2.0 · `platform` pinned to **pioarduino 55.03.311** · ESP-IDF **5.5.5** · riscv32-esp-elf 14.2.0 · esptool 5.3.0 · `board = esp32-p4-evboard` |
|---|---|
| `--minimal` | **207,224 B** flash · **17,220 B** RAM |
| `--full` | **244,198 B** flash · **17,520 B** RAM |

Zero warnings in both. The `platform` line is a **pinned URL and must stay one**: the
official `platformio/platform-espressif32` has no ESP32-P4 support at any version, so
`platform = espressif32` fails with `Unknown board ID 'esp32-p4-evboard'`.

**Neither variant has been run on hardware in the session that wrote this skill.** The
figures above are build output. The code itself is a port of a project that *was* run on a
rev v1.3 board — see the skill's "Reporting" section for exactly which claims that covers.

## Files by subsystem

| File | Subsystem | In `--minimal`? |
|---|---|---|
| `platformio.ini` | pinned pioarduino platform, 16 MB flash, partitions, USB console, `BLINK_GPIO` | yes |
| `sdkconfig.defaults` | **silicon revision**, flash size, console route, 32 MB PSRAM | yes |
| `CMakeLists.txt`, `src/CMakeLists.txt` | ESP-IDF project glue; `src/` globs its own `.c` files | yes |
| `include/board_pins.h` | **the only board-specific file** — every pin, with its source marked | yes |
| `include/app.h` | module interfaces | no |
| `src/main.c` | GPIO setup, self-test order, blink + telemetry loop | replaced |
| `src/board_report.c` | chip and revision, measured clock, flash, partitions, heaps, PSRAM/SRAM/CPU benchmarks, die temperature | no |
| `src/i2c_scan.c` | the one shared I2C bus, with the expected addresses named | no |
| `variants/minimal/main.c` | revision + flash + PSRAM check + blink | is `src/main.c` |
| `variants/new-project.sh` | scaffold | — |

## Stripping `--full` back

Each module is independent. To drop one, delete its `.c` and the call to it in `main.c`
(`src/CMakeLists.txt` globs, so nothing else changes):

- **no I2C scan** — delete `i2c_scan.c`. Frees GPIO7/GPIO8, which is the whole bus.
- **no report** — delete `board_report.c`. Keep an `esp_psram_get_size()` check somewhere;
  on this board it is the fastest way to tell a good build from a bad one.

## Why `--minimal` exists

The board has **no user LED** — only a red power indicator that is not on a GPIO. So the
usual "blink to prove the toolchain" does not work here, and worse, `BLINK_GPIO` is
configured `GPIO_MODE_INPUT_OUTPUT`: the input buffer reads the **pad**, and an unloaded pad
simply follows its own driver. The read-back proves the *driver* works, never the wiring.

`--minimal` therefore proves the four things that actually fail on this board — the
pioarduino platform resolved, ESP-IDF built for the right silicon revision, the console is
on the socket you plugged into, and the 32 MB PSRAM mapped — and nothing else. Flash it
first on a board you have not used before.

## What to change first

`include/board_pins.h`. It carries every pin on the board with its source marked — `[S]`
the schematic, `[V]` Guition's own demo code, `[D]` the ESP32-P4 datasheet — and calls out
where a source is wrong. `BLINK_GPIO` defaults to **GPIO20**, JP1 pin 17; override it in
`platformio.ini`, and pick only from the eleven free header pins listed in that file.

Wire the LED as:

```
JP1 pin --- [330R] --- anode(+) LED cathode(-) --- GND
```

## Third-party code

None vendored. Everything comes from ESP-IDF itself, pulled by the pinned platform. There
is no `lib_deps` and no component-manager dependency, which is deliberate: on this board the
first build should fail or succeed for reasons you can see in two files.
