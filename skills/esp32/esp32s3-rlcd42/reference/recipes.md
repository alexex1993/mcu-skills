# Recipes — ESP32-S3-RLCD-4.2

Code that is known to build. Everything in §1–§5 is extracted from `template/`, which
compiles clean with PlatformIO Core 6.2.0 / platform-espressif32 55.3.311 / Arduino core
3.3.11 / U8g2 2.36.18. §6 and §7 are **not** in the template and are marked accordingly.

Pins come from `template/include/board_pins.h`; the reasoning behind them is in
`board-hardware.md` §2.

---

## 1. platformio.ini

Five of these settings correct the `esp32-s3-devkitc-1` board definition, which is an N8
with no PSRAM. All five are silent when wrong.

```ini
[env:esp32-s3-rlcd-4_2]
platform  = espressif32
board     = esp32-s3-devkitc-1
framework = arduino

board_build.arduino.memory_type = qio_opi
board_build.flash_mode          = qio
board_build.partitions          = default_16MB.csv
board_upload.flash_size         = 16MB
board_upload.maximum_size       = 16777216

build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1

monitor_speed = 115200

lib_deps =
    olikraus/U8g2@^2.36.18
```

**Do not "fix" this to `^2.36.19`.** Waveshare's docs ask for "U8g2 v2.36.19 or later" and
upstream's ChangeLog lists ST7305 300×400 under 2.36.19 — but the **PlatformIO registry's
newest U8g2 is 2.36.18**, and that package already contains the constructors (verified by
compiling). `^2.36.19` fails to resolve. In the Arduino IDE's Library Manager, where 2.36.19
does exist, Waveshare's floor is the right one.

---

## 2. Console over native USB

There is no UART bridge chip, so this is the only console.

```cpp
Serial.begin(115200);
// The CDC device does not exist until the host enumerates it. Without this
// wait the boot banner is printed into a void and the board looks dead.
uint32_t t0 = millis();
while (!Serial && millis() - t0 < 2000) delay(10);
```

---

## 3. The reflective panel

### 3.1 Bring-up

```cpp
#include <SPI.h>
#include <U8g2lib.h>

// 300x400 native; U8G2_R1 rotates to the 400x300 landscape the board is read
// in. Full-frame ("_F_") buffer = 15,000 bytes, comfortably internal SRAM.
static U8G2_ST7305_300X400_F_4W_HW_SPI lcd(U8G2_R1, /*CS*/40, /*DC*/5, /*RST*/41);

void displayBegin() {
  // U8g2 uses the global SPI object but never assigns its pins, and the S3
  // defaults are not this board's. Claiming the bus first makes U8g2's own
  // SPI.begin() a no-op. Skip this and the panel gets no data, silently.
  SPI.begin(/*SCLK*/11, /*MISO*/-1, /*MOSI*/12, /*SS*/-1);
  lcd.begin();
  // GPIO matrix (not IO MUX) caps SPI2 at ~40 MHz; the ST7305 wants a write
  // period >= 30 ns (~33 MHz). 24 MHz clears both.
  lcd.setBusClock(24000000);
}
```

If the image comes out upside down for your enclosure, swap `U8G2_R1` for `U8G2_R3`.

### 3.2 Drawing

Ordinary U8g2. `getDisplayWidth()` is 400 and `getDisplayHeight()` is 300 under `R1`.

```cpp
lcd.clearBuffer();
lcd.drawBox(0, 0, 400, 40);          // inverted title bar
lcd.setDrawColor(0);
lcd.setFont(u8g2_font_helvB14_tr);
lcd.drawStr(20, 27, "ESP32-S3-RLCD-4.2");
lcd.setDrawColor(1);
lcd.setFont(u8g2_font_logisoso32_tn);
lcd.drawStr(20, 118, "21.4");
lcd.sendBuffer();
```

Monochrome: `setDrawColor(1)` draws pixels on, `0` draws them off (for text over a filled
box). There is no greyscale through the ST7305 driver.

### 3.3 Power modes — the part U8g2 does not wrap

```cpp
void displayEnterLowPower() { lcd.sendF("c", 0x39); delay(100); }  // LPM, ~1 Hz
void displayLeaveLowPower() { lcd.sendF("c", 0x38); delay(100); }  // HPM, ~32 Hz
```

- **`setPowerSave(1)` is not LPM.** It sends `0x28` (display off) and the image vanishes.
- Writing while in LPM works but can take a full refresh period (~1 s) to become visible.
  Fast redraw is HPM → draw → `sendBuffer()` → LPM.
- The image persists with no host involvement, so there is no reason to redraw on a timer
  unless the content changed.

### 3.4 Continuous refresh

Stay in HPM and never switch. Waveshare measures ≈45 `sendBuffer()`/s at 24 MHz; the
panel's own self-refresh stays at 32 Hz regardless, so writing faster buys nothing.

---

## 4. The shared I2C bus

Four slaves on one bus. Passing the pins is mandatory: the S3 Arduino variant defaults
`Wire` to SDA 8 / SCL 9, which on this board are I2S data and bit clock.

```cpp
#include <Wire.h>
Wire.begin(/*SDA*/13, /*SCL*/14, 100000);
```

| Address | Chip |
|---|---|
| `0x18` | ES8311 speaker codec |
| `0x40` (some batches `0x42`) | ES7210 microphone ADC |
| `0x51` | PCF85063A RTC |
| `0x70` | SHTC3 temperature/humidity |

An external device on the header's `SDA`/`SCL` must avoid all of those.

### 4.1 SHTC3

```cpp
static bool shtc3Cmd(uint16_t cmd) {
  Wire.beginTransmission(0x70);
  Wire.write((uint8_t)(cmd >> 8));
  Wire.write((uint8_t)(cmd & 0xFF));
  return Wire.endTransmission() == 0;
}

bool shtc3Read(float *tempC, float *rh) {
  if (!shtc3Cmd(0x3517)) return false;   // wake
  delayMicroseconds(300);                // 240 us minimum
  if (!shtc3Cmd(0x7CA2)) return false;   // normal mode, T first
  delay(13);                             // 12.1 ms max conversion
  if (Wire.requestFrom(0x70, 6) != 6) return false;
  uint8_t b[6];
  for (int i = 0; i < 6; i++) b[i] = Wire.read();
  shtc3Cmd(0xB098);                      // sleep, ~0.6 uA
  *tempC = -45.0f + 175.0f * (float)(((uint16_t)b[0] << 8) | b[1]) / 65535.0f;
  *rh    =         100.0f * (float)(((uint16_t)b[3] << 8) | b[4]) / 65535.0f;
  return true;
}
```

`b[2]` and `b[5]` are CRC-8 bytes, ignored here. Leaving the part asleep between reads is
what keeps it at sub-microamp; it also stops it self-heating, which otherwise biases the
reading a few tenths of a degree high.

### 4.2 PCF85063A

```cpp
static uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }

bool pcf85063ReadTime(struct tm *out) {
  Wire.beginTransmission(0x51);
  Wire.write(0x04);                             // Seconds register
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(0x51, 7) != 7) return false;
  uint8_t sec = Wire.read(), min = Wire.read(), hour = Wire.read();
  uint8_t day = Wire.read(), wday = Wire.read();
  uint8_t mon = Wire.read(), year = Wire.read();

  // Bit 7 of Seconds is OS: set means the oscillator stopped and the time is
  // not trustworthy.
  bool ok = (sec & 0x80) == 0;

  out->tm_sec = bcd2dec(sec & 0x7F);   out->tm_min  = bcd2dec(min & 0x7F);
  out->tm_hour = bcd2dec(hour & 0x3F); out->tm_mday = bcd2dec(day & 0x3F);
  out->tm_wday = wday & 0x07;          out->tm_mon  = bcd2dec(mon & 0x1F) - 1;
  out->tm_year = bcd2dec(year) + 100;  // struct tm counts from 1900
  return ok;
}
```

**Expect `ok == false` on a board out of the box.** The PH1.0 backup holder ships empty,
so the RTC loses time on every power cycle. Treat the OS flag as "sync from NTP now"
rather than as a fault. Only a **rechargeable** cell belongs in that holder — the board
charges it.

`INT` is on GPIO15, active low, if you want alarms rather than polling.

---

## 5. Battery

```cpp
analogSetPinAttenuation(4, ADC_11db);   // ~3.1 V full scale, not the ~0.95 V default

float batteryVolts() {
  uint32_t acc = 0;
  for (int i = 0; i < 8; i++) acc += analogReadMilliVolts(4);
  return (float)acc / 8.0f / 1000.0f * 3.0f;   // R21 200K / R23 100K, 1%
}
```

Waveshare's own BSP does the same thing through the IDF API: `ADC_UNIT_1`, `ADC_CHANNEL_3`,
`ADC_ATTEN_DB_12`, `ADC_BITWIDTH_12`, curve-fitting calibration, then `0.001 * mV * 3`.

A full 18650 at 4.2 V presents ~1.40 V at the pin — above the default full scale, so
without the attenuation call every reading saturates at the same number. GPIO4 is
**ADC1**_CH3, so it survives Wi-Fi being up; ADC2 does not.

Rough state of charge: **2.5 V empty, 4.2 V full**, clamped (the ESPHome tutorial's curve).
Waveshare's own firmware is tighter — **3.0 V = 0%, 4.12 V = 100%**. Either is defensible;
say which you used.

---

## 6. microSD — SDMMC, 1-bit

**Not in `template/`, and not run on hardware.** The pins are confirmed three ways — the
schematic, the Zephyr board port, and Waveshare's own Arduino BSP, which declares
`CustomSDPort(name, clk = 38, cmd = 21, d0 = 39, width = 1)` over
`esp_vfs_fat_sdmmc_mount()`. Only the Arduino `SD_MMC` wrapper below is untested.

```cpp
#include <SD_MMC.h>

bool sdBegin() {
  // MUST come before begin(). Called after, it returns true, changes nothing,
  // and begin() uses the S3 defaults — which overlap the octal PSRAM pins.
  if (!SD_MMC.setPins(/*CLK*/38, /*CMD*/21, /*D0*/39)) return false;

  // mode1bit = true is not optional: D1/D2/D3 are not routed on this board.
  // format_if_empty = false unless the user actually asked for the opposite.
  return SD_MMC.begin("/sdcard", /*mode1bit*/true, /*format_if_empty*/false,
                      SDMMC_FREQ_DEFAULT);
}
```

- **There is no CS pin.** The card's `CD/D3` sits on a 10 K pull-up and reaches GPIO17 only
  through `R7`, which is not populated — so the card comes up in native SD mode and `SD.h`
  over a `SPI` bus cannot work, whatever CS you pass.
- **There is no card-detect either** — the socket's CD pin is tied to ground. Poll a mount
  attempt if you need to know whether a card is in.
- Cards that will not mount at `SDMMC_FREQ_DEFAULT` sometimes mount at
  `SDMMC_FREQ_PROBING`. The Zephyr port caps the bus at 20 MHz.
- FAT32 only.
- Mounting takes MTCK (GPIO39), so pad-JTAG is gone. Use the
  USB-Serial-JTAG on the Type-C port instead; it costs no pins.

---

## 7. Audio — ES8311 + ES7210

**Not in `template/`, and not verified.** Both codecs need a register-level init sequence
that this skill has not run on hardware, so no Arduino code is offered rather than code
that might be wrong.

The pin map and the two facts that matter:

```
MCLK 16 · BCLK 9 · LRCK 45 · DOUT 8 (ESP -> ES8311, playback)
                            · DIN 10 (ES7210 -> ESP, capture)
PA enable: GPIO46, ACTIVE HIGH — low means silence, no matter what else is right
```

- **Directions.** The vendor table names pins from the codec's side (`DSDIN` into the
  codec, `ASDOUT` out of it). From the host: GPIO8 is an output, GPIO10 an input. The
  schematic confirms it — GPIO8 to ES8311 pin 9 `DSDIN`, ES7210 pin 11 `SDOUT1` through
  `R42` (51 R) to GPIO10. The Zephyr pinctrl has them swapped and its `i2s0` node is never
  enabled — do not use it.
- **Raise GPIO46 before initialising the ES8311**, per a user who got the chain working.
- **GPIO45 and GPIO46 are strapping pins** as well as audio pins. See `esp32s3-soc.md` §3.

The route with the most evidence behind it is ESPHome, whose config for this board is
published by Waveshare and reproduced here as the pin/parameter reference:

```yaml
i2s_audio:
  - id: i2s_shared
    i2s_lrclk_pin: GPIO45
    i2s_bclk_pin: GPIO9
    i2s_mclk_pin: GPIO16
audio_dac:
  - platform: es8311
    id: es8311_dac
    bits_per_sample: 16bit
    sample_rate: 16000
speaker:
  - platform: i2s_audio
    id: spk
    i2s_audio_id: i2s_shared
    i2s_dout_pin: GPIO8
    dac_type: external
    audio_dac: es8311_dac
audio_adc:
  - platform: es7210
    id: es7210_adc
    bits_per_sample: 16bit
    sample_rate: 16000
    mic_gain: 24dB
microphone:
  - platform: i2s_audio
    id: mic
    i2s_audio_id: i2s_shared
    i2s_din_pin: GPIO10
    adc_type: external
    pdm: false
switch:
  - platform: gpio
    pin: GPIO46
    restore_mode: RESTORE_DEFAULT_ON   # the amplifier enable
```

**Start from Waveshare's own code**, not from scratch:
`github.com/waveshareteam/ESP32-S3-RLCD-4.2` has `02_Example/Arduino/07_Audio_Test` plus
ESP-IDF and XiaoZhi variants and a prebuilt factory firmware. For ESP-IDF generally, the
maintained path is Espressif's `esp_codec_dev` component, which ships ES8311 and ES7210
drivers; wire it to `I2S_NUM_0` with the pins above and drive GPIO46 yourself.

---

## 8. Picking a pin for something new

The free list on the header is four pins: **GPIO1, GPIO2, GPIO17** and **GPIO3** — the
last only if you give its floating JTAG-source strap a defined level at reset. GPIO43/44
are a fifth and sixth as long as the console stays on USB, which it must anyway.

**GPIO7, GPIO42, GPIO47 and GPIO48 are connected to nothing** on this board, but none is
brought out to the header — module pads only.

For anything wider, put an I2C expander on the header's `SDA`/`SCL` at an address that is
not `0x18`, `0x40`, `0x42`, `0x51` or `0x70`. Do not reach for GPIO35/36/37 (octal PSRAM),
GPIO19/20 (USB), GPIO0 (BOOT) or GPIO18 (KEY, button still fitted in parallel).

---

## 9. Deep sleep

Nothing on this board fights deep sleep, and the panel is the reason to bother: it holds
its image with the SoC powered down entirely.

```cpp
displayDrawDashboard(...);      // draw the final frame
displayEnterLowPower();         // 0x39 — the panel keeps the image at ~1 Hz
esp_sleep_enable_timer_wakeup(15ULL * 60 * 1000000);
esp_sleep_enable_ext0_wakeup((gpio_num_t)18, 0);   // KEY, active low
esp_deep_sleep_start();
```

GPIO18 (KEY) and GPIO0 (BOOT) are both RTC-capable and both idle high, which makes them
usable as `ext0`/`ext1` wake sources with no extra hardware. Waking is a full reset, so
`setup()` runs again — check `esp_reset_reason() == ESP_RST_DEEPSLEEP` and skip the parts
that do not need repeating. The panel does **not** need to be redrawn on wake if the
content has not changed.
