# Copy-paste recipes — Beetle (Mini Arduino Leonardo, ATmega32U4)

Code that is known to compile against `platform = atmelavr` 5.3.0 +
`framework-arduino-avr`, `board = leonardo`. Recipes 1–4, 13 and the HID
guards in 12 are extracted from `../template/`, which builds clean in all
three variants. Everything marked **⚠︎ compile-checked only** was compiled
against the same core but not run on hardware by the author of this skill.
Nothing here has been run on a physical Beetle — see SKILL.md §Reporting.

Pin names throughout come from `../template/include/board.h`. The pad
inventory is `../reference/board-hardware.md` §2.

---

## 1. platformio.ini

```ini
[env:beetle]
platform = atmelavr
board = leonardo            ; no "beetle" board exists; Leonardo is exact —
                            ; same Caterina bootloader, same USB VID/PID,
                            ; 16 MHz, avr109, 28672 B flash, 2560 B SRAM
framework = arduino
monitor_speed = 115200      ; a formality: Serial is USB CDC, no baud rate

build_flags =
    -Wall

; Deliberately NO upload_port. The board is two different serial ports —
; the sketch's (PID 0x8036) and the bootloader's (PID 0x0036) — and the
; upload hands off between them. Pinning either one breaks the handshake.

; Libraries that the Arduino IDE bundles and PlatformIO does not:
;lib_deps =
;    arduino-libraries/Keyboard
;    arduino-libraries/Mouse
;    arduino-libraries/Servo
```

## 2. board.h — the pads, and the watchdog line

The full file is `../template/include/board.h`. The load-bearing part:

```c
#define BOARD_PIN_RX        0    /* PD2  Serial1 RXD1, INT2                  */
#define BOARD_PIN_TX        1    /* PD3  Serial1 TXD1, INT3                  */
#define BOARD_PIN_SDA       2    /* PD1  Wire SDA, INT1                      */
#define BOARD_PIN_SCL       3    /* PD0  Wire SCL, INT0, PWM (Timer0 OC0B)   */
#define BOARD_PIN_D9        9    /* PB5  PWM (Timer1 OC1A), ADC12 = A9       */
#define BOARD_PIN_D10      10    /* PB6  PWM (Timer1 OC1B), ADC13 = A10      */
#define BOARD_PIN_D11      11    /* PB7  PWM (Timer0 OC0A), PCINT7           */
#define BOARD_PIN_A0       A0    /* = D18, PF7, ADC7  — also JTAG TDI        */
#define BOARD_PIN_A1       A1    /* = D19, PF6, ADC6  — also JTAG TDO        */
#define BOARD_PIN_A2       A2    /* = D20, PF5, ADC5  — also JTAG TMS        */
#define BOARD_PIN_LED      LED_BUILTIN  /* = 13, PC7, on = HIGH, no pad      */

/* the same four pads under their plain Arduino numbers, for GPIO use */
#define BOARD_PIN_D0       BOARD_PIN_RX
#define BOARD_PIN_D1       BOARD_PIN_TX
#define BOARD_PIN_D2       BOARD_PIN_SDA
#define BOARD_PIN_D3       BOARD_PIN_SCL
```

Every `setup()` on this board starts with these two lines. Caterina uses the
watchdog to hand control to the sketch; a sketch that leaves it armed with a
short timeout reset-loops faster than the 8 s bootloader window can be caught.

```c
#include <avr/wdt.h>

void setup()
{
    MCUSR = 0;
    wdt_disable();
    /* ... */
}
```

## 3. Non-blocking timing

The default shape of every loop here. `delay()` in `loop()` is not just
sloppy — it is time during which the 1200 bps touch cannot be serviced, so a
long `delay()` makes the board harder to re-flash.

```c
#define HEARTBEAT_MS 500u

static uint32_t last_beat_ms = 0;
static bool     led_state    = false;

void loop()
{
    const uint32_t now = millis();

    if (now - last_beat_ms >= HEARTBEAT_MS) {   /* unsigned subtraction:
                                                   correct across the ~49-day
                                                   millis() rollover */
        last_beat_ms = now;
        led_state = !led_state;
        digitalWrite(BOARD_PIN_LED, led_state);
    }
}
```

## 4. USB CDC that never blocks

The three forms, in order of how much trouble they cause.

```c
/* WRONG — hangs forever on a board that is not plugged into a computer.
   On an ATmega328P this line is a no-op, which is why it gets copied here. */
Serial.begin(115200);
while (!Serial);

/* Acceptable — waits for a host, but always gives up. */
Serial.begin(115200);
const uint32_t deadline = millis() + 2000u;
while (!Serial && millis() < deadline) { }

/* Best — never waits at all; prints only when someone is listening.
   `if (Serial)` is the CDC line state, not "is USB compiled in".
   WARNING: operator bool() contains a delay(10) in the core — every test
   costs 10 ms. Testing once per loop() pass caps the loop at 100 Hz. */
Serial.begin(115200);
...
if (Serial) {
    Serial.print(F("up "));      /* F() keeps the literal in flash — the
                                    2,560 B of SRAM is the real budget */
    Serial.println(millis() / 1000);
}
```

Free-RAM probe, ~20 B of flash, the only honest way to know how close the
heap and stack are:

```c
static int free_ram(void)
{
    extern int __heap_start, *__brkval;
    int v;
    return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
```

Below ~250 B free, restructure rather than add. The template's full variant
runs at 164 B of static RAM, leaving most of the 2,560 B for heap and stack.

## 5. `Serial1` — the real UART on the TX/RX pads ⚠︎ compile-checked only

Unlike an Uno or Nano, where D0/D1 are wired to the USB-serial chip, the
Beetle's TX/RX pads are a **free hardware UART**. Baud rate matters here.

```c
void setup()
{
    Serial.begin(115200);      /* USB CDC — to the host           */
    Serial1.begin(9600);       /* TX = D1 (PD3), RX = D0 (PD2)    */
}

void loop()
{
    /* bridge the two: USB console <-> external device */
    while (Serial1.available())  Serial.write(Serial1.read());
    while (Serial.available())   Serial1.write(Serial.read());
}
```

Prefer this over `SoftwareSerial` on any pad: it costs no CPU and does not
break when interrupts are blocked. Note that D0 and D1 are also INT2 and
INT3 — you get the UART or the interrupts, not both.

## 6. I2C on SDA=D2 / SCL=D3 ⚠︎ compile-checked only

```c
#include <Wire.h>

void setup()
{
    Wire.begin();              /* SDA = D2 (PD1), SCL = D3 (PD0) */
    Wire.setClock(100000);     /* 400000 for fast mode           */
}

static bool read_reg(uint8_t addr, uint8_t reg, uint8_t *out)
{
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;  /* repeated START */
    if (Wire.requestFrom(addr, (uint8_t)1) != 1) return false;
    *out = Wire.read();
    return true;
}
```

Two Beetle-specific costs:

- **SCL (D3) is also a PWM pad.** Using `Wire` leaves three PWM pads.
- The AVR's internal pull-ups are 20–50 kΩ — far too weak for I2C at any
  useful speed. Fit external **4.7 kΩ** pull-ups to the 5 V rail. Symptom of
  not doing it: works with one short jumper, fails on a longer cable or a
  second device.

An I2C scanner is the fastest way to prove wiring:

```c
for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0 && Serial) {
        Serial.print(F("found 0x")); Serial.println(a, HEX);
    }
}
```

## 7. SPI on the test pads ⚠︎ compile-checked only

The SPI signals are **not on castellated pads** — they are on the six dots on
the back, which DFRobot document as a standard 6-pin ICSP interface
(board-hardware.md §3.3): 1=MISO, 2=VCC, 3=SCK, 4=MOSI, 5=RESET, 6=GND.

Note that **hardware SS (D17/PB0) is not on that header**, so chip-select has
to come from an edge pad anyway — which is fine, the SPI library only needs SS
to be an output.

```c
#include <SPI.h>

/* Chip-select on a real pad, because D17/PB0 (the hardware SS) is a test
   pad AND the RX LED. Any of the ten pads works as CS. */
#define CS_PIN  BOARD_PIN_D10

void setup()
{
    pinMode(CS_PIN, OUTPUT);
    digitalWrite(CS_PIN, HIGH);
    SPI.begin();               /* SCK = D15/PB1, MISO = D14/PB3,
                                  MOSI = D16/PB2, SS = D17/PB0 */
}

static uint8_t xfer_reg(uint8_t reg, uint8_t val)
{
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    digitalWrite(CS_PIN, LOW);
    SPI.transfer(reg);
    const uint8_t r = SPI.transfer(val);
    digitalWrite(CS_PIN, HIGH);
    SPI.endTransaction();
    return r;
}
```

Using D10 as CS costs you a Timer1 PWM pad; D11 (Timer0 PWM) or A0–A2 are
usually the cheaper choice.

## 8. ADC — the channel map is the whole recipe

Five channels reach the pads, and two of them are the D9/D10 PWM pads wearing
a different name.

| Constant | Pad | Port | ADC channel |
|---|---|---|---|
| `A0` | `A0` | PF7 | 7 |
| `A1` | `A1` | PF6 | 6 |
| `A2` | `A2` | PF5 | 5 |
| `A9` | `9` | PB5 | 12 |
| `A10` | `10` | PB6 | 13 |

```c
const int raw = analogRead(A0);            /* 0..1023 over 0..5 V     */
const int on_d9 = analogRead(A9);          /* the "9" pad, not "D9"   */

/* mV, using the default AVCC reference (~5 V, i.e. whatever USB gives) */
const uint32_t mv = (uint32_t)raw * 5000ul / 1023ul;
```

**Never pass a bare digital pin number.** `analogRead()` on this core treats
its argument as an *analog channel index* for values below 18, so
`analogRead(2)` samples A2/PF5 rather than the SDA pad — silently, with a
plausible-looking number.

`analogReference()`: leave it at `DEFAULT`. `EXTERNAL` needs the AREF pin,
which is not bonded on this board, and selecting it without a reference
voltage gives readings of zero.

The default reference is AVCC, i.e. **whatever the `+` rail happens to be** —
and DFRobot only guarantee 4.5–5 V. A reading converted with a hard-coded
5000 mV is wrong by however far the rail has sagged; over USB with a load on
the board, several percent is normal. Ratiometric sensors (potentiometers,
most resistive dividers) are immune because they scale with the same rail;
absolute measurements are not.

Each conversion takes ~100 µs. Reading five channels in a tight loop is
500 µs of blocked USB service — space them out.

## 9. Interrupts ⚠︎ compile-checked only

Four external interrupts reach the pads — twice what an Uno offers — but the
pin-to-`INTn` mapping is scrambled, so `digitalPinToInterrupt()` is mandatory.

| Pad | Vector |
|---|---|
| D3 (SCL) | INT0 |
| D2 (SDA) | INT1 |
| D0 (RX) | INT2 |
| D1 (TX) | INT3 |

```c
volatile uint16_t edge_count = 0;    /* volatile, and read atomically below */

static void on_edge(void)            /* ISRs: short, no Serial, no delay */
{
    edge_count++;
}

void setup()
{
    pinMode(BOARD_PIN_D2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BOARD_PIN_D2), on_edge, FALLING);
}

/* A 16-bit read is two instructions on an 8-bit core — without the guard,
   an interrupt landing between them returns a torn value. */
static uint16_t read_count(void)
{
    uint16_t v;
    noInterrupts();
    v = edge_count;
    interrupts();
    return v;
}
```

Pin-change interrupts cover D9, D10, D11 (PCINT5/6/7). All three share the
single `PCINT0_vect`, so the handler must work out which pin moved:

```c
#include <avr/interrupt.h>

void setup_pcint(void)
{
    PCICR  |= (1 << PCIE0);                              /* enable PCINT0..7 */
    PCMSK0 |= (1 << PCINT5) | (1 << PCINT6) | (1 << PCINT7);   /* D9,D10,D11 */
}

ISR(PCINT0_vect)
{
    /* fires on BOTH edges of ANY enabled pin — compare against a saved
       snapshot of PINB to find out what changed */
}
```

Keep every ISR short. USB is serviced by interrupts too: a handler or a
`noInterrupts()` window longer than a few hundred µs drops USB frames and the
host reports a flaky device, not a slow one.

## 10. PWM and the timer map

```c
analogWrite(BOARD_PIN_D9,  128);   /* Timer1 OC1A, ~490 Hz */
analogWrite(BOARD_PIN_D10, 200);   /* Timer1 OC1B, ~490 Hz */
analogWrite(BOARD_PIN_D11,  64);   /* Timer0 OC0A, ~977 Hz */
analogWrite(BOARD_PIN_SCL,  32);   /* Timer0 OC0B, ~977 Hz — D3 */
```

| Pad | Timer | Freq | Reconfigurable? |
|---|---|---|---|
| D3, D11 | **Timer0** | **~977 Hz** (fast PWM, /64) | **Never** — Timer0 *is* `millis()`, `micros()`, `delay()` |
| D9, D10 | Timer1 | **~490 Hz** (phase-correct, /64) | Yes, if no `Servo` is attached |

The two default frequencies differ because the core sets the two timers to
different PWM modes in `wiring.c`. If you are matching pitch across pads —
motor whine, LED flicker on camera — you cannot, without reprogramming
Timer1 (below) and leaving Timer0 alone.

Higher-frequency PWM therefore only exists on D9/D10, via Timer1 in fast-PWM
mode. 16-bit, so pick ICR1 for the frequency you want: ⚠︎ compile-checked only

```c
/* ~1 kHz, 10-bit resolution, on D9 (OC1A) and D10 (OC1B).
   f = 16 MHz / (prescale * (ICR1 + 1)) = 16e6 / (8 * 2000) = 1000 Hz */
void timer1_fast_pwm_1khz(void)
{
    pinMode(BOARD_PIN_D9,  OUTPUT);
    pinMode(BOARD_PIN_D10, OUTPUT);

    TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM11);
    TCCR1B = (1 << WGM13)  | (1 << WGM12)  | (1 << CS11);   /* /8 */
    ICR1   = 1999;
    OCR1A  = 1000;      /* 50 % on D9  — write OCR1A/OCR1B directly now, */
    OCR1B  =  500;      /* 25 % on D10   analogWrite() would undo the mode */
}
```

Doing the same thing to Timer0 for D3/D11 breaks `delay()`, `millis()`,
`Serial1` framing and every bit-banged protocol simultaneously, with no error
anywhere. Move the load to D9/D10 instead.

## 11. `Servo` vs `tone()` ⚠︎ compile-checked only

On this chip the two do not conflict with each other — but `Servo` conflicts
badly with the board's pad set.

| Library | Timer on ATmega32U4 | Costs you |
|---|---|---|
| `Servo` | **Timer1 only** (`ServoTimers.h`) | `analogWrite()` on **D9 and D10** |
| `tone()` | **Timer3** (`Tone.cpp`) | OC3A = D5, **which is not a Beetle pad** — nothing |

So `tone()` is free here, and one `attach()` halves your PWM.

```c
#include <Servo.h>          /* lib_deps = arduino-libraries/Servo */

Servo arm;

void setup()
{
    arm.attach(BOARD_PIN_D9);    /* from here on, analogWrite(9) and
                                    analogWrite(10) do nothing */
    tone(BOARD_PIN_D11, 440);    /* Timer3 — coexists fine */
}
```

With a servo attached, the only PWM left is D3 and D11, both on Timer0 —
which is exactly the timer you must not reconfigure. Plan the pin budget
before wiring: **servo + PWM + I2C does not fit on ten pads** without giving
something up.

## 12. USB HID keyboard — with the guards

Full file: `../template/variants/hid/main.cpp`. The two guards are not
optional while developing; without them a misbehaving sketch types into your
editor every time you plug the board in to fix it.

```c
#include <Keyboard.h>       /* lib_deps = arduino-libraries/Keyboard —
                               NOT bundled with PlatformIO's AVR core */

#define ARM_PIN        BOARD_PIN_D9    /* hold LOW to arm */
#define BOOT_GRACE_MS  5000u           /* dead time after reset */

void setup()
{
    pinMode(ARM_PIN, INPUT_PULLUP);    /* open = HIGH = disarmed */

    /* Guard 2: send nothing for five seconds after reset, so there is
       always a window in which to re-flash a runaway sketch. */
    delay(BOOT_GRACE_MS);

    Keyboard.begin();       /* adds a HID interface next to the CDC one;
                               the board stays a composite device and
                               uploads keep working */
}

void loop()
{
    /* Guard 1: re-read every pass, so pulling the jumper stops output
       immediately rather than at the next reset. */
    if (digitalRead(ARM_PIN) != LOW) {
        Keyboard.releaseAll();     /* never leave a key stuck down */
        return;
    }
    /* ... send keystrokes ... */
}
```

Three more things that surprise people:

- **HID carries key positions, not characters.** `Keyboard.print("y")` on a
  host set to a non-US layout produces whatever key sits at that position.
  Stick to letters and digits, or handle layouts yourself.
- `Keyboard.releaseAll()` on every exit path. A key left down by a reset
  mid-press repeats into the host until the board is unplugged.
- `Mouse.h` is a **separate** dependency (`arduino-libraries/Mouse`).
  Joystick/gamepad descriptors are third-party — nothing is bundled.

Recovery when it does go wrong: **short ICSP pin 5 (RESET) to pin 6 (GND)
once** on the six dots on the back, then upload within eight seconds
(Caterina's `TIMEOUT_PERIOD`), while the host is still being spammed. One
touch is enough — Caterina has no double-tap logic. If the timing is hard to
catch, start the upload *first* and touch the pads immediately after.

## 13. EEPROM with update semantics

1 KB, 100 k write cycles per cell. `put()`/`get()` handle structs and only
write bytes that actually changed.

```c
#include <EEPROM.h>

struct boot_log_t {
    uint32_t boots;
    uint32_t last_uptime_s;
};

boot_log_t log;
EEPROM.get(0, log);                     /* reads never wear the cell */
if (log.boots == 0xFFFFFFFFul) {        /* blank chip reads all 0xFF */
    log.boots = 0;
    log.last_uptime_s = 0;
}
log.boots++;
EEPROM.put(0, log);                     /* update semantics */
```

`EESAVE` is unprogrammed in the Arduino fuse set, so a chip erase — which is
what `Burn Bootloader` does — **wipes the EEPROM**. Do not keep the only copy
of calibration data there if you expect to re-burn the bootloader.

## 14. Sleep, and why USB makes it hard ⚠︎ compile-checked only

`SLEEP_MODE_PWR_DOWN` stops the USB clock, so the host sees the device
disappear and the CDC port drops. That is fine for a battery-powered prop and
fatal for anything that is supposed to stay enumerated.

```c
#include <avr/sleep.h>
#include <avr/power.h>

static void sleep_until_pin_change(void)
{
    /* Detach USB cleanly first, or the host logs a surprise disconnect. */
    USBCON |= (1 << FRZCLK);
    USBCON &= ~(1 << USBE);
    PLLCSR &= ~(1 << PLLE);         /* the PLL is pure current draw now */

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli();
    attachInterrupt(digitalPinToInterrupt(BOARD_PIN_D2), []{}, LOW);
    sleep_enable();
    sei();
    sleep_cpu();
    /* --- wakes here --- */
    sleep_disable();
    detachInterrupt(digitalPinToInterrupt(BOARD_PIN_D2));

    USBDevice.attach();             /* re-enumerate; the host assigns a
                                       port name again, possibly a new one */
}
```

Waking from power-down is restricted: a **`LOW`-level** external interrupt, a
pin-change interrupt, a TWI address match or the watchdog. Edge-triggered
`RISING`/`FALLING`/`CHANGE` on an `INTn` pin needs a running clock and will
never fire — which is why the example above asks for `LOW`. Note that the board
has no way to cut the power LED, so "deep sleep" on a Beetle still burns
whatever that LED draws; a board destined for battery use usually has it
desoldered.
