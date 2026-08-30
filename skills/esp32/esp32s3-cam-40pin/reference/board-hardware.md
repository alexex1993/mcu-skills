# ESP32-S3-WROOM CAM, 40-pin — board hardware

Everything physical about the board, then the development guide (§8 onward).

Sources, and how much to trust each:

| Claim | Source |
|---|---|
| module pinout, PSRAM footnote, electrical limits | Espressif *ESP32-S3-WROOM-1/1U Datasheet v1.8* |
| strapping, ADC/touch, glitches, memory map | Espressif *ESP32-S3 Series Datasheet v2.2* |
| camera and SD pin maps, LED pins, header order | Freenove's published sketches and pinout drawing for FNK0085 |
| clone behaviour | inference — a clone copies the layout, not necessarily the BOM |

---

## 1. What this board is

An ESP32-S3-WROOM-1 module on a 22 × 60 mm carrier with a 24-pin DVP camera FPC
connector, a push-push microSD slot, a WS2812, two USB-C sockets and a 2 × 20 header.
Sold as the **Freenove ESP32-S3-WROOM Board (FNK0085)** and, with the same layout and
pin map, as **"ESP32-S3 CAM"**, **"ESP32-S3-WROOM-1 N16R8 CAM Development Board"**,
**"ESP32-S3 CAM Board with OV2640"** and similar.

Module variants seen on this footprint:

| Marking | Flash | PSRAM | Consequence |
|---|---|---|---|
| ESP32-S3-WROOM-1-**N8R8** | 8 MB quad | 8 MB **octal** | `flash_size = 8MB`, `memory_type = qio_opi` |
| ESP32-S3-WROOM-1-**N16R8** | 16 MB quad | 8 MB **octal** | same, but `16MB` is available |
| ESP32-S3-WROOM-1-N16R16V (rare) | 16 MB | 16 MB octal, **VDD_SPI 1.8 V** | GPIO47/48 run at 1.8 V — the WS2812 on GPIO48 may not drive |

Nothing on the silkscreen distinguishes them. `esptool.py --chip esp32s3 flash_id` and
`ESP.getPsramSize()` do.

---

## 2. Header pin map

Forty pins, twenty a side, antenna at the top and the two USB-C sockets at the bottom.
`CAM` = wired to the camera FPC, `SD` = wired to the card slot, `PSRAM` = wired inside
the module and unusable.

### Left header (J1), top to bottom

| # | Silk | GPIO | Board use | Alternate functions |
|---|---|---|---|---|
| 1 | 3V3 | — | 3.3 V out, ~500 mA budget | |
| 2 | RST | — | CHIP_PU / EN. Low = chip off | not readable from firmware |
| 3 | 4 | 4 | **CAM_SIOD** (SCCB SDA) | RTC4, TOUCH4, ADC1_CH3 |
| 4 | 5 | 5 | **CAM_SIOC** (SCCB SCL) | RTC5, TOUCH5, ADC1_CH4 |
| 5 | 6 | 6 | **CAM_VSYNC** | RTC6, TOUCH6, ADC1_CH5 |
| 6 | 7 | 7 | **CAM_HREF** | RTC7, TOUCH7, ADC1_CH6 |
| 7 | 15 | 15 | **CAM_XCLK** | RTC15, U0RTS, ADC2_CH4, XTAL_32K_P |
| 8 | 16 | 16 | **CAM_D7** (Y9) | RTC16, U0CTS, ADC2_CH5, XTAL_32K_N |
| 9 | 17 | 17 | **CAM_D6** (Y8) | RTC17, U1TXD, ADC2_CH6 |
| 10 | 18 | 18 | **CAM_D5** (Y7) | RTC18, U1RXD, ADC2_CH7, CLK_OUT3 |
| 11 | 8 | 8 | **CAM_D2** (Y4) · *Arduino default `SDA`* | RTC8, TOUCH8, ADC1_CH7, SUBSPICS1 |
| 12 | 3 | 3 | free · **strapping: JTAG source, floating, no internal pull** | RTC3, TOUCH3, ADC1_CH2 |
| 13 | 46 | 46 | free after boot · **strapping: boot mode + ROM log, weak pull-down** | — |
| 14 | 9 | 9 | **CAM_D1** (Y3) · *Arduino default `SCL`* | RTC9, TOUCH9, ADC1_CH8, FSPIHD |
| 15 | 10 | 10 | **CAM_D3** (Y5) · *Arduino default `SS`* | RTC10, TOUCH10, ADC1_CH9, FSPICS0 |
| 16 | 11 | 11 | **CAM_D0** (Y2) · *Arduino default `MOSI`* | RTC11, TOUCH11, ADC2_CH0, FSPID |
| 17 | 12 | 12 | **CAM_D4** (Y6) · *Arduino default `SCK`* | RTC12, TOUCH12, ADC2_CH1, FSPICLK |
| 18 | 13 | 13 | **CAM_PCLK** · *Arduino default `MISO`* | RTC13, TOUCH13, ADC2_CH2, FSPIQ |
| 19 | 14 | 14 | **free — the cleanest spare pin on the board** | RTC14, TOUCH14, ADC2_CH3, FSPIWP/FSPIDQS |
| 20 | 5V | — | USB 5 V in/out | |

### Right header (J2), top to bottom

| # | Silk | GPIO | Board use | Alternate functions |
|---|---|---|---|---|
| 1 | TX | 43 | UART0 TX to the CH343 bridge | U0TXD, CLK_OUT1 |
| 2 | RX | 44 | UART0 RX from the CH343 bridge | U0RXD, CLK_OUT2 |
| 3 | 1 | 1 | **free** | RTC1, TOUCH1, ADC1_CH0 |
| 4 | 2 | 2 | **on-board LED, active HIGH** — also free as an output | RTC2, TOUCH2, ADC1_CH1 |
| 5 | 42 | 42 | **free** (costs pad-JTAG) | MTMS |
| 6 | 41 | 41 | **free** (costs pad-JTAG) | MTDI, CLK_OUT1 |
| 7 | 40 | 40 | **SD_D0** | MTDO, CLK_OUT2 |
| 8 | 39 | 39 | **SD_CLK** | MTCK, CLK_OUT3, SUBSPICS1 |
| 9 | 38 | 38 | **SD_CMD** | FSPIWP, SUBSPIWP |
| 10 | 37 | 37 | **OCTAL PSRAM — do not use** | SPIDQS |
| 11 | 36 | 36 | **OCTAL PSRAM — do not use** | SPIIO7 |
| 12 | 35 | 35 | **OCTAL PSRAM — do not use** | SPIIO6 |
| 13 | 0 | 0 | BOOT button + **strapping: boot mode, weak pull-up** | RTC0 |
| 14 | 45 | 45 | free after boot · **strapping: VDD_SPI voltage, weak pull-down** | — |
| 15 | 48 | 48 | **WS2812 data** | SPICLK_N_DIFF |
| 16 | 47 | 47 | **free** | SPICLK_P_DIFF |
| 17 | 21 | 21 | **free** | RTC21 |
| 18 | 20 | 20 | **native USB D+** | RTC20, ADC2_CH9, U1CTS, CLK_OUT1 |
| 19 | 19 | 19 | **native USB D-** | RTC19, ADC2_CH8, U1RTS, CLK_OUT2 |
| 20 | GND | — | ground | |

**Not on this header at all:** GPIO26-34 (GPIO26-32 are the module's internal quad flash
bus; GPIO33/34 are simply not routed). GPIO22-25 do not exist on the ESP32-S3 die.

### The free-pin budget, by configuration

| Build | Free GPIOs |
|---|---|
| camera + SD + WS2812 + UART0 console | 1, 2, 14, 21, 41, 42, 47 — **seven** |
| …plus console moved to native USB | + 43, 44 |
| …without the SD card | + 38, 39, 40 |
| …without the camera | + 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18 |
| never, on any module with PSRAM | 35, 36, 37 |

---

## 3. Camera connector

24-pin 0.5 mm FPC, DVP (parallel) 8-bit. Contacts face **down**, toward the PCB; the
stiffener faces up. The latch is a flip-lock at the back of the connector.

| Signal | GPIO | Note |
|---|---|---|
| XCLK | 15 | driven by the ESP32-S3, LEDC channel 0 / timer 0 |
| PCLK | 13 | sensor → S3 |
| VSYNC | 6 | |
| HREF | 7 | |
| SIOD (SCCB SDA) | 4 | camera driver owns this bus |
| SIOC (SCCB SCL) | 5 | |
| D0…D7 (Y2…Y9) | 11, 9, 8, 10, 12, 18, 17, 16 | note the non-monotonic order |
| PWDN | — | **not wired. Must be `-1`** |
| RESET | — | **not wired. Must be `-1`** |
| 3V3, GND, 2V8, 1V2 | — | LDOs on the carrier |

This is byte-for-byte `CAMERA_MODEL_ESP32S3_EYE` in the Arduino core's `camera_pins.h`,
so the one-line form is:

```cpp
#define CAMERA_MODEL_ESP32S3_EYE
#include "camera_pins.h"
```

Sensors seen on this connector: **OV2640** (2 MP, UXGA 1600×1200, 15 fps; SCCB slave
0x60/0x61), **OV3660** (3 MP), **GC2145** and **GC0308** on later batches. The driver
detects which and `sensor_t.id.PID` reports it. **OV5640 modules physically fit and do
not work** with the bundled driver configuration on this board.

OV2640 electrical notes from its datasheet: I/O 1.7-3.3 V, core 1.3 V, analog 2.5-3.0 V —
all generated on the camera module itself; 125 mW active at UXGA/15 fps, 600 µA standby.
Output formats: YUV422/420, YCbCr422, RGB565/555, 8-/10-bit raw, and JPEG from its
internal compression engine.

---

## 4. microSD slot

Push-push holder on the underside. Wired to the ESP32-S3 **SDMMC host**, **1-bit bus
only** — D1, D2 and D3 are not routed.

| Signal | GPIO | Also |
|---|---|---|
| CLK | 39 | MTCK |
| CMD | 38 | FSPIWP |
| D0 | 40 | MTDO |

There is no card-detect line. `SD_MMC.cardType() == CARD_NONE` is how you detect an empty
slot. Because CLK and D0 are two of the four JTAG pads, mounting the card removes
pad-JTAG; the USB-Serial-JTAG on the native USB port is unaffected.

---

## 5. LEDs and buttons

| Part | GPIO | Polarity |
|---|---|---|
| plain LED (silk `IO2`) | 2 | **active HIGH** |
| WS2812 RGB, 1 pixel, GRB | 48 | `neopixelWrite(48, r, g, b)` |
| power LED | — | hard-wired to 3V3 |
| TX / RX LEDs | — | driven by the CH343, not by firmware |
| **BOOT** button | 0 | pressed = **LOW**, internal weak pull-up |
| **EN / RST** button | — | pulls CHIP_PU low; not readable |

Some clones populate only the WS2812. If `--minimal` prints ticks and the WS2812 blinks
but nothing else lights, that is the board, not the code.

---

## 6. USB, console and debug

Two USB-C sockets, and they are not interchangeable.

| | CH343 bridge | Native USB |
|---|---|---|
| Silicon | WCH CH343 USB-UART, VID `1A86` | the ESP32-S3's own USB peripheral, VID `303A` PID `1001` |
| Wired to | UART0 — GPIO43 (TX), GPIO44 (RX) | GPIO19 (D-), GPIO20 (D+) |
| Enumerates as | `/dev/cu.wchusbserial*`, `/dev/ttyACM*`, `COM*` | `/dev/cu.usbmodem*` |
| `Serial` prints here | **yes, by default** | only with `-DARDUINO_USB_CDC_ON_BOOT=1` |
| Flashes | yes, with DTR/RTS auto-reset — no buttons | yes (USB-Serial-JTAG download) |
| Debug | none | **USB-Serial-JTAG — the board's only debugger** |
| Driver | WCH CH343 driver on macOS < 12 and older Windows | none needed |

The ROM bootloader prints its banner to **both** UART0 and the USB-Serial-JTAG controller
by default (datasheet §3.3), which is why a ROM banner on the native port does not prove
your `Serial` is configured for it.

The board never resets itself into download mode on the native port after a bad firmware;
the ROM always accepts the manual BOOT + EN sequence on either socket.

---

## 7. Power tree

```
USB-C (either socket) --5V--> [ 3.3 V LDO ] --3V3--> module, camera LDOs, SD slot, WS2812
      |                                          |
      +--> 5V header pin                         +--> 3V3 header pin (~500 mA budget)
```

- **One supply route at a time.** USB, *or* 5V, *or* 3V3 — feeding two can back-drive
  the LDO or the host.
- Peak draw: ESP32-S3 ~340 mA at 802.11b TX, plus ~60 mA for an OV2640 streaming, plus
  card write bursts. A thin cable or an unpowered hub browns out at exactly the moment
  Wi-Fi associates.
- Deep sleep on this board is **milliamps, not microamps**: the LDO's quiescent current,
  the CH343 and the power LED dominate. The datasheet's 7-8 µA is the bare chip.
- `VDD_SPI` is 3.3 V on N8R8/N16R8 modules and is what powers the flash and PSRAM.
  GPIO45 selects it at reset; see §11.

---

## 8. Toolchain

Verified combination:

```
PlatformIO Core 6.1.19
platform-espressif32 7.0.1
framework-arduinoespressif32 3.20017 (Arduino core 2.0.17)
board = esp32-s3-devkitc-1
```

`esp_camera.h` and the esp32-camera driver ship **inside** the framework package — no
`lib_deps` entry, no `git clone`. `SD_MMC.h` likewise.

There is no board definition that matches this board. `esp32-s3-devkitc-1` is the right
base because it is an ESP32-S3 with the same variant pin names; the four settings it gets
wrong are corrected in `platformio.ini` (§9).

ESP-IDF works equally well: add `espressif/esp32-camera` from the component registry and
set `CONFIG_SPIRAM=y`, `CONFIG_SPIRAM_MODE_OCT=y`, `CONFIG_SPIRAM_SPEED_80M=y`.

---

## 9. platformio.ini, line by line

```ini
[env:esp32s3cam]
platform  = espressif32
board     = esp32-s3-devkitc-1
framework = arduino

board_build.arduino.memory_type = qio_opi   ; quad flash + OCTAL psram. THE critical line
board_build.flash_mode          = qio
board_build.f_flash             = 80000000L
board_build.f_cpu               = 240000000L

board_upload.flash_size   = 8MB             ; safe on N8R8 and N16R8 alike
board_upload.maximum_size = 8388608
board_build.partitions    = partitions.csv

build_flags =
    -DBOARD_HAS_PSRAM                       ; without it psramFound() is false
    -DCORE_DEBUG_LEVEL=1

monitor_speed   = 115200
monitor_filters = esp32_exception_decoder, time
upload_speed    = 921600
```

| Setting | Why the default is wrong |
|---|---|
| `memory_type = qio_opi` | the board definition is an N8 with **no** PSRAM (`qio_qspi`); the octal PSRAM is never mapped |
| `-DBOARD_HAS_PSRAM` | the Arduino core skips PSRAM init entirely without it |
| `flash_size = 8MB` | the definition already says 8MB — but state it, because raising it to 16MB on an 8MB part boot-loops |
| `partitions.csv` | the stock `default_8MB.csv` is dual-OTA; `huge_app.csv` is a **4 MB** table that strands half the flash |

Optional flags worth knowing:

| Flag | Effect |
|---|---|
| `-DARDUINO_USB_CDC_ON_BOOT=1` | `Serial` moves to the native USB-C |
| `-DARDUINO_USB_MODE=1` | USB-Serial-JTAG (default here); `=0` selects TinyUSB OTG |
| `-DCONFIG_SPIRAM_USE_MALLOC=1` | lets plain `malloc()` fall back to PSRAM |
| `-DCORE_DEBUG_LEVEL=4` | driver-level logs; the camera driver becomes very talkative |

Partition table shipped in `template/partitions.csv` (8 MB, single app):

```
nvs        data nvs      0x9000   0x5000
phy_init   data phy      0xE000   0x1000
factory    app  factory  0x10000  0x400000     # 4 MB sketch
storage    data spiffs   0x410000 0x3E0000     # 3.875 MB
coredump   data coredump 0x7F0000 0x10000
```

---

## 10. Flashing and recovery

Normal:

```sh
pio run -t upload -t monitor
```

Manual download mode (either socket): **hold BOOT → tap EN/RST → release BOOT.** The ROM
answers on both USB routes.

```sh
esptool.py --chip esp32s3 -p <port> flash_id       # real flash size and chip revision
esptool.py --chip esp32s3 -p <port> erase_flash    # ~15 s for 8 MB
esptool.py --chip esp32s3 -p <port> read_mac
```

Recovery from anything short of an eFuse mistake is `erase_flash` followed by the
`--minimal` template. There is no state a bad sketch can leave that survives that.

**One unrecoverable action:** burning `VDD_SPI_FORCE`/`VDD_SPI_TIEH` to 1.8 V with
`espefuse.py` on a 3.3 V module makes the flash permanently unreadable. eFuses are
one-time-programmable. Do not run `espefuse.py burn_efuse` on this board.

---

## 11. Symptom → cause → fix

| Symptom | Cause | Fix |
|---|---|---|
| `psramFound()` false, `ESP.getPsramSize()` 0 | `memory_type` is `qio_qspi` (the board default) | `board_build.arduino.memory_type = qio_opi` **and** `-DBOARD_HAS_PSRAM` |
| `esp_camera_init` → `0x101 ESP_ERR_NO_MEM` | no PSRAM (above), or frame size × `fb_count` too large | fix PSRAM first; then drop to SVGA / `fb_count = 1` |
| `esp_camera_init` → `0x105 ESP_ERR_NOT_FOUND` | no sensor answered on SCCB | reseat the FPC contacts-down; check `PWDN`/`RESET` are `-1`; check the pin macro is `ESP32S3_EYE` |
| camera inits, frames torn / green / half | XCLK too fast for the FPC and batch | `xclk_freq_hz = 10000000` |
| camera was fine, breaks when you add a sensor library | `Wire.begin()` defaulted to GPIO8/9 = CAM_D2/D1 | pass explicit pins: `Wire.begin(14, 21)` |
| camera goes dark after an `analogWrite()` | LEDC channel 0 / timer 0 is XCLK's | `ledcSetup(4, ...)` or higher for your PWM |
| `esp_camera_fb_get()` blocks forever, then TASK_WDT | a leaked frame buffer on an error path | one `esp_camera_fb_return()` per `fb_get()`, always |
| `Cache disabled but cached memory region accessed`, or random `LoadProhibited` | something drove GPIO35/36/37 | those are the octal PSRAM bus — unwire them |
| SD `Card Mount Failed` | `setPins()` after `begin()`, no card, not FAT32, or 4-bit mode | `setPins(39, 38, 40)` **first**, `mode1bit = true`, reformat FAT32 |
| board reboot-loops as soon as the SD driver starts | `begin()` ran with default SDMMC pins, which overlap PSRAM | as above |
| external JTAG probe stopped working | GPIO39/40 are MTCK/MTDO and the card owns them | use USB-Serial-JTAG on the native USB-C |
| port enumerates, nothing prints | cable is in the native USB-C and `Serial` is on UART0 | move the cable, or `-DARDUINO_USB_CDC_ON_BOOT=1` |
| first second of boot output missing on native USB | host enumeration takes ~1.5 s | add `while (!Serial && millis() < 3000);` |
| `Wrong boot mode detected`, `No serial data received` | something holds GPIO0 low, or DTR/RTS is blocked | unwire GPIO0; then the manual BOOT + EN sequence |
| runs, then resets when Wi-Fi associates | brownout — ~340 mA TX peak | thicker cable, powered hub, or 5V from a bench supply |
| resets with `ESP_RST_BROWNOUT` on card writes | write-burst current on top of Wi-Fi | same |
| board dead after wiring an LED to GPIO45 | GPIO45 high at reset ⇒ VDD_SPI 1.8 V ⇒ flash unreadable | remove it; the board is undamaged |
| firmware "stopped running" after wiring GPIO0 | GPIO0 low at reset ⇒ download mode | move the signal to GPIO14 or GPIO21 |
| sketch overflows the app partition | a 4 MB table (`huge_app.csv`) or the 4 MB `default.csv` | use `template/partitions.csv` |
| boot-loops right after an upload, `invalid header` | `flash_size` set to 16MB on an 8 MB module | `flash_id` to check, then set 8MB and `erase_flash` |
| `analogRead()` returns 4095 or noise with Wi-Fi up | the pin is on ADC2, which the Wi-Fi PHY owns | move to ADC1 — with the camera wired that is GPIO1, 2 or 3 |
