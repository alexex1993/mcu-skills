# ESP32-C6-Touch-LCD-1.47 template project

A complete PlatformIO + ESP-IDF project for the Waveshare ESP32-C6-Touch-LCD-1.47.
Builds clean as-is; nothing is generated and no paths are embedded, so copying this tree
by hand works exactly as well as running the scaffold script.

```sh
./variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir>
pio run -t upload -t monitor
```

## Variants

| | Contents | Flash | Static RAM |
|---|---|---|---|
| `--full` (default) | JD9853 display, AXS5106L touch with a live dot and coordinates, three-point calibration stored in NVS, 5×7 font, tile renderer, LEDC backlight, USB console | **271,242 B** — 25.9 % of the 1 MB default app partition | **12,892 B** — 3.9 % of the 327,680 B the linker offers, plus an 11,008 B DMA tile buffer on the heap |
| `--minimal` | Console heartbeat, backlight PWM breathing, I²C bus scan. No display, no SPI bus. | **192,320 B** — 18.3 % | **12,040 B** — 3.7 % |

Figures are what `pio run` reports for ESP-IDF 6.1.0. `--minimal` is the one to flash
first on a new board: it proves the toolchain, the flashing route, the console and the
I²C bus without the display being able to confuse the diagnosis. A healthy board answers
the scan at **0x63** (AXS5106L touch) and **0x6B** (QMI8658A IMU).

Note the denominator on the flash figures: the chip has 8 MB, but
`CONFIG_PARTITION_TABLE_SINGLE_APP` gives the app 1 MB of it. Add a custom
`partitions.csv` before planning on the rest.

## Files by subsystem

| File | Subsystem | Needed by |
|---|---|---|
| `platformio.ini` | build config — platform pin, USB console, port pinning | both |
| `sdkconfig.defaults` | 8 MB flash, console over USB-Serial-JTAG, main-task stack | both |
| `CMakeLists.txt`, `src/CMakeLists.txt` | ESP-IDF project glue | both |
| `include/board_pins.h` | **the pin map** — every board pin in one place | both |
| `components/jd9853/` | `esp_lcd` panel driver + the Waveshare 172×320 init sequence | full |
| `components/axs5106l/` | AXS5106L polling driver, no LVGL, returns raw *and* mapped coordinates | full |
| `include/font5x7.h` | 5×7 ASCII font, 0x20–0x7E | full |
| `src/main.c` | SPI/panel/I²C bring-up, tile renderer, touch loop, calibration flow | full |
| `variants/minimal/main.c` | the minimal `app_main()` | minimal |
| `variants/minimal/CMakeLists.txt` | `src/CMakeLists.txt` without the two components | minimal |

To strip a `--full` scaffold back to something of your own, keep `include/board_pins.h`
and `components/`, and replace `src/main.c` — the display and touch bring-up in
`display_init()` / `i2c_init()` and the `flush_rect()` pattern are the reusable half.

## What is verified on hardware

The display path and the touch path both ran on a real board:

- the pin map, the JD9853 vendor init batch, the 40 MHz pixel clock,
  `set_gap(34, 0)`, `INVON`, big-endian RGB565,
- the `on_color_trans_done` semaphore around the asynchronous `draw_bitmap`, and the
  band-by-band tile renderer,
- the AXS5106L protocol — 14-byte frames, two transactions per read, 200 ms/300 ms
  reset, presence by address ACK — and the mirror-X coordinate mapping,
- the three-point calibration and its NVS blob,
- the LEDC backlight at 90 % duty,
- the USB Serial/JTAG console.

Two things changed since that firmware last ran on hardware, both deliberate:

- **`BSP_BOOT_BTN` moved from GPIO8 to GPIO9.** The vendor wiki says the BOOT button is
  GPIO8; the schematic routes Key2 to QFN32 pin 15, which is GPIO9, and GPIO8 goes only
  to header pin 20. The original value meant `boot_button_held()` read a floating pin and
  always returned false, so the calibration hatch never opened. The fix is
  schematic-derived and compile-verified, **not re-tested on a board.**
- **The backlight is now claimed at 0 % duty before the panel is initialised**, and
  raised to 90 % after the first full flush. GPIO23's reset pull-up biases the backlight
  transistor on, so the panel was lit while it still held power-on garbage. Same LEDC
  calls, different order; compile-verified, not re-tested.

`--minimal` is new in this template and has been built but never flashed.

Nothing here exercises the **QMI8658A IMU**, the **TF card slot**, the **battery ADC**,
Wi-Fi, BLE or deep sleep. Recipes for the first three are in the skill's
`reference/recipes.md`, marked as compile-checked only.

## Third-party code

- `include/font5x7.h` — the classic 5×7 fixed-width glyphs from the
  [Adafruit GFX library](https://github.com/adafruit/Adafruit-GFX-Library), **BSD
  licensed**. The attribution is in the file header; keep it.

Everything else — both components and `src/main.c` — is original to this template.
