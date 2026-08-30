# Recipes — 40-pin ESP32-S3-WROOM CAM board

Code that compiles against PlatformIO 6.1.19 / platform-espressif32 7.0.1 / Arduino core
2.0.17. The camera, SD and report recipes are extracted from `template/`, not retyped;
the rest were compiled in the same project.

Pin names below come from `template/include/board.h`.

---

## 1. `platformio.ini`

```ini
[env:esp32s3cam]
platform  = espressif32
board     = esp32-s3-devkitc-1
framework = arduino

; The board definition is an N8 with NO PSRAM. Both of the next two are mandatory.
board_build.arduino.memory_type = qio_opi
board_build.flash_mode          = qio
board_build.f_flash             = 80000000L
board_build.f_cpu               = 240000000L

board_upload.flash_size   = 8MB
board_upload.maximum_size = 8388608
board_build.partitions    = partitions.csv

build_flags =
    -DBOARD_HAS_PSRAM
    -DCORE_DEBUG_LEVEL=1

monitor_speed   = 115200
monitor_filters = esp32_exception_decoder, time
upload_speed    = 921600
```

`partitions.csv` (8 MB, single 4 MB app):

```
# Name,     Type, SubType,  Offset,   Size,      Flags
nvs,        data, nvs,      0x9000,   0x5000,
phy_init,   data, phy,      0xE000,   0x1000,
factory,    app,  factory,  0x10000,  0x400000,
storage,    data, spiffs,   0x410000, 0x3E0000,
coredump,   data, coredump, 0x7F0000, 0x10000,
```

---

## 2. Camera pins, the one-line form

The Arduino core already ships this board's map:

```cpp
#define CAMERA_MODEL_ESP32S3_EYE      // byte-for-byte correct for this board
#include "camera_pins.h"              // copy it from the core's CameraWebServer example
```

`camera_pins.h` is not on the include path by default — copy it out of
`framework-arduinoespressif32/libraries/ESP32/examples/Camera/CameraWebServer/` into your
project, or use the explicit form in §3, which is what `template/` does.

---

## 3. Camera init

```cpp
#include <esp_camera.h>

bool cameraBegin(void) {
  if (!psramFound()) {                     // check this FIRST — see rule 1
    Serial.println("no PSRAM: fix board_build.arduino.memory_type = qio_opi");
    return false;
  }

  camera_config_t cfg = {};
  cfg.ledc_channel = LEDC_CHANNEL_0;       // XCLK generator. Keep analogWrite off it.
  cfg.ledc_timer   = LEDC_TIMER_0;

  cfg.pin_pwdn  = -1;                      // not wired on this board
  cfg.pin_reset = -1;                      // not wired on this board
  cfg.pin_xclk  = 15;
  cfg.pin_sccb_sda = 4;
  cfg.pin_sccb_scl = 5;
  cfg.pin_d0 = 11; cfg.pin_d1 =  9; cfg.pin_d2 =  8; cfg.pin_d3 = 10;
  cfg.pin_d4 = 12; cfg.pin_d5 = 18; cfg.pin_d6 = 17; cfg.pin_d7 = 16;
  cfg.pin_vsync = 6;
  cfg.pin_href  = 7;
  cfg.pin_pclk  = 13;

  cfg.xclk_freq_hz = 20000000;             // drop to 10000000 if frames tear
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.frame_size   = FRAMESIZE_SVGA;
  cfg.jpeg_quality = 12;                   // lower = better = more PSRAM
  cfg.fb_count     = 2;                    // >1 requires PSRAM
  cfg.fb_location  = CAMERA_FB_IN_PSRAM;
  cfg.grab_mode    = CAMERA_GRAB_LATEST;   // WHEN_EMPTY for stills

  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    Serial.printf("esp_camera_init: 0x%x (%s)\n", err, esp_err_to_name(err));
    return false;                          // 0x101 = memory, 0x105 = no sensor on SCCB
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 1);                      // sensor is mounted upside down here
  s->set_hmirror(s, 1);
  return true;
}
```

Frame-size budget in PSRAM, JPEG, per buffer (approximate, scene-dependent):

| `frame_size` | Pixels | JPEG q12 | RGB565 |
|---|---|---|---|
| `FRAMESIZE_QVGA` | 320×240 | ~8 KB | 150 KB |
| `FRAMESIZE_VGA` | 640×480 | ~30 KB | 600 KB |
| `FRAMESIZE_SVGA` | 800×600 | ~45 KB | 937 KB |
| `FRAMESIZE_XGA` | 1024×768 | ~80 KB | 1.5 MB |
| `FRAMESIZE_UXGA` | 1600×1200 | ~180 KB | **3.7 MB** |

RGB565 at UXGA fits once in 8 MB and never twice.

---

## 4. Capture one frame

```cpp
camera_fb_t *fb = esp_camera_fb_get();
if (!fb) { Serial.println("capture failed"); return; }

// ... use fb->buf, fb->len, fb->width, fb->height, fb->format ...

esp_camera_fb_return(fb);      // ALWAYS, including on every error path
```

If the frame is not already JPEG:

```cpp
uint8_t *jpg = nullptr; size_t jpg_len = 0;
if (frame2jpg(fb, 80, &jpg, &jpg_len)) {
  // ... use jpg ...
  free(jpg);                   // frame2jpg mallocs; you own it
}
```

---

## 5. microSD over SDMMC, 1-bit

```cpp
#include <FS.h>
#include <SD_MMC.h>

bool sdBegin(void) {
  // setPins BEFORE begin. The other order silently uses the S3 defaults,
  // which overlap the octal PSRAM pins and reboot the board.
  if (!SD_MMC.setPins(39 /*CLK*/, 38 /*CMD*/, 40 /*D0*/)) return false;

  // mode1bit = true  : D1/D2/D3 are not routed on this board
  // format_if_empty  : false, unless the user asked to wipe their card
  if (!SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 5)) return false;

  return SD_MMC.cardType() != CARD_NONE;
}
```

Write a blob:

```cpp
File f = SD_MMC.open("/camera/1.jpg", FILE_WRITE);
if (f) { f.write(buf, len); f.close(); }
```

If a card that works elsewhere will not mount, try `SDMMC_FREQ_PROBING` (400 kHz) in the
fourth argument before suspecting the code.

---

## 6. Camera → SD, the whole path

```cpp
bool snap(const char *dir) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return false;

  bool ok = false;
  if (fb->format == PIXFORMAT_JPEG) {
    char path[64];
    snprintf(path, sizeof(path), "%s/%d.jpg", dir, nextIndex(dir));
    File f = SD_MMC.open(path, FILE_WRITE);
    if (f) { ok = f.write(fb->buf, fb->len) == fb->len; f.close(); }
  }
  esp_camera_fb_return(fb);
  return ok;
}
```

Throughput is the card, not the camera: a class-10 microSD sustains roughly 2-4 SVGA
JPEGs per second on this 1-bit bus. For bursts, collect into PSRAM first
(`heap_caps_malloc(n, MALLOC_CAP_SPIRAM)`) and flush afterwards.

---

## 7. MJPEG stream over Wi-Fi

```cpp
#include <WiFi.h>
#include <esp_http_server.h>
#include <esp_camera.h>

static esp_err_t streamHandler(httpd_req_t *req) {
  httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return ESP_FAIL;

    char part[64];
    int n = snprintf(part, sizeof(part),
                     "\r\n--frame\r\nContent-Type: image/jpeg\r\n"
                     "Content-Length: %u\r\n\r\n", (unsigned)fb->len);

    esp_err_t r = httpd_resp_send_chunk(req, part, n);
    if (r == ESP_OK) r = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);

    esp_camera_fb_return(fb);          // before the early return, not after
    if (r != ESP_OK) return r;
  }
}

void startStream(void) {
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  httpd_handle_t srv = nullptr;
  httpd_uri_t uri = { "/stream", HTTP_GET, streamHandler, nullptr };
  if (httpd_start(&srv, &cfg) == ESP_OK) httpd_register_uri_handler(srv, &uri);
}
```

Use `grab_mode = CAMERA_GRAB_LATEST` and `fb_count = 2` for streaming, or every viewer
sees a frame that is one capture stale. Expect ~12-20 fps at SVGA on 2.4 GHz Wi-Fi;
the limit is the radio, not the sensor.

---

## 8. Console on the native USB-C

Add to `build_flags`:

```ini
-DARDUINO_USB_CDC_ON_BOOT=1
-DARDUINO_USB_MODE=1
```

and give the host time to enumerate, or the first second of output is lost:

```cpp
Serial.begin(115200);
while (!Serial && millis() < 3000) delay(10);
```

The port becomes `/dev/cu.usbmodem*` (VID `303A`). UART0 on GPIO43/44 is then free as two
ordinary GPIOs — the only way to get the pin count above seven without giving up the
camera or the card.

---

## 9. I2C on pins that are actually free

```cpp
#include <Wire.h>

#define I2C_SDA 14      // free with camera + SD in use
#define I2C_SCL 21

Wire.begin(I2C_SDA, I2C_SCL, 400000);   // NEVER Wire.begin() with no arguments here:
                                        // the defaults are GPIO8/9 = CAM_D2/CAM_D1
```

Scanner, for confirming a device is alive before blaming the driver:

```cpp
for (uint8_t a = 1; a < 127; a++) {
  Wire.beginTransmission(a);
  if (Wire.endTransmission() == 0) Serial.printf("i2c device at 0x%02X\n", a);
}
```

Do **not** put your devices on GPIO4/5 — that is the camera's SCCB bus, and the camera
driver reprograms the sensor whenever settings change.

---

## 10. SPI on pins that are actually free

```cpp
#include <SPI.h>

SPIClass spi(FSPI);
spi.begin(21 /*SCK*/, 47 /*MISO*/, 14 /*MOSI*/, 41 /*SS*/);
```

Unlike the original ESP32, routing SPI through the S3's GPIO matrix costs almost nothing
below ~40 MHz, so any free pins will do. The IO_MUX pads for SPI2 are GPIO9-14 — all
camera pins on this board, so there is no faster option available anyway.

---

## 11. ADC

```cpp
// With the camera wired, the only free ADC1 channels are GPIO1, GPIO2, GPIO3.
// ADC2 (GPIO11-20) stops working the moment Wi-Fi starts, and analogRead() will
// not tell you — it just returns nonsense.
analogSetPinAttenuation(1, ADC_11db);          // 0 - 2900 mV, +/-50 mV
uint32_t mv = analogReadMilliVolts(1);         // uses the factory calibration curve
```

`analogReadMilliVolts()` applies the eFuse calibration; raw `analogRead()` does not and is
noticeably non-linear near both rails. Average 16 samples: DNL is ±4 LSB.

---

## 12. WS2812 on GPIO48

```cpp
neopixelWrite(48, r, g, b);      // core 2.0.14+, RMT under the hood, no library
```

Order is R, G, B in the call even though the pixel is physically GRB — the core handles
it. There is exactly one pixel. For animations use the Adafruit_NeoPixel or FastLED
library with `NEO_GRB + NEO_KHZ800`.

The plain LED, for comparison:

```cpp
pinMode(2, OUTPUT);
digitalWrite(2, HIGH);           // active HIGH
```

---

## 13. Deep sleep, woken by BOOT

```cpp
#include <esp_sleep.h>

esp_sleep_enable_ext0_wakeup((gpio_num_t)0, 0);   // GPIO0 = BOOT, wake on LOW
esp_sleep_enable_timer_wakeup(60ULL * 1000000ULL); // or after 60 s
esp_deep_sleep_start();
```

Only **GPIO0-21** are RTC-capable and can wake the chip. GPIO38-48 cannot, whatever you
configure. On this board that leaves GPIO0, 1, 2, 3, 14 and 21 once the camera and card
have taken the rest.

Expect **milliamps**, not microamps: the LDO, the CH343 and the power LED are on the same
rail as the module and none of them sleep.

---

## 14. Board report

The diagnostic that answers most questions in one paste — full version in
`template/src/board_report.cpp`:

```cpp
Serial.printf("chip   : ESP32-S3 rev %d, %d cores @ %lu MHz\n",
              chip.revision, chip.cores, (unsigned long)getCpuFrequencyMhz());
Serial.printf("flash  : %lu MB\n", (unsigned long)(flash_bytes / (1024 * 1024)));
Serial.printf("PSRAM  : %s %lu KB\n", psramFound() ? "OK" : "*** NOT FOUND ***",
              (unsigned long)(ESP.getPsramSize() / 1024));
Serial.printf("reset  : %d\n", (int)esp_reset_reason());

uint32_t in0 = REG_READ(GPIO_IN_REG), in1 = REG_READ(GPIO_IN1_REG);
Serial.printf("straps : IO0=%d IO3=%d IO45=%d IO46=%d\n",
              (int)((in0 >> 0) & 1), (int)((in0 >> 3) & 1),
              (int)((in1 >> 13) & 1), (int)((in1 >> 14) & 1));
```

`GPIO_IN1_REG` holds GPIO32-48, so GPIO45 is bit 13 and GPIO46 is bit 14 of it.

---

## 15. Wi-Fi station, with the brownout in mind

```cpp
#include <WiFi.h>

WiFi.mode(WIFI_STA);
WiFi.setSleep(false);                 // sleep costs latency and saves nothing here
WiFi.begin(ssid, password);
for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(250);
Serial.println(WiFi.localIP());
```

If the board resets during `WiFi.begin()`, read `esp_reset_reason()` before reading the
backtrace: `ESP_RST_BROWNOUT` means the 340 mA TX peak collapsed the rail, and no code
change fixes it.
