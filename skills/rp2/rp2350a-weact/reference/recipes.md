# WeAct RP2350A Core Board recipes

Copy-paste-ready. Recipes 1–7 are extracted from `template/` and are what the
build in `template/README.md` measured; the rest are marked where they are
compile-verified only. Everything assumes `-DBOARD_REV=10` or `=20` is set —
see recipe 2.

---

## 1. `platformio.ini` — four envs, one per board

```ini
[platformio]
default_envs = weact_rp2350a_v20

[env]
platform = raspberrypi@1.20.0   ; first registry release with RP2350 boards
board = rpipico2                ; V2.0 is pin-for-pin a Pico 2
framework = arduino
board_build.core = earlephilhower   ; plain `framework = arduino` = Mbed core,
                                    ; which has no RP2350 support at all
board_build.filesystem_size = 0MB
monitor_speed = 115200
build_flags =
    -Wall

[env:weact_rp2350a_v20]
build_flags = ${env.build_flags} -DBOARD_REV=20

[env:weact_rp2350a_v20_16mb]
build_flags = ${env.build_flags} -DBOARD_REV=20
board_upload.maximum_size = 16777216

[env:weact_rp2350a_v10]
build_flags = ${env.build_flags} -DBOARD_REV=10

[env:weact_rp2350a_v10_16mb]
build_flags = ${env.build_flags} -DBOARD_REV=10
board_upload.maximum_size = 16777216
```

`board_upload.maximum_size` is the *only* knob for flash size on this platform;
the builder derives max sketch size, the EEPROM address and the filesystem
window from it. `board_build.f_flash` does nothing here.

## 2. The revision guard

Put this at the top of `board.h` so a wrong-revision build cannot happen
quietly:

```c
#if !defined(BOARD_REV)
#error "BOARD_REV is not set. Add -DBOARD_REV=20 (V2.0) or -DBOARD_REV=10 (V1.0) to build_flags."
#elif (BOARD_REV != 10) && (BOARD_REV != 20)
#error "BOARD_REV must be 10 (RP2350A_V10) or 20 (RP2350A_V20)."
#endif

#if BOARD_REV == 20
#define BOARD_PIN_LED        25  /* green, active-HIGH, the only user LED */
#define BOARD_PIN_VSYS_ADC   29  /* ~VSYS/3, test pad only                */
#define BOARD_PIN_VBUS_SENSE 24  /* HIGH while USB VBUS present           */
#define BOARD_PIN_SMPS_MODE  23  /* LOW = PFM (default), HIGH = forced PWM */
#else
#define BOARD_PIN_LED        25  /* LED2, green, active-HIGH              */
#define BOARD_PIN_LED2       24  /* LED1, blue,  active-HIGH              */
#define BOARD_PIN_KEY        23  /* active-LOW, external 5.1 kOhm pull-up */
#define BOARD_PIN_ADC_A3     29  /* free ADC3 on header pin 35            */
#endif
```

## 3. Read the real flash size (4 MB or 16 MB)

Nothing on the board says which chip is fitted, and the linker believes
`platformio.ini`. Ask the flash itself, **first thing in `setup()`**:
`flash_do_cmd()` stops XIP while it runs, so it is only safe during startup
with interrupts off and before core 1 or any flash-resident ISR can run.

```c
#include <hardware/flash.h>

static uint32_t read_flash_size_bytes()
{
    uint8_t tx[4] = { 0x9F, 0, 0, 0 };   /* JEDEC READ ID */
    uint8_t rx[4] = { 0 };

    noInterrupts();
    flash_do_cmd(tx, rx, sizeof(tx));
    interrupts();

    if (rx[3] < 16 || rx[3] > 25) {      /* capacity byte = log2(size) */
        return 0;
    }
    return 1ul << rx[3];
}

void setup() {
    uint32_t chip  = read_flash_size_bytes();   /* what is really fitted */
    uint32_t built = PICO_FLASH_SIZE_BYTES;     /* what the build assumed */
    /* if these disagree, switch env before doing anything else */
}
```

`picotool info -d` with the board in BOOTSEL is a cross-check from the host
side, but it reads the flash size out of OTP, which this board leaves
unprogrammed — the JEDEC read above is the reliable one.

## 4. LEDs

```c
pinMode(BOARD_PIN_LED, OUTPUT);
digitalWrite(BOARD_PIN_LED, HIGH);      /* on — 5.1 kOhm series, so dim */

#if BOARD_REV == 10
pinMode(BOARD_PIN_LED2, OUTPUT);        /* V1.0's second (blue) LED */
digitalWrite(BOARD_PIN_LED2, LOW);
#endif
```

## 5. The V1.0 KEY button, and any button you add

```c
#if BOARD_REV == 10
pinMode(BOARD_PIN_KEY, INPUT);          /* board already has a 5.1k pull-up */
bool pressed = (digitalRead(BOARD_PIN_KEY) == LOW);
#endif
```

For your own buttons, wire them **to GND** and use `INPUT_PULLUP`. Erratum
RP2350-E9 makes `INPUT_PULLDOWN` unreliable: input-mode leakage of up to
~120 µA overwhelms the internal pull-down and latches the pad at ~2 V, which
reads HIGH forever. If a button must pull *up*, fit an external pull-down of
8.2 kΩ or less.

## 6. V2.0 power sensing

```c
#if BOARD_REV == 20
pinMode(BOARD_PIN_VBUS_SENSE, INPUT);
bool usb_present = digitalRead(BOARD_PIN_VBUS_SENSE);   /* VBUS/2 divider */

analogReadResolution(12);
float vsys = analogRead(BOARD_PIN_VSYS_ADC) * 3.3f / 4095.0f * 3.0f;
#endif
```

The ×3 is nominal: R23/R24 100 kΩ into R25 100 kΩ works out to VSYS/3, but
there is a FET (gate at +3V3) in the path whose resistance varies with VSYS.
Trust the trend, calibrate the absolute value against a meter. On V1.0 there is
no VSYS sense at all — GP29 is a plain ADC3 input.

## 7. ADC and die temperature

```c
analogReadResolution(12);               /* default is 10 bits (0-1023) */

int   raw  = analogRead(A0);            /* A0=GP26, A1=GP27, A2=GP28 */
float volt = raw * 3.3f / 4095.0f;      /* ADC_VREF is 3V3 - I*100R  */
float degC = analogReadTemp();          /* on-die sensor, channel 4  */
```

Return analog grounds to the `AGND` pin (header pin 33), not to a digital GND.
On V2.0 you can measure the actual reference on header pin 35 (`VREF`) and
scale against it; on V1.0 that pin is GP29 and the reference is only on test
point T1.

Quieter conversions on V2.0 — force the buck-boost out of PFM while sampling
(*compile-verified only*):

```c
#if BOARD_REV == 20
digitalWrite(BOARD_PIN_SMPS_MODE, HIGH);   /* forced PWM: less ripple */
delayMicroseconds(100);
int raw = analogRead(A0);
digitalWrite(BOARD_PIN_SMPS_MODE, LOW);    /* back to PFM efficiency */
#endif
```

## 8. `Serial` is USB CDC; `Serial1` is the UART

```c
Serial.begin(115200);       /* USB CDC. Baud is ignored.            */
Serial1.begin(115200);      /* the real UART on GP0/GP1             */
```

Never `while (!Serial) {}` — that blocks until a terminal attaches. Bytes
printed before the host opens the port are dropped, so repeat anything
important from `loop()`.

## 9. `Wire1` collides with the ADC

```c
Wire.begin();               /* i2c0 on GP4/GP5 — fine as-is         */

Wire1.setSDA(2);            /* i2c1 defaults to GP26/GP27 = A0/A1!  */
Wire1.setSCL(3);            /* call before begin()                  */
Wire1.begin();
```

Valid i2c1 pairs on this board: 2/3, 6/7, 10/11, 14/15, 18/19, 26/27.

## 10. PWM, with the slice map in mind

```c
analogWriteFreq(50);            /* servo rate, applies per slice */
analogWriteRange(255);
analogWrite(2, 128);
```

Slice = `(gpio >> 1) & 7`, channel = `gpio & 1`. So GP2 shares its slice — and
therefore its frequency — with GP3, GP18 and GP19, and shares its *channel*
with GP18: `analogWrite(2, x)` and `analogWrite(18, y)` write the same duty
register. The full table is in `board-hardware.md` §11. Only GP14/GP15
(slice 7) have no aliases.

## 11. EEPROM (last 4 KB of flash)

```c
#include <EEPROM.h>

struct cfg_t { uint32_t boots; uint32_t magic; };

EEPROM.begin(sizeof(cfg_t));    /* maps a RAM shadow                    */
cfg_t c;
EEPROM.get(0, c);               /* reads never wear the sector          */
c.boots++;
EEPROM.put(0, c);               /* stages, update semantics             */
EEPROM.commit();                /* one 4 KB erase — without it nothing
                                   persists, silently and with no error */
```

The sector lives at `0x103FF000` (4 MB) or `0x10FFF000` (16 MB). `commit()`
stalls both cores while it runs.

## 12. LittleFS

```ini
board_build.filesystem_size = 1MB   ; default is 0MB — begin() fails otherwise
```

```c
#include <LittleFS.h>
LittleFS.begin();               /* false if no space was reserved */
```

Then `pio run -t uploadfs` once to write the initial image. The filesystem is
carved out of flash below the EEPROM sector, so raising it lowers the maximum
sketch size.

## 13. Reboot into the bootloader from code

```c
rp2040.rebootToBootloader();    /* same object name on RP2350 */
```

Useful when you want an upload path that does not depend on the 1200-baud
touch — bind it to the V1.0 KEY button, or to a serial command.

## 14. Second core

```c
void setup1()  { pinMode(BOARD_PIN_LED, OUTPUT); }
void loop1()   { digitalWrite(BOARD_PIN_LED, !digitalRead(BOARD_PIN_LED));
                 delay(500); }
```

Both cores execute from the same XIP flash, so anything that writes flash
(`EEPROM.commit()`, LittleFS) stalls both. Use `rp2040.fifo` to pass values;
guard shared state with `mutex_t` or `rp2040.idleOtherCore()`.

## 15. Reading the BOOT button at runtime

On V2.0 there is no user button on a GPIO at all — but BOOT can be polled,
because it grounds the flash chip select and arduino-pico knows how to float
that pad and sample it:

```c
if (BOOTSEL) {                  /* true while BOOT is held */
    rp2040.rebootToBootloader();
}
```

Each read stops XIP, disables interrupts and idles the other core for about a
millisecond, so poll it at human speed (tens of hertz), never from an ISR or a
tight loop, and never while core 1 is doing anything time-critical. Holding
BOOT during a *reset* still enters the bootloader as usual — this only reads it
while the sketch runs. *Compile-verified only.*

## 16. Board identity at runtime

```c
Serial.println(rp2040.getChipID());     /* 64-bit unique ID from the flash */
Serial.println(PICO_FLASH_SIZE_BYTES);  /* what the build assumed          */
```

There is no way to read the *board revision* from software — the difference is
in what is wired to GP23/GP24/GP29, and probing those to guess would drive the
V1.0 LEDs or the V2.0 SMPS mode pin. Set `-DBOARD_REV` and be explicit.
