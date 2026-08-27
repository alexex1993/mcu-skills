# Copy-paste recipes — ProMicro nRF52840 (V1940, Adafruit nRF52 Arduino core)

Everything here builds against **platform-nordicnrf52 10.11.0 +
framework-arduinoadafruitnrf52 1.10700.0** (Adafruit core 1.7.0). Recipes 1–5
are extracted from `template/`, which is the source of truth — not retyped.
Items marked **⚠︎ compile-checked only** were built by the author but not
exercised on hardware.

## 1. platformio.ini

```ini
[env:promicro_nrf52840]
platform = nordicnrf52
board = promicro_nrf52840      ; resolved from boards/ in THIS project
framework = arduino

upload_protocol = nrfutil      ; DFU over the UF2 bootloader's serial port
monitor_speed = 115200         ; ignored by USB CDC; used by Serial1

build_flags =
    -Wall
; -DBLINK_ALL_LED_CANDIDATES=1   ; blink P0.15 + P0.26 + P0.30 to find the LED
```

## 2. boards/promicro_nrf52840.json

The board does not exist in PlatformIO; this file is what makes
`board = promicro_nrf52840` resolve. Full file in
`template/boards/promicro_nrf52840.json`; the load-bearing keys:

```jsonc
{
  "build": {
    "arduino": { "ldscript": "nrf52840_s140_v6.ld" },  // must match the SoftDevice
    "core": "nRF5",
    "cpu": "cortex-m4",
    // NFCT_PINS_AS_GPIOS — plural. The singular spelling matches nothing.
    "extra_flags": "-DARDUINO_NRF52840_PROMICRO -DNRF52840_XXAA -DCONFIG_NFCT_PINS_AS_GPIOS",
    "f_cpu": "64000000L",
    "mcu": "nrf52840",
    "variant": "promicro_nrf52840",
    "variants_dir": "boards/variants",   // relative to the PROJECT root
    "bsp": { "name": "adafruit" },
    "softdevice": {
      "sd_flags": "-DS140", "sd_name": "s140",
      "sd_version": "6.1.1", "sd_fwid": "0x00B6"   // 7.3.0 would be 0x0123
    },
    "bootloader": { "settings_addr": "0xFF000" }
  },
  "upload": {
    "maximum_ram_size": 237568,   // 0x20006000..0x20040000, per the linker script
    "maximum_size": 815104,       // 0x26000..0xED000
    "protocol": "nrfutil",
    "speed": 115200,
    "use_1200bps_touch": true,
    "require_upload_port": true,
    "wait_for_upload_port": true
  },
  "frameworks": ["arduino"],
  "name": "ProMicro nRF52840 (V1940 / nice!nano v2 compatible)"
}
```

## 3. The pin map (variant.cpp)

`g_ADigitalPinMap[arduino_pin] = 32 * port + pin`. This table is what every
`pinMode`, `Wire`, `SPI` and `analogRead` call goes through, so a wrong entry
misroutes a peripheral with no compile error. Full file in
`template/boards/variants/promicro_nrf52840/variant.cpp`:

```cpp
const uint32_t g_ADigitalPinMap[] =
{
   8,  //  D0 = P0.08  RX          6,  //  D1 = P0.06  TX
  17,  //  D2 = P0.17  SDA        20,  //  D3 = P0.20  SCL
  22,  //  D4 = P0.22             24,  //  D5 = P0.24
  32,  //  D6 = P1.00             11,  //  D7 = P0.11
  36,  //  D8 = P1.04             38,  //  D9 = P1.06
   9,  // D10 = P0.09  SS/NFC1
  15,  // D11 = P0.15  LED (not on the header)
  26,  // D12 = P0.26            30,  // D13 = P0.30   (LED candidates)
  43,  // D14 = P1.11  MISO      45,  // D15 = P1.13  SCK
  10,  // D16 = P0.10  MOSI/NFC2
  28,  // D17 = P0.28  (not on the header)
  47,  // D18 = P1.15  A0 — NO ADC
   2,  // D19 = P0.02  A1/AIN0
  29,  // D20 = P0.29  A2/AIN5
  31,  // D21 = P0.31  A3/AIN7
};
```

(Laid out two-per-line here for space; the file has one entry per line.)

The two `variant.h` lines that decide whether the board runs at all:

```c
#define USE_LFRC              // internal 32 kHz RC — always works
// #define USE_LFXO           // 32.768 kHz crystal — hangs if not populated

#define PIN_LED1     (11)     // P0.15
#define LED_STATE_ON  1       // active HIGH
```

## 4. Blink (minimal, no USB)

From `template/variants/minimal/main.cpp`. `ledOn`/`ledOff` apply
`LED_STATE_ON`, so this survives a polarity change in `variant.h`.

```cpp
#include <Arduino.h>

static uint32_t lastToggle = 0;
static bool     ledIsOn    = false;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  ledOff(LED_BUILTIN);
  lastToggle = millis();
}

void loop() {
  const uint32_t now = millis();
  if (now - lastToggle >= 500) {
    lastToggle = now;
    ledIsOn = !ledIsOn;
    ledIsOn ? ledOn(LED_BUILTIN) : ledOff(LED_BUILTIN);
  }
}
```

## 5. USB CDC + ADC + die temperature

From `template/src/main.cpp`. Three board-specific points: the TinyUSB
include, no `while (!Serial)`, and the ADC on A1 rather than A0.

```cpp
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>   // <-- this is what defines `Serial`

void setup() {
  Serial.begin(115200);         // never `while (!Serial);`
  analogReadResolution(12);     // core default is 10 bits of a 14-bit ADC
}

void loop() {
  const int   raw = analogRead(A1);          // P0.02/AIN0. A0 has NO ADC.
  const float mv  = raw * (3600.0f / 4096);  // full scale 3.6 V, not VDD
  Serial.printf("A1=%d (%.0f mV)  die=%.1f C\n", raw, mv, readCPUTemperature());
  delay(2000);
}
```

## 6. USB CDC and the hardware UART at the same time ⚠︎ compile-checked only

`Serial` and `Serial1` are unrelated objects — a USB-serial adapter on D0/D1
never sees `Serial.print`.

```cpp
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

void setup() {
  Serial.begin(115200);         // USB CDC
  Serial1.begin(9600);          // UARTE on D0 (P0.08 RX) / D1 (P0.06 TX)
}

void loop() {
  while (Serial1.available()) Serial.write(Serial1.read());
  while (Serial.available())  Serial1.write(Serial.read());
}
```

Moving the UART to other pins — nRF52 peripherals are routed by register, so
any GPIO works:

```cpp
Serial1.setPins(/* rx */ 4, /* tx */ 5);   // before begin()
Serial1.begin(115200);
```

## 7. I2C scan ⚠︎ compile-checked only

Default bus is D2 (P0.17) SDA / D3 (P0.20) SCL. **No pull-ups on the board** —
fit 4.7 kΩ to 3.3 V.

```cpp
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  // Wire.setPins(4, 5);   // optional: any two GPIOs, before begin()
  Wire.begin();
  Wire.setClock(400000);

  delay(2000);                       // give a host time to attach
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) Serial.printf("device at 0x%02X\n", addr);
  }
}

void loop() {}
```

## 8. SPI ⚠︎ compile-checked only

D15 SCK (P1.13), D16 MOSI (P0.10), D14 MISO (P1.11), SS on D10 (P0.09).
**MOSI and SS are the NFC pins** — this only works because the board JSON
defines `CONFIG_NFCT_PINS_AS_GPIOS` (plural; the singular spelling that most
clone JSONs carry is a no-op).

```cpp
#include <Arduino.h>
#include <SPI.h>

void setup() {
  pinMode(SS, OUTPUT);
  digitalWrite(SS, HIGH);
  SPI.begin();
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
}

uint8_t readRegister(uint8_t reg) {
  digitalWrite(SS, LOW);
  SPI.transfer(reg | 0x80);
  const uint8_t v = SPI.transfer(0x00);
  digitalWrite(SS, HIGH);
  return v;
}

void loop() {}
```

## 9. Analog input, properly scaled ⚠︎ compile-checked only

```cpp
#include <Arduino.h>

void setup() {
  analogReadResolution(12);            // 10 by default; 14 is the hw maximum
  analogOversampling(16);              // SAADC burst mode: hardware averaging
  // analogReference(AR_INTERNAL_3_0); // 3.0 V full scale instead of 3.6 V
  // analogReference(AR_VDD4);         // ratiometric to VDD instead
}

static uint32_t readMillivolts(uint8_t pin) {
  return (uint32_t)analogRead(pin) * 3600u / 4096u;   // default 3.6 V scale
}

void loop() {
  const uint32_t mv = readMillivolts(A1);   // A1/A2/A3 only — A0 has no ADC
  (void)mv;
  delay(100);
}
```

Usable analog pins on this footprint: **A1 = P0.02/AIN0, A2 = P0.29/AIN5,
A3 = P0.31/AIN7**. `A0` is P1.15 and has no SAADC channel at all.

## 10. BLE UART bridged to USB ⚠︎ compile-checked only

From `template/variants/ble/main.cpp`. `Bluefruit.begin()` starts the S140
SoftDevice already on the board — and is the exact line that hangs if
`variant.h` says `USE_LFXO` on a board without the crystal.

```cpp
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <bluefruit.h>

BLEUart bleuart;

void setup() {
  Serial.begin(115200);

  Bluefruit.begin();
  Bluefruit.setTxPower(4);          // -40 -20 -16 -12 -8 -4 0 +2..+8 only
  Bluefruit.setName("ProMicro-nRF52840");
  bleuart.begin();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);   // × 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);     // seconds
  Bluefruit.Advertising.start(0);               // 0 = forever
}

void loop() {
  while (bleuart.available()) Serial.write((char)bleuart.read());
  while (Serial.available())  bleuart.write((char)Serial.read());
}
```

Cost: 128,676 B flash / 15,588 B RAM, against 20,196 B / 3,092 B for recipe 4.
`Bluefruit.Periph.clearBonds()` drops stored bonds — the fix for a peer that
will not reconnect after a firmware change.

## 11. InternalFS (LittleFS on internal flash) ⚠︎ compile-checked only

28 KB at `0xED000`, shared with BLE bonding data.

```cpp
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

using namespace Adafruit_LittleFS_Namespace;

void setup() {
  Serial.begin(115200);
  InternalFS.begin();

  File f(InternalFS);
  if (f.open("/boot.txt", FILE_O_WRITE)) {
    f.write("hello\n", 6);
    f.close();
  }
  // InternalFS.format();   // also destroys stored BLE bonds
}

void loop() {}
```

## 12. Deep sleep ⚠︎ compile-checked only

`systemOff()` reaches the nRF52840's System OFF state (~0.4 µA on the chip;
this board's regulator sets the real floor, and it is not documented for
V1940). It **never returns** — the chip resets on wake.

```cpp
#include <Arduino.h>

void goToSleep(uint8_t wakePin) {
  pinMode(wakePin, INPUT_PULLUP);
  // wake on the pin going LOW; execution resumes from reset, not from here
  systemOff(wakePin, LOW);
}
```

Note that USB keeps the HFCLK and USB peripheral running whenever a host is
attached, so any current measured over USB measures the USB stack. Measure
from the battery pads, with a build that has no CDC in it (recipe 4).

## 13. High-drive GPIO (15 mA) ⚠︎ compile-checked only

The Arduino API has no high-drive setting; go at `PIN_CNF` directly. Remember
`g_ADigitalPinMap` translates Arduino pin → `32*port + pin`.

```cpp
#include <Arduino.h>
#include <nrf.h>

static void setHighDrive(uint8_t arduinoPin) {
  const uint32_t p = g_ADigitalPinMap[arduinoPin];
  NRF_GPIO_Type *port = (p < 32) ? NRF_P0 : NRF_P1;
  const uint32_t pin  = p & 0x1F;

  port->PIN_CNF[pin] = (port->PIN_CNF[pin] & ~GPIO_PIN_CNF_DRIVE_Msk)
                     | (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos);
}
```

## 14. Converting firmware.hex to UF2

For drag-and-drop flashing, or for a machine with no PlatformIO:

```sh
# uf2conv.py from https://github.com/microsoft/uf2 (tools/uf2conv.py)
python3 uf2conv.py -c -f 0xADA52840 \
    .pio/build/promicro_nrf52840/firmware.hex \
    -o firmware.uf2
# double-tap RESET, then copy firmware.uf2 to the NICENANO/PROMICRO drive
```

`0xADA52840` is the UF2 family ID for "nRF52840 with the Adafruit bootloader".
A UF2 built for the wrong family is silently ignored by the bootloader.

## 15. Switching to a board with SoftDevice S140 7.x

Only if the bootloader rejects your DFU package. Both keys change together,
and core 1.7.0 has no `nrf52840_s140_v7.ld`, so the core has to move too:

```jsonc
// boards/promicro_nrf52840.json
"arduino":    { "ldscript": "nrf52840_s140_v7.ld" },
"softdevice": { "sd_flags": "-DS140", "sd_name": "s140",
                "sd_version": "7.3.0", "sd_fwid": "0x0123" }
```

```ini
; platformio.ini — a core new enough to ship the v7 linker script
platform_packages =
    framework-arduinoadafruitnrf52 @ https://github.com/adafruit/Adafruit_nRF52_Arduino.git#master
```

⚠︎ Untested by the author — the reference board here is S140 6.1.1.
