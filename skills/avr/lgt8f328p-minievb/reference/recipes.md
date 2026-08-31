# Copy-paste recipes — LGT8F328P-LQFP32 MiniEVB ("LGTBF32BP")

Recipes 1–6 and 9 are extracted from `template/`, which builds clean against the platform and
package versions in `board-hardware.md` §8. Everything else is marked **⚠︎ compile-checked
only**: it compiles against this core and is derived from the databook and the core source,
but it was **not run on hardware** by the author of this skill. Nothing here was verified with
an oscilloscope, a meter or a load.

Contents: [1 platformio.ini](#1-platformioini) · [2 board.h](#2-boardh) ·
[3 blink](#3-blink) · [4 the 12-bit ADC](#4-the-12-bit-adc) ·
[5 measuring the real VCC](#5-measuring-the-real-vcc) · [6 the DAC on D4](#6-the-dac-on-d4) ·
[7 Timer3 PWM on D1/D2](#7-timer3-pwm-on-d1d2) · [8 80 mA drive](#8-80-ma-drive) ·
[9 emulated EEPROM](#9-emulated-eeprom) · [10 moving the UART to D5/D6](#10-moving-the-uart-to-d5d6) ·
[11 sleep](#11-sleep) · [12 SWC/SWD as GPIO](#12-swcswd-as-gpio)

---

## 1. platformio.ini

```ini
[env:lgt8f328p]
platform = https://github.com/darkautism/pio-lgt8fx.git
board = LGT8F328P                 ; build.variant = lgt8fx8p == the LQFP32 part
framework = arduino
monitor_speed = 115200

board_build.f_cpu = 32000000L
board_build.f_osc = 32000000L     ; the internal RC. NEVER track f_cpu with this.
board_build.clock_source = 1      ; 1 = internal RC, 2 = external crystal

build_flags =
    -Wall

upload_protocol = arduino
upload_speed = 57600
;upload_flags = -V                ; skip avrdude read-back verification
```

To run at 16 MHz, change **only** `f_cpu`:

```ini
board_build.f_cpu = 16000000L
board_build.f_osc = 32000000L     ; unchanged
```

The core computes `F_DIV = F_OSC / F_CPU` and programs `CLKPR` from it. Setting both to
16 MHz gives `F_DIV = 1`, leaves the RC undivided, and every timing in the firmware is 2×
wrong with no error. See `board-hardware.md` §9.

## 2. board.h

The full annotated version is `template/include/board.h`. The essentials:

```c
#define BOARD_PIN_LED      13    /* PB5, on = HIGH; also SPI SCK             */
#define BOARD_PIN_RX       0     /* PD0 — wired to the USB-serial bridge     */
#define BOARD_PIN_TX       1     /* PD1 — bridge; ALSO Timer3 OC3A           */
#define BOARD_PIN_PWM_D1   1     /* Timer3 OC3A — steals UART TX             */
#define BOARD_PIN_PWM_D2   2     /* Timer3 OC3B — steals INT0                */
#define BOARD_PIN_DAC      4     /* PD4 = DAC0, analogWrite() is a real DAC  */
#define BOARD_PIN_SDA      A4    /* PC4 = 18 */
#define BOARD_PIN_SCL      A5    /* PC5 = 19 */
#define BOARD_ADC_VCC_DIV5 V5D1  /* = 22, internal VCC x 1/5 channel         */
#define BOARD_PIN_SWC      E0    /* = 22, PE0 — SWD clock                    */
#define BOARD_PIN_SWD      E2    /* = 23, PE2 — SWD data                     */
#define BOARD_PIN_PE4      E4    /* = 24, pad is PE4 || PF4                  */
#define BOARD_PIN_PE5      E5    /* = 26, pad is PE5 || PF5                  */
#define BOARD_PIN_AREF     E6    /* = 25, PE6 — AVREF unless PMX2.E6EN       */
#define BOARD_PIN_RESET    C6    /* = 27, PC6 — reset unless PMX2.C6EN       */
```

`E5` is pin 26 and `E6` is pin 25. The core's `D25`/`D26` spellings are swapped relative to
their own numbers — use the `E*` names.

## 3. Blink

`template/variants/minimal/main.cpp`. The first thing to flash on an unfamiliar board.

```cpp
#include <Arduino.h>

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);   /* D13 / PB5, on = HIGH */
}

void loop()
{
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
}
```

If it blinks at half or double the expected rate, the clock is misconfigured — recipe 1, not
the code.

## 4. The 12-bit ADC

The hardware is 12-bit. The core returns 10 bits until told otherwise, because
`analog_resbit` starts at 2:

```cpp
void setup()
{
    analogReadResolution(12);       /* now analogRead() returns 0..4095 */
    analogReference(INTERNAL2V048); /* 1.024 / 2.048 / 4.096 V, +-1%    */
}

int millivolts_on_a0()
{
    (void)analogRead(A0);           /* discard the first sample after a mux
                                       or reference change */
    const uint32_t raw = analogRead(A0);
    return (int)((raw * 2048UL) / 4096UL);
}
```

Two things that differ from an ATmega328P and bite ported code:

- `INTERNAL` here is `INTERNAL1V024` (1.024 V), not 1.1 V.
- `INTERNAL2V56` is `#define`d to the same value as `INTERNAL2V048`, so asking for 2.56 V
  silently gives you 2.048 V.

`analogReadResolution()` accepts up to 16; above 12 the core left-shifts, which adds range,
not information.

Each `analogRead()` runs **two** conversions (the core takes a sign-inverted sample and
averages) plus a gain-error correction, so budget roughly twice the time an ATmega328P
needs.

## 5. Measuring the real VCC

A protection diode sits in the board's 5 V line, so VCC is around 4.6 V on USB power and
every `analogRead()` against `DEFAULT` is scaled by that. The chip can measure its own
supply through an internal VCC/5 channel:

```cpp
/* Real supply in millivolts, against the internal 1.024 V reference. */
static uint16_t vcc_mv(void)
{
    analogReference(INTERNAL1V024);
    (void)analogRead(V5D1);                     /* discard: the mux just moved */
    const uint32_t raw = analogRead(V5D1);      /* V5D1 = VCC x 1/5 */
    return (uint16_t)((raw * 1024UL * 5UL) / 4096UL);
}
```

`V5D4` (= VCC × 4/5) and `IVREF` (the reference itself) work the same way. Requires
`analogReadResolution(12)` for the 4096 divisor above.

With that number you can either correct `DEFAULT`-referenced readings or, better, stop using
`DEFAULT` at all.

## 6. The DAC on D4

D4 is not a PWM pin. `analogWrite()` on it writes the 8-bit DAC register `DAL0` and produces
a real analog voltage between 0 and the selected reference:

```cpp
#include "board.h"

void ramp()
{
    for (uint16_t v = 0; v <= 255; v++) {
        analogWrite(BOARD_PIN_DAC, v);   /* BOARD_PIN_DAC == 4 */
        delay(4);
    }
}
```

Notes that matter:

- `analogWrite()` does **not** call `pinMode()` for the DAC pin, and does not short-circuit
  `0` and `255` to `digitalWrite()` the way it does for PWM pins.
- The DAC shares its reference with the ADC through `DACON`, so `analogReference()` moves the
  DAC's full-scale too.
- A Nano sketch that "fades an LED" on D4 will produce a smooth DC ramp here rather than a
  PWM waveform. If something downstream needs edges — a MOSFET gate, an opto — move it.

## 7. Timer3 PWM on D1/D2

⚠︎ compile-checked only.

The LQFP32 bonds PF1 onto the D1 pad and PF2 onto the D2 pad, so Timer3's OC3A/OC3B come out
on the two pins a Nano user thinks of as TX and INT0:

```cpp
analogWrite(1, 128);   /* Timer3 OC3A on the D1 (TX) pad */
analogWrite(2, 64);    /* Timer3 OC3B on the D2 pad      */
```

The core does the bonded-pad dance for you — it clears `DDRD1` before setting `DDRF1`, so the
PD1 driver lets go before the PF1 driver takes over. If you program `TCCR3A` yourself, do the
same or the two drivers fight inside the package.

Consequence: **`Serial` output dies the moment you `analogWrite(1, …)`.** Either pick another
pin or move the UART (recipe 10). Losing D2 costs you INT0.

## 8. 80 mA drive

⚠︎ compile-checked only.

Six pads can be switched from the default 12 mA push-pull to 80 mA through `HDR` (0xE0). On
the header those are D5, D6, D1/TX, D2, PE4 and PE5:

```cpp
#include "board.h"

void setup()
{
    pinMode(5, OUTPUT);
    HDR |= (1 << 0);        /* HDR[0] -> PD5 (D5) at 80 mA */
    digitalWrite(5, HIGH);
}
```

| bit | pad | header |
|---|---|---|
| `HDR[0]` | PD5 | D5 |
| `HDR[1]` | PD6 | D6 |
| `HDR[2]` | PD1 ‖ PF1 | D1 / TX |
| `HDR[3]` | PD2 ‖ PF2 | D2 |
| `HDR[4]` | PE4 ‖ PF4 | PE4 |
| `HDR[5]` | PE5 ‖ PF5 | PE5 |

The LQFP32 package has exactly one VCC pin and one GND pin. The databook says not to drive
four high-current loads simultaneously on this package — the bond wires, not the pads, are
the limit. Two is a sane working maximum.

## 9. Emulated EEPROM

There is no EEPROM array. `EEPROM.h` drives a flash page-swap controller.

```cpp
#include <EEPROM.h>

struct settings_t { uint32_t boots; uint16_t threshold; };

void setup()
{
    lgt_eeprom_init(1);          /* 1 KB emulated, costs 2 KB of program flash.
                                    n = 0, 1, 2, 4 or 8. The EEPROM object's
                                    constructor already does lgt_eeprom_init(). */

    settings_t s;
    EEPROM.get(0, s);
    if (s.boots == 0xFFFFFFFFul) {    /* blank, or freshly re-uploaded */
        s.boots = 0;
        s.threshold = 512;
    }
    s.boots++;
    EEPROM.put(0, s);
}
```

Read `board-hardware.md` §5.1 before relying on this. The three facts that change designs:

1. **Every sketch upload erases it**, including an in-place update. A real AVR keeps its
   EEPROM across programming; this one does not. Calibration data, device IDs and pairing
   keys need somewhere else to live.
2. **1 KB of EEPROM costs 2 KB of flash**, and PlatformIO's 29,696 B budget does not know
   about it. With the default 1 KB, keep the sketch under roughly 27.6 KB.
3. Of each 1 KB page, **1020 bytes are usable** — two bytes hold the page-valid flag and
   cells are 32 bits wide.

`lgt_eeprom_init(0)` disables the emulation entirely and gives the flash back.

## 10. Moving the UART to D5/D6

⚠︎ compile-checked only.

`PMX0` bit 1 (`TXD6`) moves TX from PD1 to PD6, and bit 0 (`RXD5`) moves RX from PD0 to PD5.
That frees D0/D1 — the pins the USB bridge and the bootloader use — for the application,
which is otherwise impossible on a Nano-shaped board.

```cpp
static void uart_to_d5_d6(void)
{
    /* PMX0 is write-protected: set WCE, then finish within 6 system clocks. */
    PMX0 = 0x80;
    PMX0 = 0x03;            /* TXD6 | RXD5 */
}

void setup()
{
    Serial.begin(115200);
    uart_to_d5_d6();        /* console now on D6 (TX) / D5 (RX) */
}
```

Do this **after** `Serial.begin()`, and remember that the USB bridge is still wired to D0/D1
— you now need an external USB-serial adapter on D5/D6 to see the console, and uploads still
go through D0/D1 because the bootloader runs before your code.

The same register moves OC1A/OC1B, OC0A/OC0B and SPI SS; the full bit table is in
`board-hardware.md` §2.5.

## 11. Sleep

⚠︎ compile-checked only.

The bundled `PMU` library wraps the power-management unit:

```cpp
#include <PMU.h>

void loop()
{
    /* Deepest mode that a periodic wake can leave: everything off, WDT
       running from the 32 kHz RC. */
    PMU.sleep(PM_POWERDOWN, SLEEP_8S);

    /* ... woken, do work ... */
}
```

Modes: `PM_IDLE` (core clock only), `PM_POWERDOWN`, `PM_POFFS0` (external interrupt or
periodic wake) and `PM_POFFS1` (external interrupt only, lowest power — the datasheet's
1 µA @ 3.3 V figure). Periods run `SLEEP_64MS` … `SLEEP_32S` and `SLEEP_FOREVER`.

The databook is explicit that sleep does **not** power the analog blocks down: disable ADC,
DAC, the comparators and LVD in software first, or they keep drawing current and the datasheet
numbers are meaningless.

## 12. SWC/SWD as GPIO

⚠︎ compile-checked only — and read `board-hardware.md` §7 first.

PE0 (SWC) and PE2 (SWD) are the debug and recovery port. They become ordinary GPIO only by
disabling SWD, which also disables the only way to reprogram a board whose bootloader is
gone:

```cpp
/* Point of no easy return. The bit must be written twice within 4 cycles. */
static void disable_swd(void)
{
    MCUSR |= 0x80;
    MCUSR |= 0x80;
}
```

Then `E0` (22) and `E2` (23) work like any other pin.

If you did this and need SWD back: **hold RESET low while powering the board up**, so the
sketch never runs and never sets the bit. Then reprogram over SWD with a second Arduino
running the core's `LarduinoISP` example (D13→SWC, D12→SWD, D10→RST, plus VCC/GND, and a
10 µF cap between the programmer's RESET and VCC).

The related traps in ordinary code:

```cpp
MCUSR = 0;        /* safe — bit 7 written as zero, and what the core already does */
MCUSR = 0xFF;     /* disables SWD. Never clear reset flags this way. */
MCUSR |= 0x80;    /* the same thing, spelled deliberately */
```
