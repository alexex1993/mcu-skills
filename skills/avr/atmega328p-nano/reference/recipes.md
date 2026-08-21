# Copy-paste recipes — Arduino Nano (ATmega328P)

Extracted from `template/`, which builds clean against platform-atmelavr
5.3.0 + Arduino AVR core. Anything that is **not** in the template is marked
**⚠︎ compile-checked only** — it compiles, but it has not been run on
hardware by the author of this skill. Only the minimal-variant Blink ran on
a real board.

Contents: [1 platformio.ini](#1-platformioini) · [2 board.h](#2-boardh) ·
[3 non-blocking timing](#3-non-blocking-timing) · [4 serial + free RAM](#4-serial-report-and-free-ram) ·
[5 EEPROM](#5-eeprom-with-update-semantics) · [6 ADC and references](#6-adc-and-references) ·
[7 external interrupts](#7-external-interrupts-d2d3) · [8 PWM and the timer map](#8-pwm-and-the-timer-map) ·
[9 Servo vs tone()](#9-servo-vs-tone-timer-conflicts) · [10 power-down sleep](#10-power-down-sleep) ·
[11 flash-resident tables](#11-flash-resident-tables-progmem)

---

## 1. platformio.ini

```ini
[env:nano_atmega328p]
platform = atmelavr
board = nanoatmega328new             ; Optiboot: genuine boards sold since Jan 2018
framework = arduino
monitor_speed = 115200

; Old-bootloader boards (pre-2018 genuine, most clones) speak 57600, not
; 115200 — uncomment to fix "not in sync" / stk500_recv upload failures:
;upload_speed = 57600

build_flags =
    -Wall
```

`nanoatmega328new` and `nanoatmega328` are the same hardware; the difference
is the bootloader preset (Optiboot 115200 vs ATmegaBOOT 57600) used for
upload and Burn Bootloader. Both report 30,720 B usable flash and 2,048 B
RAM.

## 2. board.h

```c
#define BOARD_PIN_LED      13    /* "L" LED, on = HIGH; also SPI SCK        */
#define BOARD_PIN_TX       1     /* wired to the USB-serial chip            */
#define BOARD_PIN_RX       0     /* keep wiring off it during upload        */
#define BOARD_PIN_INT0     2     /* attachInterrupt(digitalPinToInterrupt())*/
#define BOARD_PIN_INT1     3     /* + PWM ~490 Hz on Timer2                 */
#define BOARD_PIN_PWM_D3   3     /* Timer2 — lost while tone() plays        */
#define BOARD_PIN_PWM_D5   5     /* Timer0 ~980 Hz — Timer0 IS millis()     */
#define BOARD_PIN_PWM_D6   6     /* Timer0 ~980 Hz — never reconfigure      */
#define BOARD_PIN_PWM_D9   9     /* Timer1 — lost while Servo is attached   */
#define BOARD_PIN_PWM_D10  10    /* Timer1; also SPI slave-select           */
#define BOARD_PIN_PWM_D11  11    /* Timer2; also SPI MOSI                   */
#define BOARD_PIN_SDA      A4    /* = 18 */
#define BOARD_PIN_SCL      A5    /* = 19 */
#define BOARD_PIN_SS       10
#define BOARD_PIN_MOSI     11
#define BOARD_PIN_MISO     12
#define BOARD_PIN_SCK      13    /* + LED_BUILTIN                           */
#define BOARD_PIN_ADC_A0   A0    /* A0–A5 double as digital pins 14–19      */
#define BOARD_PIN_ADC_A6   A6    /* analog-input ONLY — no digital port     */
#define BOARD_PIN_ADC_A7   A7    /* analog-input ONLY                       */
```

The full version with per-line comments is `template/include/board.h`.

## 3. Non-blocking timing

`delay()` stops everything; Timer0 and `millis()` keep running through this
pattern. The `uint32_t` subtraction below is rollover-safe.

```cpp
uint32_t last_beat_ms = 0;
bool led_state = false;

void loop()
{
    const uint32_t now = millis();

    if (now - last_beat_ms >= 500) {   /* toggling every 500 ms = 1 Hz blink */
        last_beat_ms = now;
        led_state = !led_state;
        digitalWrite(LED_BUILTIN, led_state);
    }
}
```

## 4. Serial report and free RAM

The `F()` macro keeps string literals in flash — without it every printed
string also occupies SRAM, and 2 KB fills up fast. The free-RAM probe is the
early-warning for the heap/stack collision that otherwise shows up as
corrupted variables days later.

```cpp
void setup()
{
    Serial.begin(115200);
    Serial.print(F("up, free "));
    Serial.print(free_ram());
    Serial.println(F(" B"));
}

static int free_ram(void)
{
    extern int __heap_start, *__brkval;
    int v;
    return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
```

Below ~200 B free, restructure before adding anything: the part has no MPU,
so nothing catches the overflow.

## 5. EEPROM with update semantics

`EEPROM.put()` internally uses update: only bytes that actually changed are
written, so a once-per-boot counter is safe but a once-per-loop counter is
not (100 k cycles/cell endurance).

```cpp
#include <EEPROM.h>

struct boot_log_t {
    uint32_t boots;
    uint32_t last_uptime_s;
};

boot_log_t log;
EEPROM.get(0, log);              /* reads never wear the cell */
if (log.boots == 0xFFFFFFFFul)   /* first run on a blank chip */
    log = { 0, 0 };
log.boots++;
EEPROM.put(0, log);              /* writes only the changed bytes */
```

The struct lives in EEPROM's own address space (1 KB, 0x000–0x3FF) — never
paste `EEPROM.get()` addresses from SRAM-based examples without checking the
offset stays below 1024 minus `sizeof(log)`.

## 6. ADC and references

⚠︎ compile-checked only

```cpp
int raw = analogRead(A0);        /* 0–1023, 10-bit, ~100 µs, works A0..A7 */

/* Averaging — cheap noise reduction */
uint16_t adcAvg(uint8_t pin, uint8_t n)
{
    uint32_t sum = 0;
    for (uint8_t i = 0; i < n; i++)
        sum += analogRead(pin);
    return (uint16_t)(sum / n);
}

/* Internal 1.1 V reference for ratiometric sensors on 3.3 V rails etc. */
analogReference(INTERNAL);       /* DEFAULT = 5 V, INTERNAL = 1.1 V, EXTERNAL = AREF */
int millivolts = (int)((analogRead(A1) * 1100UL) / 1023);
```

`analogReference(EXTERNAL)` must be called **before** any external voltage
touches AREF — driving AREF while an internal reference is selected damages
the chip. AREF itself must never exceed 5 V.

## 7. External interrupts (D2/D3)

⚠︎ compile-checked only

The only two hardware-interrupt pins on the board. `digitalPinToInterrupt()`
is not optional on AVR cores.

```cpp
volatile uint32_t events = 0;

void onEvent()
{
    events++;                    /* keep ISRs this short */
}

void setup()
{
    pinMode(2, INPUT_PULLUP);    /* internal pull-up is 20–50 kΩ */
    attachInterrupt(digitalPinToInterrupt(2), onEvent, FALLING);
}
```

For more than two wake sources use pin-change interrupts (`PCINT0–2` vectors
via the `PinChangeInt` library or raw `PCICR`/`PCMSK` registers) — every pin
has a PCINT, at the cost of a shared vector.

## 8. PWM and the timer map

⚠︎ compile-checked only

```cpp
analogWrite(9, 128);             /* 8-bit duty on D3, D5, D6, D9, D10, D11 */
```

What each analogWrite-pin actually costs, because the timers are shared:

| Pin | ~Frequency | Timer | Also owned by that timer |
|---|---|---|---|
| D5, D6 | 980 Hz | Timer0 | `millis()`, `micros()`, `delay()` — **untouchable** |
| D9, D10 | 490 Hz | Timer1 | `Servo` library |
| D3, D11 | 490 Hz | Timer2 | `tone()` |

Timer1 fast PWM (e.g. ~3.13 kHz audible-quiet motor drive) is the one safe
reconfiguration:

```cpp
#include <avr/interrupt.h>

/* Timer1: fast PWM 8-bit, prescaler 1 → 16 MHz / 256 / 1 = 62.5 kHz.
   Prescaler 8 gives 7.8 kHz, 64 gives 977 Hz. Kills the Servo library. */
TCCR1A = _BV(WGM10);                       /* fast PWM 8-bit            */
TCCR1B = _BV(WGM12) | _BV(CS10);           /* prescaler 1               */
TIMSK1 = 0;                                /* no interrupts needed      */
/* analogWrite(9/10, value) now outputs at the new frequency */
```

Never write TCCR0A/TCCR0B: `millis()` silently drifts and `delay()` lies.

## 9. Servo vs tone() timer conflicts

⚠︎ compile-checked only (Servo 1.3.0 from the PlatformIO registry)

**PlatformIO quirk:** the Arduino IDE bundles `Servo` with the AVR boards
package, but PlatformIO's `framework-arduino-avr` ships only EEPROM, HID,
SoftwareSerial, SPI and Wire — `#include <Servo.h>` is a fatal "No such file
or directory" until it is added. Same for `EEPROM`-adjacent IDE-only extras
like `ArduinoISP`. Add the library to the project:

```ini
; platformio.ini
lib_deps = arduino-libraries/Servo
```

Both libraries are usable together — they take different timers — but each
kills the PWM pins behind its timer while active.

```cpp
#include <Servo.h>

Servo servo;

void setup()
{
    servo.attach(9);       /* Timer1: D9/D10 analogWrite() dead from here  */
    servo.write(90);

    tone(8, 440);          /* Timer2: D3/D11 analogWrite() dead from here  */
    /* ... */
    noTone(8);             /* D3/D11 PWM returns */
    servo.detach();        /* D9/D10 PWM returns */
}
```

`tone()` can output on any digital pin — the pin is just toggled by Timer2.

## 10. Power-down sleep

⚠︎ compile-checked only

Power-down is the deepest mode (µA-range for the MCU; the board's regulator
and USB chip still draw their share). Wake from it needs a LOW level on INT0
— RISING/FALLING do not work in power-down.

```cpp
#include <avr/sleep.h>
#include <avr/power.h>

void wakeISR() { /* nothing — waking is the job */ }

void sleepUntilD2Low()
{
    pinMode(2, INPUT_PULLUP);
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    attachInterrupt(digitalPinToInterrupt(2), wakeISR, LOW);
    sleep_mode();          /* sleeps here; execution resumes after wake     */
    sleep_disable();
    detachInterrupt(digitalPinToInterrupt(2));
}
```

`power_all_disable()` / `power_all_enable()` (from `<avr/power.h>`) turn off
peripheral clocks around the sleep for extra savings.

## 11. Flash-resident tables (PROGMEM)

⚠︎ compile-checked only

AVR is Harvard-architecture: a `const` array still lands in SRAM unless told
otherwise. 256-byte lookup tables are an eighth of the whole RAM.

```cpp
#include <avr/pgmspace.h>

const uint8_t sine8[16] PROGMEM = {
    128, 177, 218, 246, 255, 246, 218, 177,
    128,  79,  38,  10,   0,  10,  38,  79,
};

uint8_t v = pgm_read_byte(&sine8[i & 0x0F]);   /* never index it directly */
```

The same rule at string level is the `F()` macro from recipe 4.
