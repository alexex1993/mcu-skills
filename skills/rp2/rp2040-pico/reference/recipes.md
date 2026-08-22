# Copy-paste recipes — Raspberry Pi Pico (RP2040, arduino-pico core)

All recipes build against platform-raspberrypi 1.19.0 + arduino-pico 5.6.0
(earlephilhower core, `board_build.core = earlephilhower`). Recipes 1-2 are
extracted from `template/`, not retyped — the template is the source of
truth. Items marked **⚠︎ compile-checked only** were built but not run on
hardware by the author.

## 1. platformio.ini

```ini
[env:pico]
platform = raspberrypi
board = pico
framework = arduino
board_build.core = earlephilhower   ; without this you get the Mbed core:
                                    ; no EEPROM.h, no Wire1, 500 Hz PWM
monitor_speed = 115200              ; baud ignored by USB CDC
build_flags =
    -Wall
```

## 2. board.h

See `template/include/board.h` — it is the pin map and power-fact header,
kept in one place. The load-bearing lines:

```c
#define BOARD_PIN_LED        25   /* on = HIGH, not on the header       */
#define BOARD_PIN_UART0_TX    0   /* Serial1                            */
#define BOARD_PIN_UART0_RX    1
#define BOARD_PIN_SDA         4   /* Wire default                       */
#define BOARD_PIN_SCL         5
#define BOARD_PIN_SS         17
#define BOARD_PIN_MOSI       19
#define BOARD_PIN_MISO       16
#define BOARD_PIN_SCK        18
#define BOARD_PIN_ADC_A0     26   /* A0-A2 on the header; A3 = VSYS/3   */
#define BOARD_PIN_VBUS_SENSE 24   /* HIGH while USB power present       */
#define BOARD_PIN_SMPS_PS    23   /* HIGH forces SMPS PWM mode          */
#define BOARD_PIN_VSYS_ADC   29   /* A3: reads VSYS/3                   */
```

## 3. USB CDC and hardware UART at the same time ⚠︎ compile-checked only

```cpp
void setup()
{
    Serial.begin(115200);        // USB CDC — baud ignored, port appears
                                 // when the host opens it
    Serial1.begin(115200);       // UART0 on GPIO0(TX)/GPIO1(RX), real baud

    Serial.println(F("goes to the USB port"));
    Serial1.println(F("goes to the header pins"));
}
```

Do not block on `while (!Serial)` — it gates startup on the monitor being
open. Prints before the host opens the port are dropped.

## 4. Non-blocking heartbeat (extracted from template/src/main.cpp)

```cpp
static uint32_t last_beat_ms = 0;
static bool led_state = false;

void loop()
{
    const uint32_t now = millis();
    if (now - last_beat_ms >= 500u) {
        last_beat_ms = now;
        led_state = !led_state;
        digitalWrite(BOARD_PIN_LED, led_state);   // GPIO25, HIGH = on
    }
}
```

## 5. ADC: external pin, die temperature, VSYS battery monitor

Extracted from the template's report block:

```cpp
analogReadResolution(12);          // native 12 bits; default is 10-bit!

int raw = analogRead(A0);          // GPIO26, 0-4095, ratiometric to 3.3 V
float volts = raw * 3.3f / 4095.0f;

float die_c = analogReadTemp();    // on-chip sensor, degrees C

// A3 (GPIO29) = VSYS/3 through the on-board divider — battery monitor:
float vsys = analogRead(A3) * 3.3f / 4095.0f * 3.0f;

bool usb_powered = digitalRead(24);          // VBUS sense pin
pinMode(24, INPUT);                          // do once in setup()
```

Accuracy envelope (datasheet §4.3): reference is filtered 3V3, ~30 mV
built-in offset, ENOB 8.7 bits, DNL spikes at 512/1536/2560/3584. Average,
or cancel offset with a grounded second channel. For best results drive
GPIO23 HIGH (SMPS PWM mode) while sampling and low afterwards.

## 6. PWM: analogWrite, frequency, resolution, the slice rule ⚠︎ compile-checked only

```cpp
analogWrite(15, 128);             // any GPIO, 8-bit, 1 kHz default
analogWriteFreq(25000);           // 100 Hz .. 10 MHz, clamped outside
analogWriteResolution(12);        // 2 .. 16 bits
analogWrite(15, 2048);            // now 0..4095
```

PWM slice = pin/2: GPIO14 and GPIO15 share slice 7 — one frequency per
slice, duty per pin. Two pins on one slice with different frequencies:
last `analogWriteFreq()` wins for both.

## 7. I2C — Wire, and Wire1 off the ADC pins ⚠︎ compile-checked only

```cpp
Wire.begin();                     // i2c0: SDA=4, SCL=5 (defaults)

Wire1.setSDA(18);                 // i2c1 defaults to 26/27, which are A0/A1
Wire1.setSCL(19);
Wire1.begin();                    // now on 18/19 (I2C1 also fits 2/3, 6/7, ...)
```

## 8. SPI ⚠︎ compile-checked only

```cpp
SPI.begin();                      // spi0: SS=17, MOSI=19, MISO=16, SCK=18
// SPI1.begin();                  // spi1: SS=13, MOSI=15, MISO=12, SCK=14
```

## 9. EEPROM — flash emulation with commit() (extracted from template)

```cpp
#include <EEPROM.h>

struct boot_log_t { uint32_t boots; uint32_t last_uptime_s; };

EEPROM.begin(sizeof(boot_log_t)); // map the shadow (max 4 KB)
boot_log_t log;
EEPROM.get(0, log);               // reads never wear the sector
if (log.boots == 0xFFFFFFFFul) { log.boots = 0; log.last_uptime_s = 0; }
log.boots++;
EEPROM.put(0, log);               // put() = update semantics, stages only
EEPROM.commit();                  // one 4 KB sector erase IF bytes changed
```

There is no physical EEPROM: this is the last flash sector
(`0x101ff000`). Each changed commit is a sector erase — 100 k cycle
endurance; don't commit in a fast loop.

## 10. Interrupts on any GPIO ⚠︎ compile-checked only

```cpp
void isr() { /* fast, set a flag */ }

void setup()
{
    pinMode(21, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(21), isr, FALLING);  // any GPIO
}
```

## 11. Second core ⚠︎ compile-checked only

```cpp
volatile int core1_ticks = 0;

void setup1() { }                 // defining setup1/loop1 launches core 1

void loop1()
{
    core1_ticks++;                // no yield needed: loop1 IS the scheduler
}

void loop()
{
    // safe-ish: int writes are atomic on this core. For more, use the
    // inter-core FIFO: rp2040.fifo.write()/read(), 8 deep each way.
}
```

## 12. BOOTSEL as a runtime input ⚠︎ compile-checked only

```cpp
if (BOOTSEL) {                   // reads the button at runtime (Bootsel.h):
    // ...                       // it grounds QSPI SS — the core handles flash
}                                // access races, do not fight it manually
```

## 13. Reboot into BOOTSEL from code ⚠︎ compile-checked only

```cpp
#include <pico/bootrom.h>
reset_usb_boot(0, 0);             // what the 1200-baud touch triggers;
                                  // board re-enumerates as RPI-RP2
```
