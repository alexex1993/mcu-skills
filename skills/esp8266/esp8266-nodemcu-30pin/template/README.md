# NodeMCU 30-pin ESP8266 template project

A complete PlatformIO + Arduino-core project for the 30-pin NodeMCU ESP8266
devkit (ESP-12E / ESP-12F module). Builds clean as-is; nothing is generated and
no paths are embedded, so copying this tree by hand works exactly as well as
running the scaffold script.

```sh
./variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir>
pio run -t upload -t monitor
```

## Variants

| | Contents | Flash | RAM |
|---|---|---|---|
| `--full` (default) | Board self-test: reset/boot-mode/chip/flash report, heap and fragmentation, boot-strap levels read back, GPIO16↔RST deep-sleep link test, ADC, LittleFS boot counter, Wi-Fi scan, non-blocking dual-LED heartbeat, FLASH button re-runs the report | **304,199 B** — 29.1 % of the 1,044,464 B the `4m1m` layout allows | **29,792 B** — 36.4 % of 81,920 B |
| `--minimal` | Blink on GPIO2 and GPIO16 plus a serial heartbeat. Nothing else. | **267,483 B** — 25.6 % | **28,292 B** — 34.5 % |

Figures are what `pio run` reports for platform-espressif8266 4.2.1 /
framework-arduinoespressif8266 3.1.2 / xtensa-gcc 10.3.0, zero warnings.

The minimal variant is still 267 KB because the Arduino ESP8266 core links the
non-OS Wi-Fi SDK into every sketch whether you call it or not. There is no
smaller floor — do not read 267 KB as "my blink is bloated".

## Files by subsystem

| File | Subsystem | Needed by |
|---|---|---|
| `platformio.ini` | build config — board, flash layout (`4m1m`), LittleFS, monitor and upload speeds, the 74880-baud note | both |
| `include/board.h` | **the pin map** — D-number ↔ GPIO translation, strapping pins, the GPIO16 restrictions, the ADC divider, what is unusable and why | both |
| `src/main.cpp` | the self-test: report, RTC-memory link probe, LittleFS counter, Wi-Fi scan, heartbeat, button | full |
| `variants/minimal/main.cpp` | blink + heartbeat, the first thing to flash on an unknown board | minimal |

To strip a `--full` scaffold back to something of your own, keep
`include/board.h` — that is the reusable half — and replace `src/main.cpp`.

## Uploading

`pio run -t upload -t monitor`. The CP2102 (V1.0/Amica) or CH340G (LoLin V3)
bridge drives RST and GPIO0 through the two-transistor auto-program circuit, so
esptool handles the reset cycle with no buttons pressed.

If upload fails with `Failed to connect` / `Wrong boot mode detected`, hold
**FLASH** while tapping **RST**, release FLASH, then upload. If auto-reset has
stopped working after an interrupted flash, hold FLASH while plugging the USB
cable in — that is the NodeMCU instruction sheet's own recovery step.

`pio device monitor -b 74880` shows the ROM bootloader banner
(`ets Jan 8 2013,rst cause:N,boot mode:(M,N)`). At 115200 the same bytes arrive
as a burst of garbage before your first line — that garbage is the boot log, not
a broken cable.

## What is verified on hardware

Both variants were built and run on a physical 30-pin NodeMCU (CH340G bridge,
ESP-12E/F module, chip id 0x7107F0, 4 MB flash id 0x00164068, 26 MHz crystal
reported by esptool). Confirmed live on that board:

- upload at 460800 baud over the CH340 with no buttons pressed
- ROM boot log at 74880 baud; the same bytes as garbage at 115200
- strapping levels after a normal boot: GPIO0 = 1, GPIO2 = 1, GPIO15 = 0
- `ESP.getFreeHeap()` ≈ 50.4 KB in the full variant, 52.2 KB in the minimal one
- `ESP.deepSleepMax()` ≈ 3.2–3.3 h
- LittleFS mounts and persists across resets (1,024,000 B partition)
- Wi-Fi scan returns real APs and does not brown out over USB
- **GPIO16 is NOT tied to RST on this board** — the probe drove GPIO16 low and
  the chip did not reset, so `ESP.deepSleep()` would never wake it

The LED polarity (both LEDs active LOW) and the A0 divider ratio are read from
the NodeMCU DevKit V1.0 schematic, sheets 6 and 7, not measured. LoLin V3
pinout differences (VU + GND in place of the two RSV pads) come from vendor
documentation, not from a board in hand.

## Third-party code

None. Everything here is original to this template; the Arduino ESP8266 core it
links against ships with PlatformIO's `espressif8266` platform.
