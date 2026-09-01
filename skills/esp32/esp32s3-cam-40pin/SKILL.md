---
name: esp32s3-cam-40pin
description: Firmware development for the 40-pin ESP32-S3-WROOM-1 camera board — the Freenove ESP32-S3-WROOM (FNK0085) and the AliExpress "ESP32-S3 CAM" / "ESP32-S3-WROOM N16R8 CAM" clones that copy its header: 20 pins a side, a 24-pin DVP camera FPC connector for an OV2640/OV3660, an on-board microSD slot on the SDMMC bus, a WS2812, two USB-C sockets (CH343 bridge and native USB), ESP32-S3-WROOM-1-N8R8 or N16R8 with 8 MB octal PSRAM. Use when working on this board or any ESP32-S3 DVP camera board: project setup, platformio.ini, the qio_opi PSRAM memory type, camera and SD_MMC pin maps, esp_camera_init failures, PSRAM not found, frame buffer allocation, SD card mount failures, which GPIOs are left once the camera is wired, strapping pins, ADC1/ADC2, flashing over the two USB ports, BOOT/RESET recovery, brownouts, or debugging why the camera, the card or the board does not work.
---

# ESP32-S3-WROOM CAM — 40-pin

Twenty pins a side, a DVP camera connector on the front, a microSD slot on the back, and
two USB-C sockets. The camera claims **fourteen** of the thirty-six GPIO pads, the card
claims three more, and the octal PSRAM claims three that are printed on the header and
look free. What is left is seven pins.

That arithmetic is the whole story of this board. It also means the Arduino defaults for
`Wire` and `SPI` land squarely on camera data lines, and that the single most common
failure — "the camera does not initialise" — is almost never the camera.

- `reference/board-hardware.md` — the board: the full 40-pin header table with every
  alternate function, the camera and SDMMC maps traced to the vendor's own code, the
  PSRAM and strapping pins, the two USB routes, the power tree, the flash/partition
  layout — **plus** a development guide (§8 toolchain, §9 platformio.ini, §10 flashing
  and recovery, §11 symptom → cause → fix table).
- `reference/esp32s3-soc.md` — the silicon: which GPIOs do not exist, strapping semantics
  from the datasheet's own tables, ADC1/ADC2 and touch maps, the LCD_CAM DVP controller,
  power-up glitches, memory map and PSRAM addressing, deep sleep and RTC wake pins.
- `reference/esp32-family.md` — the rest of the family, for "should this be a different
  ESP32?" questions: what does and does not port between chips, radio and USB capability
  per chip, the RMT generation table (WS2812 under Wi-Fi load), deep-sleep memory and
  ULP/LP-core availability, and a chip-selection table.
- `reference/recipes.md` — code that compiles: `platformio.ini`, camera init and capture,
  SDMMC, camera → SD, an MJPEG web server, moving the console to native USB, I2C and SPI
  on the pins that are actually free, ADC, WS2812, deep sleep.
- `template/` — a **project that builds clean**, in two variants, plus a scaffold script.
  See `template/README.md`.

## Confirm the board first

Several ESP32-S3 camera boards exist and their pin maps are mutually incompatible. This
skill is for the 40-pin one. Check three things before writing any pin:

| | This board | Not this board |
|---|---|---|
| Header | **20 + 20 pins**, `3V3 RST 4 5 6 7 15 16 17 18 8 3 46 9 10 11 12 13 14 5V` down one side | 44-pin DevKitC-1 (no camera), 21-pin XIAO S3 Sense, ESP32-S3-EYE (no header) |
| Camera | 24-pin FPC on the **top face**, XCLK on **GPIO15** | ESP32-S3-CAM-LCD boards use XCLK GPIO40; XIAO S3 Sense uses XCLK GPIO10 |
| microSD | slot on the **back**, SDMMC on **GPIO39/38/40** | XIAO S3 Sense: SPI on GPIO7/8/9; LCD boards: GPIO47/48/... |

The camera pin map is byte-for-byte `CAMERA_MODEL_ESP32S3_EYE` in the Arduino core's
`camera_pins.h`. **Use that macro, not `CAMERA_MODEL_ESP32S3_CAM_LCD` and not
`CAMERA_MODEL_XIAO_ESP32S3`** — both compile and both give you a sensor that never
answers on SCCB.

If the user is unsure, flash `template/ --minimal` and read the report: it prints flash
size, PSRAM size and chip revision, which identifies the module.

## Orientation

| | |
|---|---|
| Module | ESP32-S3-WROOM-1 **N8R8** or **N16R8**. ESP32-S3, **Xtensa LX7 dual-core** @ 240 MHz |
| Memory | 8 or 16 MB quad SPI flash · **8 MB OCTAL SPI PSRAM** · 512 KB SRAM (~320 KB linkable) · 8 KB RTC FAST + 8 KB RTC SLOW |
| Header | 40 pins, 20 a side. **36 GPIO pads**, plus 3V3, 5V, GND, EN |
| Camera | 24-pin DVP FPC. OV2640 (2 MP) on most batches, OV3660 or GC2145 on some. `PWDN`/`RESET` **not wired — must be `-1`** |
| Camera pins | XCLK **15** · SIOD **4** · SIOC **5** · VSYNC **6** · HREF **7** · PCLK **13** · D0-D7 = **11, 9, 8, 10, 12, 18, 17, 16** |
| microSD | SDMMC host, **1-bit only** (D1/D2/D3 not routed). CLK **39** · CMD **38** · D0 **40** |
| LEDs | plain LED on **GPIO2, active high** · **WS2812 on GPIO48** (one pixel, GRB) · power/TX/RX LEDs not on a GPIO |
| Buttons | **BOOT** = GPIO0, pressed low, internal pull-up · **EN/RST** acts on CHIP_PU, not readable |
| USB | **two USB-C**. One is a **WCH CH343** bridge on UART0 (GPIO43/44) with DTR/RTS auto-reset; the other is the S3's **native USB** on GPIO19/20 (USB-Serial-JTAG + OTG) |
| Console | UART0 @ 115200 by default. `Serial` moves to native USB only with `-DARDUINO_USB_CDC_ON_BOOT=1` |
| Unusable | **35, 36, 37** — octal PSRAM, on the header and fatal to touch · **26-32** flash bus, not brought out · **22-25** do not exist on the S3 |
| Strapping | **0** (boot) · **45** (VDD_SPI voltage) · **46** (boot + ROM log) · **3** (JTAG source, floating, no internal pull) |
| Free with camera + SD | **1, 2, 14, 21, 41, 42, 47** — and 19/20 or 43/44 if you give up a USB route |
| Analog | ADC1 = GPIO1-10, ADC2 = GPIO11-20. With the camera wired the only free ADC1 channels are **GPIO1, 2, 3** |
| Radio | Wi-Fi b/g/n + Bluetooth 5 LE, PCB antenna |
| Toolchain | PlatformIO 6.1.19 + `platform-espressif32` 7.0.1 + Arduino core 2.0.17, `board = esp32-s3-devkitc-1` |

## Rules that prevent the expensive mistakes

Each of these produces a failure that looks like something else.

1. **`board_build.arduino.memory_type = qio_opi` and `-DBOARD_HAS_PSRAM`, or nothing
   works.** The `esp32-s3-devkitc-1` board definition is an **N8 with no PSRAM**; its
   default memory type is `qio_qspi`. Leave it and the 8 MB octal PSRAM is never mapped:
   `psramFound()` returns false and `esp_camera_init()` fails with `ESP_ERR_NO_MEM`
   (0x101) — or, worse, succeeds at QVGA and then dies the first time you raise the frame
   size. There is no message saying "PSRAM missing"; check `psramFound()` first, always.

2. **GPIO35, 36 and 37 are printed on the header and are the PSRAM bus.** On any R8 or
   R16V module the datasheet's own footnote reads: "pins IO35, IO36, and IO37 are
   connected to the Octal SPI PSRAM and are not available for other uses". Driving one is
   not a soft failure — it corrupts the bus the CPU is reading its cached data from, and
   the board reboots with `Cache disabled but cached memory region accessed` or a
   `LoadProhibited` panic that points anywhere but the pin you touched.

3. **`Wire.begin()` and `SPI.begin()` with no arguments land on camera data lines.** The
   ESP32-S3 Arduino variant defaults to `SDA = 8, SCL = 9` and `MOSI = 11, MISO = 13,
   SCK = 12, SS = 10`. On this board those are camera **D2, D1, D0, PCLK, D4, D3**. The
   symptom is not an I2C error: the camera starts returning torn or all-green frames the
   moment your sensor library initialises. **Always pass explicit pins** —
   `Wire.begin(SDA_PIN, SCL_PIN)` — and pick them from the free list.

4. **The camera's SIOD/SIOC on GPIO4/5 is an SCCB bus owned by the camera driver, not a
   general I2C bus.** Hanging your own I2C devices there works until the driver reprograms
   the sensor and the transaction collides. Give other I2C devices their own pins.

5. **`SD_MMC.setPins(39, 38, 40)` must be called *before* `SD_MMC.begin()`.** In the other
   order `setPins()` returns true, changes nothing, and `begin()` uses the S3's default
   SDMMC pins — which on an R8 module overlap the PSRAM pins of rule 2 and put the board
   into a reboot loop. And pass `mode1bit = true`: D1/D2/D3 are not routed on this board,
   so 4-bit mode cannot work.

6. **Mounting the card kills pad-JTAG.** GPIO39 is MTCK and GPIO40 is MTDO. Once the SD
   driver owns them an external JTAG probe is gone. Use the **built-in USB-Serial-JTAG on
   the native USB-C** instead — it needs no pins and works while the card is mounted.

7. **`Serial` goes to UART0 (GPIO43/44), not to the native USB port.** Plugging into the
   wrong USB-C gives you a device that enumerates and prints nothing. The CH343 bridge
   shows up as `wchusbserial`/`ttyACM`; the native port as `usbmodem` with VID `303A`. To
   put the console on native USB add **`-DARDUINO_USB_CDC_ON_BOOT=1`** — and accept that
   you then lose the first ~1.5 s of boot output while the host enumerates.

8. **GPIO19 and GPIO20 are the native USB D-/D+.** Using them as GPIO kills that port,
   which also removes USB-Serial-JTAG (rule 6) and the only debugger the board has. They
   are also ADC2_CH8/CH9, so "just use them for analog" costs the same.

9. **GPIO45 high at reset sets VDD_SPI to 1.8 V and the flash stops reading.** It has a
   weak pull-down and must stay low through reset. An LED to 3V3, a pull-up on a shared
   bus, or a peripheral idling the line high leaves a board that looks bricked and is
   fine as soon as the pull comes off. GPIO46 is the same shape of trap for boot mode.

10. **GPIO0 is the BOOT button *and* a header pin.** Anything holding it low at reset puts
    the board in download mode — which presents as "my firmware stopped running", not as a
    pin problem. If uploads start failing after you wire something, unplug GPIO0 first.

11. **XCLK is generated by LEDC channel 0 / timer 0.** `analogWrite()` on the Arduino core
    allocates LEDC channels from 0 upward, so the first `analogWrite()` in a camera sketch
    can retune the timer under the sensor. Frames go dark or tear and nothing reports an
    error. Give your PWM an explicit high channel with `ledcSetup(4, ...)` and up.

12. **ADC2 is dead while Wi-Fi is running**, and with the camera wired ADC2 is GPIO11-20 —
    all camera pins anyway. Your entire analog budget on a connected camera build is
    **GPIO1, GPIO2 and GPIO3** on ADC1. Plan the pinout around that number.

13. **Every `esp_camera_fb_get()` needs a matching `esp_camera_fb_return()`, on the error
    paths too.** With `fb_count = 2` two leaked buffers stall the driver permanently: the
    next `fb_get()` blocks forever and the watchdog resets the board, pointing at whatever
    task happened to be running.

14. **A reset when Wi-Fi starts, or when the flash LED fires, is the supply.** The S3 peaks
    at ~340 mA transmitting, the sensor adds its own, and a thin USB cable or a laptop
    hub will not hold 3V3. `esp_reset_reason()` returns `ESP_RST_BROWNOUT`; say that
    rather than reading the backtrace.

15. **`board_upload.flash_size` must match the module.** The N8R8 and N16R8 both exist on
    this footprint and nothing on the silkscreen distinguishes them. Set **8MB** — it is
    correct on an N8R8 and merely wasteful on an N16R8. The reverse (16 MB set on an 8 MB
    part) writes a partition table off the end of the flash and boot-loops. `esptool.py
    flash_id` prints the real size.

16. **Do not use `huge_app.csv` here.** It is a **4 MB** table: its coredump partition
    lands at 0x3F0000 and the upper half of an 8 MB flash is simply lost. The board's own
    default (`default_8MB.csv`) is a dual-OTA table that spends 3.1 MB on a second app
    slot. `template/partitions.csv` is a single-app 4 MB + 3.875 MB SPIFFS layout that
    keeps the same offsets on 8 MB and 16 MB parts.

## When the task is choosing pins

Work outward from what is already taken. With the camera and the card both in use the
header has **seven** free GPIOs:

| GPIO | Note |
|---|---|
| 1, 2, 3 | ADC1_CH0/CH1/CH2, TOUCH1/2/3. **GPIO2 also drives the plain LED**; **GPIO3 is the JTAG-source strap and floats** — give it a defined level at reset |
| 14 | ADC2_CH3, TOUCH14. The cleanest single free pin on the board |
| 21 | plain GPIO, no strap, no analog |
| 41, 42 | MTDI / MTMS. Free unless you want pad-JTAG (which rule 6 already cost you) |
| 47 | plain GPIO. GPIO48 next to it is the WS2812 |

Then the ones you buy by giving something up:

- **19, 20** — cost you the native USB port and USB-Serial-JTAG (rule 8).
- **43, 44** — cost you the UART0 console; only sane together with rule 7.
- **45, 46** — usable *after* boot if their reset-time level is respected (rule 9).
- **4-13, 15-18** — camera. Free only in a build with no camera.
- **38, 39, 40** — microSD. Free only in a build with no card.

If seven is not enough, the answer is an **I2C GPIO expander or a shift register on
GPIO14/21**, not one of the PSRAM pins. There is no ninth pin hiding on this header.

## When the task is the camera

Start from `template/src/camera.cpp` — it is a working init for this exact board.

The mechanism worth knowing: the LCD_CAM peripheral clocks 8 parallel data bits in on
every PCLK edge into a DMA ring, and the driver hands you whole frames out of PSRAM. So:

- **Frame buffers live in PSRAM** (`fb_location = CAMERA_FB_IN_PSRAM`). An SVGA JPEG with
  `fb_count = 2` costs roughly 200 KB of the 8 MB. UXGA RGB565 is 3.8 MB **per buffer** —
  one buffer fits, two do not, and the failure is `ESP_ERR_NO_MEM` at init.
- **`PIXFORMAT_JPEG` is the only format worth streaming.** RGB565 at anything above QVGA
  saturates the PSRAM bandwidth before it saturates Wi-Fi; the frame rate you measure will
  be the memory bus, not the sensor.
- **`xclk_freq_hz = 20000000` is the usual figure; the vendor's own sketches ship
  10 MHz.** Torn frames, a green cast, or `Camera capture failed` on an otherwise correct
  configuration is the first thing 10 MHz fixes. It is a signal-integrity limit of the FPC
  and the batch, not a setting you can reason from the datasheet.
- **`grab_mode = CAMERA_GRAB_LATEST`** for a live stream, `CAMERA_GRAB_WHEN_EMPTY` for
  stills. The default drops you a frame behind.
- The sensor is mounted upside down relative to the silkscreen: `set_vflip(s, 1)` and
  `set_hmirror(s, 1)`.
- **`ESP_ERR_NOT_FOUND` (0x105) means no sensor answered on SCCB.** In order: reseat the
  FPC (contacts face the PCB, latch fully down), confirm `PWDN`/`RESET` are `-1`, confirm
  the pin macro is `CAMERA_MODEL_ESP32S3_EYE`. It is a connector fault far more often
  than a code fault.

## When the task is the microSD card

`template/src/sdcard.cpp` is the working version. The card is on the **SDMMC host in
1-bit mode**, not on SPI — do not reach for `SD.h` and a `SPI` bus here; `SD_MMC.h` with
`setPins()` is the whole interface.

- Mount failures are, in order: no card, not FAT32, `setPins()` called too late (rule 5),
  or a card that needs `SDMMC_FREQ_PROBING` instead of `SDMMC_FREQ_DEFAULT`.
- Pass `format_if_empty = false` unless the user asked for the opposite. The Arduino
  signature is `begin(mountpoint, mode1bit, format_if_empty, sdmmc_frequency, maxOpenFiles)`
  and the vendor's example ships `true` in the third slot.
- Writing a JPEG per frame is bounded by the card, not the camera: a cheap class-10 card
  gives ~2-4 SVGA frames per second. Buffer in PSRAM if you need bursts.

## When the task is "the board does not work"

Diagnose in this order — each stage only depends on the ones before it:

1. **Does `psramFound()` return true?** If not, stop: rule 1. Every camera symptom
   downstream of this is a consequence, not a cause.
2. **Is anything wired to GPIO35/36/37, or to GPIO0 or GPIO45?** Unplug them first
   (rules 2, 9, 10).
3. **Which USB-C is the cable in?** `wchusbserial` prints by default; `usbmodem` needs
   `-DARDUINO_USB_CDC_ON_BOOT=1` (rule 7).
4. **Does anything print at 115200?** The ROM banner (`ESP-ROM:esp32s3-...`) comes out
   before any of your configuration applies. No banner ⇒ cable, port or bridge driver.
5. **Does it boot-loop?** Read the reset reason. `ESP_RST_BROWNOUT` ⇒ rule 14.
   `Cache disabled but cached memory region accessed` ⇒ rule 2. `invalid header` ⇒
   `esptool.py erase_flash` and reflash `--minimal`.
6. **Camera-specific:** `0x101` = memory (rule 1 or a frame size too large), `0x105` =
   nothing on SCCB (reseat the FPC), init OK but frames are torn or green = XCLK, or
   rule 3, or rule 11.
7. **Card-specific:** mount failure ⇒ the list above. Mount OK but writes fail ⇒ the card
   is full or the FAT is damaged; try another card before another line of code.
8. **Still lost?** Flash `--minimal` for a baseline that touches only the toolchain, the
   flashing route, the console, two LEDs and the PSRAM mapping — then add one subsystem
   at a time.

`template/src/board_report.cpp` prints the PSRAM state, flash size, reset reason and the
strapping levels, which answers most of steps 1-5 in one paste.

## Starting a new project

Do not hand-assemble one — the `platformio.ini` has four settings that are wrong by
default and silent when wrong. Scaffold from `template/`:

```sh
~/.claude/skills/esp32s3-cam-40pin/template/variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir> && pio run
```

- `--minimal` — LED + WS2812 blink, console heartbeat, PSRAM check. **280,785 B flash,
  19,052 B RAM.** Flash this first on a board you have not used before: its outputs fail
  independently, so it separates "wrong USB port" from "no PSRAM" from "clone without the
  GPIO2 LED" without any guessing.
- `--full` (default) — board self-test: report, microSD over SDMMC, camera init, and a
  JPEG onto the card on every BOOT press. **425,641 B flash, 24,008 B RAM.** The camera's
  frame buffers are PSRAM and do not appear in that RAM figure.

Both build as-is with Arduino core 2.0.17 (verified, zero warnings). Nothing is generated
and no paths are embedded, so copying `template/` by hand works identically.
`template/README.md` maps files to subsystems so a `--full` scaffold can be stripped back.

When the user already has a project, prefer bringing their `platformio.ini` and
`include/board.h` in line with the template over rewriting their sketch.

ESP-IDF is a reasonable alternative and every pin rule above applies unchanged; add
`espressif/esp32-camera` as a managed component and set `CONFIG_SPIRAM_MODE_OCT=y`,
`CONFIG_SPIRAM_SPEED_80M=y`. The Arduino core bundles the same camera driver, which is
why the template uses it.

## Flashing

```sh
pio run -t upload -t monitor
```

The CH343 bridge drives EN and GPIO0 through the auto-reset circuit, so no buttons are
needed on the UART port. Ports:

| Socket | Enumerates as | Notes |
|---|---|---|
| CH343 bridge | `/dev/cu.wchusbserial*`, `/dev/ttyACM*`, a `COM` port | flashes and prints out of the box; needs the WCH driver on older macOS |
| native USB | `/dev/cu.usbmodem*`, VID `303A` PID `1001` | flashes too, and carries USB-Serial-JTAG for debugging; prints only with `-DARDUINO_USB_CDC_ON_BOOT=1` |

When upload fails with `Wrong boot mode detected` or `No serial data received`, **check
GPIO0 before the cable** (rule 10). Then the manual sequence: **hold BOOT → tap EN/RST →
release BOOT → upload.** If 921600 is unreliable, drop `upload_speed` to 460800.

**Bad firmware cannot brick this board.** The ROM bootloader always answers that sequence:

```sh
esptool.py --chip esp32s3 -p <port> flash_id      # tells you 8 MB vs 16 MB (rule 15)
esptool.py --chip esp32s3 -p <port> erase_flash   # ~15 s for 8 MB
```

The one genuinely unrecoverable mistake is burning the `VDD_SPI` eFuse to 1.8 V on a
3.3 V module. eFuses are one-time. Do not run `espefuse.py` on this board.

## Reporting

Say what is verified and what is derived.

**Primary-source, high confidence:** the module pin table, the "IO35/36/37 are the octal
PSRAM and not available" footnote, the strapping tables (GPIO0/3/45/46), the power-up
glitch list and the ADC/touch channel maps in `reference/` are transcribed from the
Espressif *ESP32-S3-WROOM-1/1U Datasheet v1.8* and *ESP32-S3 Series Datasheet v2.2*.

**Vendor-source, high confidence:** the camera and SDMMC pin maps, the WS2812 on GPIO48
and the LED on GPIO2 come from Freenove's own published sketches for this board
(`camera_pins.h` → `CAMERA_MODEL_ESP32S3_EYE`, `SD_MMC_CLK 39 / CMD 38 / D0 40`, marked
"Please do not modify it") and from their published pinout drawing. The clones copy this
layout, but a clone is still a clone.

**Verified on this machine:** both template variants build clean with PlatformIO 6.1.19 /
platform-espressif32 7.0.1 / Arduino core 2.0.17, zero warnings, at the flash and RAM
figures quoted above. Every recipe in `reference/recipes.md` was compiled in that project.

**Not verified on hardware:** nothing here was run on a physical board in this session —
no photo was captured, no card was mounted, no frame rate was measured. Details that vary
by vendor and batch (which sensor is fitted, whether the GPIO2 LED is populated, whether
20 MHz XCLK is stable, N8R8 vs N16R8) must be confirmed with `--minimal` on the user's
actual board. Say so rather than presenting them as certain.
