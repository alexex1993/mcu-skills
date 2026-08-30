# NodeMCU 30-pin ESP8266 — recipes

Code that compiles. Everything here was built with platform-espressif8266 4.2.1 /
Arduino core 3.1.2 / xtensa-gcc 10.3.0 for `board = nodemcuv2`, zero warnings; §1–§5 are
extracted from `template/`, not retyped.

Pin constants come from `template/include/board.h`. Read `board-hardware.md` §1 before
picking pins — five of the nine usable GPIOs are unconditionally safe and four are not.

---

## 1. `platformio.ini`

The template's, trimmed to the load-bearing lines:

```ini
[env:nodemcuv2]
platform  = espressif8266
board     = nodemcuv2
framework = arduino

board_build.ldscript   = eagle.flash.4m1m.ld   ; 1,044,464 B sketch + 1 MB FS
board_build.filesystem = littlefs              ; SPIFFS is deprecated in core 3.x

monitor_speed   = 115200
monitor_filters = esp8266_exception_decoder, time
; monitor_dtr = 0                              ; attach without resetting the board
; monitor_rts = 0

upload_speed = 460800                          ; 921600 is unreliable on CH340 clones

build_flags =
    -Wall
    -D BOARD_HAS_LED_ON_GPIO16=1
```

Variants worth knowing:

```ini
board_build.f_cpu       = 160000000L   ; 160 MHz, ~15 mA more
board_build.f_flash     = 80000000L    ; 80 MHz XIP; not every module is stable
board_build.flash_mode  = dout         ; frees GPIO9/GPIO10 as real GPIOs
build_flags = -D DEBUG_ESP_PORT=Serial -D DEBUG_ESP_WIFI   ; SDK Wi-Fi logging
```

The ROM boot log is at **74880** baud regardless of any of this:
`pio device monitor -b 74880`.

---

## 2. Blink, both LEDs (from `template/variants/minimal/main.cpp`)

Both LEDs are active LOW. `LED_BUILTIN` (GPIO2) is on the module; `LED_BUILTIN_AUX`
(GPIO16) is on the NodeMCU PCB and absent from bare ESP-12 breakouts.

```cpp
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\nNodeMCU alive: reset=%s core=%s cpu=%u MHz flash=%u B\n",
                ESP.getResetReason().c_str(), ESP.getCoreVersion().c_str(),
                ESP.getCpuFreqMHz(), ESP.getFlashChipRealSize());
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_BUILTIN_AUX, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, LOW);          // LOW = lit
  digitalWrite(LED_BUILTIN_AUX, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(LED_BUILTIN_AUX, LOW);
  delay(500);
}
```

Non-blocking form, for anything that also runs Wi-Fi (from `template/src/main.cpp`):

```cpp
static uint32_t last = 0;
static bool lit = false;
uint32_t now = millis();
if (now - last >= (uint32_t)(lit ? 100 : 900)) {
  last = now;
  lit = !lit;
  digitalWrite(LED_BUILTIN, lit ? LOW : HIGH);
}
```

---

## 3. Is the GPIO16 ↔ RST deep-sleep link fitted? (from `template/src/main.cpp`)

Run this before writing any deep-sleep code. It uses RTC user memory, which survives a
reset but not a power cycle, so it can distinguish "GPIO16 low rebooted me" from
"GPIO16 low did nothing".

```cpp
struct RtcProbe { uint32_t magic; uint32_t stage; };
static const uint32_t PROBE_MAGIC = 0x8266C0D7;

static const char *gpio16_rst_link() {
  RtcProbe p;
  ESP.rtcUserMemoryRead(0, (uint32_t *)&p, sizeof(p));

  if (p.magic == PROBE_MAGIC && p.stage == 1) {
    p.stage = 2;
    ESP.rtcUserMemoryWrite(0, (uint32_t *)&p, sizeof(p));
    return "FITTED (deep sleep will wake)";
  }
  if (p.magic == PROBE_MAGIC && p.stage == 2)
    return "FITTED (latched; power-cycle to retest)";

  p.magic = PROBE_MAGIC;
  p.stage = 1;
  ESP.rtcUserMemoryWrite(0, (uint32_t *)&p, sizeof(p));
  Serial.flush();
  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);      // if the link is fitted, this resets the chip
  delay(5);
  digitalWrite(16, HIGH);
  delay(20);
  p.stage = 0;                // survived — re-arm for the next boot
  ESP.rtcUserMemoryWrite(0, (uint32_t *)&p, sizeof(p));
  return "NOT fitted (ESP.deepSleep never wakes)";
}
```

---

## 4. Board report (from `template/src/main.cpp`)

The five lines that answer most "why is it doing that" questions:

```cpp
Serial.printf("reset reason  : %s\n", ESP.getResetReason().c_str());
Serial.printf("boot mode     : %u (1 = normal flash boot)\n", ESP.getBootMode());
Serial.printf("flash size    : %u B real / %u B configured\n",
              ESP.getFlashChipRealSize(), ESP.getFlashChipSize());
Serial.printf("heap free     : %u B (frag %u%%, largest block %u B)\n",
              ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize());
Serial.printf("straps        : GPIO0=%d GPIO2=%d GPIO15=%d (want 1 1 0)\n",
              digitalRead(0), digitalRead(2), digitalRead(15));
```

A `flash size` mismatch means the board or the ldscript is wrong and the filesystem will
land in the wrong place. Straps other than `1 1 0` mean something you wired is fighting
the board's resistors.

---

## 5. LittleFS (from `template/src/main.cpp`)

```cpp
#include <LittleFS.h>

if (LittleFS.begin()) {                       // formats automatically on first mount
  uint32_t boots = 0;
  File f = LittleFS.open("/boots.bin", "r");
  if (f) { f.read((uint8_t *)&boots, sizeof(boots)); f.close(); }
  boots++;
  f = LittleFS.open("/boots.bin", "w");
  if (f) { f.write((const uint8_t *)&boots, sizeof(boots)); f.close(); }

  FSInfo info;
  LittleFS.info(info);
  Serial.printf("boot #%u, %u/%u B used\n", boots, info.usedBytes, info.totalBytes);
}
```

Listing a directory, and shipping files from `data/` with `pio run -t uploadfs`:

```cpp
Dir dir = LittleFS.openDir("/");
while (dir.next())
  Serial.printf("%-24s %u B\n", dir.fileName().c_str(), (unsigned)dir.fileSize());
```

`board_build.filesystem = littlefs` must be set, or `uploadfs` writes a SPIFFS image that
`LittleFS.begin()` reformats away on first boot.

---

## 6. Interrupts

The handler and everything it calls must be `IRAM_ATTR`, or the chip panics with
`ISR not in IRAM!` the first time it fires during a flash read. **GPIO16 cannot generate
interrupts at all.**

```cpp
volatile uint32_t g_edges = 0;
static volatile uint32_t g_last_us = 0;

static void IRAM_ATTR on_edge() {
  uint32_t now = micros();
  if (now - g_last_us < 5000) return;   // 5 ms debounce, inside the ISR
  g_last_us = now;
  g_edges++;
}

void setup_button(uint8_t pin) {
  pinMode(pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pin), on_edge, FALLING);
}
```

Read `g_edges` from `loop()`, not from the ISR. No `Serial`, no `delay`, no `new`, no
`String` inside an ISR.

---

## 7. PWM — set the range explicitly

`analogWrite`'s range **defaults to 255 in core 3.x** (it was 1023 in 2.x). Code written
against a 2.x tutorial saturates.

```cpp
void setup_pwm(uint8_t pin) {
  analogWriteRange(1023);      // say what you mean; the default is 255
  analogWriteFreq(1000);       // clamped to 100 Hz .. 60 kHz
  pinMode(pin, OUTPUT);
}

void fade(uint8_t pin) {
  for (int v = 0; v <= 1023; v += 8) { analogWrite(pin, v); delay(8); }
  for (int v = 1023; v >= 0; v -= 8) { analogWrite(pin, v); delay(8); }
  analogWrite(pin, 0);
}
```

This is a timer ISR, not a peripheral: expect visible jitter whenever Wi-Fi transmits, and
do not use it for servos.

---

## 8. ADC

One channel. The header pin is 0–3.2 V through the board's 220 kΩ/100 kΩ divider; the
chip's pad behind it is 0–1.0 V.

```cpp
static float read_a0_volts() {
  uint32_t acc = 0;
  for (int i = 0; i < 16; i++) { acc += analogRead(A0); delay(1); }
  return (acc / 16) * 3.2f / 1023.0f;      // 3.2 V full scale at the HEADER pin
}
```

To measure the 3.3 V rail instead — mutually exclusive with the above, and A0 must then be
left floating:

```cpp
ADC_MODE(ADC_VCC);                          // at file scope, outside any function
// ...
Serial.printf("vcc %u mV\n", ESP.getVcc());
```

---

## 9. I2C — software, so the pins are free

```cpp
#include <Wire.h>

void setup_i2c() {
  Wire.begin(4, 5);            // SDA = GPIO4 (D2), SCL = GPIO5 (D1) — the variant default
  Wire.setClock(100000);       // 400000 works on short buses with real pull-ups
}

void i2c_scan() {
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("device at 0x%02X\n", addr);
      found++;
    }
    yield();                   // scanning 126 addresses is long enough to matter
  }
  if (!found) Serial.println(F("no I2C devices — check pins and 4.7k pull-ups to 3V3"));
}
```

Any two safe GPIOs work — `Wire.begin(12, 14)` is just as valid. There are no usable
internal pull-ups; fit 4.7 kΩ to **3V3**, never to 5 V.

---

## 10. SPI (HSPI)

Pins are fixed by the hardware: SCLK GPIO14 (D5), MISO GPIO12 (D6), MOSI GPIO13 (D7).
Drive CS yourself from any GPIO — preferably **not** GPIO15, which is a strapping pin.

```cpp
#include <SPI.h>

static const uint8_t CS = 15;   // D8; if a slave holds this high at reset, boot fails

void setup_spi() {
  pinMode(CS, OUTPUT);
  digitalWrite(CS, HIGH);
  SPI.begin();
}

uint8_t spi_read_reg(uint8_t reg) {
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  digitalWrite(CS, LOW);
  SPI.transfer(reg | 0x80);
  uint8_t v = SPI.transfer(0x00);
  digitalWrite(CS, HIGH);
  SPI.endTransaction();
  return v;
}
```

---

## 11. Wi-Fi station, non-blocking

Never busy-wait for a connection: the SDK needs `loop()` to return.

```cpp
#include <ESP8266WiFi.h>

void setup_wifi(const char *ssid, const char *pass) {
  WiFi.persistent(false);        // stop rewriting credentials to flash every boot
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, pass);
}

void poll_wifi() {
  static wl_status_t prev = WL_IDLE_STATUS;
  wl_status_t now = WiFi.status();
  if (now != prev) {
    prev = now;
    if (now == WL_CONNECTED)
      Serial.printf("connected: %s  rssi %d  heap %u\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI(), ESP.getFreeHeap());
    else
      Serial.printf("wifi status %d\n", (int)now);
  }
}
```

`WiFi.persistent(false)` matters: the default writes the SSID and password to the system
parameter sectors on every `begin()`, which wears the flash on a board that reboots often.

A reset the moment the radio first transmits is the power supply, not this code.

---

## 12. Deep sleep

**Requires the GPIO16 → RST link**, which a stock NodeMCU does not have (§3, and
`board-hardware.md` §8). Without it the board sleeps and never returns.

```cpp
// 512 B of user RTC RAM survive deep sleep; they do not survive power-off.
struct SleepState { uint32_t magic, cycles; };
static const uint32_t SLEEP_MAGIC = 0xD0D0BEEF;

void sleep_for_minutes(uint32_t minutes) {
  SleepState s;
  ESP.rtcUserMemoryRead(0, (uint32_t *)&s, sizeof(s));
  if (s.magic != SLEEP_MAGIC) { s.magic = SLEEP_MAGIC; s.cycles = 0; }
  s.cycles++;
  ESP.rtcUserMemoryWrite(0, (uint32_t *)&s, sizeof(s));

  Serial.printf("sleep cycle %u; max single sleep is %.2f h\n",
                s.cycles, ESP.deepSleepMax() / 3600e6);
  Serial.flush();                              // the UART FIFO is not drained for you
  ESP.deepSleep((uint64_t)minutes * 60ULL * 1000000ULL, WAKE_RF_DEFAULT);
}
```

`ESP.deepSleepMax()` is ~3.2 h; longer intervals need a chain of sleeps counted in RTC
memory. `WAKE_RF_DISABLED` skips RF calibration on wake and saves ~100 ms and a current
spike when the wake-up does not need the radio.

While the link is fitted, **uploads fail** — lift the jumper first.

---

## 13. Keeping the watchdog fed

`loop()` must return. Anything longer than ~3 s without `delay()` or `yield()` produces
`Soft WDT reset`; ~8 s produces a hardware `wdt reset`.

```cpp
// Wrong: trips the soft watchdog.
while (!sensor_ready()) { }

// Right:
uint32_t deadline = millis() + 5000;
while (!sensor_ready() && (int32_t)(millis() - deadline) < 0)
  delay(1);                    // delay() yields to the SDK; yield() alone also works

// Long computation: yield every so often.
for (uint32_t i = 0; i < 2000000; i++) {
  work(i);
  if ((i & 0x3FF) == 0) yield();
}
```

`ESP.wdtDisable()` only stops the *software* watchdog; the hardware one keeps running and
will reset at ~8 s regardless.

---

## 14. Reading a crash

Set the decoder once and it turns every panic into source lines:

```ini
monitor_filters = esp8266_exception_decoder, time
```

Then a crash prints the exception cause, the failing PC and a decoded backtrace instead of
raw addresses. Common causes:

| | |
|---|---|
| `Exception (28)` | load from a null or freed pointer |
| `Exception (9)` | unaligned access — often a `struct` cast onto a byte buffer |
| `Exception (0)` | illegal instruction, usually a corrupted function pointer |
| `ISR not in IRAM!` | an interrupt handler missing `IRAM_ATTR` (§6) |
| `Soft WDT reset` | §13 |
| `stack smashing` / garbage backtrace | stack overflow; the ESP8266 stack is ~4 KB |

`ESP.getResetReason()` at the top of `setup()` tells you which of these happened *last*
boot, which is the only way to catch a crash that happens before the monitor attaches.

---

## 15. OTA update

```cpp
#include <ArduinoOTA.h>

void setup_ota(const char *hostname) {
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.onStart([]() { Serial.println(F("ota start")); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("ota error %u\n", e); });
  ArduinoOTA.begin();
}

// in loop():
ArduinoOTA.handle();
```

The new image has to fit **beside** the running one — `ESP.getFreeSketchSpace()` is the
budget, and it is not the 1,044,464 B link ceiling. A sketch over ~half the app region
cannot update itself over the air.

---

## 16. Second serial port for logs

UART1 is TX-only on GPIO2 — which is also the module LED and a strapping pin, so use it
when you need the console free and can give up both.

```cpp
Serial1.begin(115200);            // TX on GPIO2 (D4), no RX
Serial1.println(F("debug log"));
```

Or move UART0 off the bridge entirely, onto GPIO15 (TX) / GPIO13 (RX):

```cpp
Serial.begin(115200);
Serial.swap();                    // UART0 now on D8 / D7
```

---

## 17. WS2812 / NeoPixel

Bit-banged WS2812 output glitches whenever Wi-Fi interrupts it. The I2S DMA engine is the
only route that does not.

```ini
lib_deps = makuna/NeoPixelBus @ ^2.7.9
```

```cpp
#include <NeoPixelBus.h>

// I2S DMA method — output is fixed to GPIO3 (RX / D9), which UART0 also uses.
NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> strip(30);

void setup_pixels() {
  strip.Begin();
  strip.ClearTo(RgbColor(0, 0, 0));
  strip.Show();
}
```

Using the DMA method costs you serial *input* (GPIO3), not output. If the console must
stay fully usable, `NeoEsp8266Uart1800KbpsMethod` drives GPIO2 instead — the module LED
pin.
