/*
 * WeAct Studio RP2350A Core Board — full bring-up, both revisions.
 *
 * Non-blocking LED heartbeat, USB-CDC status report, the real QSPI flash size
 * read out of the chip's JEDEC ID, ADC samples (A0, die temperature) and the
 * per-revision extras: VSYS + VBUS sense on V2.0, the second LED and the KEY
 * button on V1.0. Plus a flash-backed EEPROM boot counter.
 *
 * The things this board makes people get wrong:
 *
 *   - `Serial` is USB CDC, not UART0. Bytes printed before the host opens the
 *     port are dropped; never block startup on the port being open. The
 *     physical pins GP0/GP1 are Serial1.
 *   - analogRead() is 10-bit by default; analogReadResolution(12) selects the
 *     native 12 bits.
 *   - The board ships with 4 MB or 16 MB of flash and nothing on it says
 *     which. The build only knows what platformio.ini claims, so this sketch
 *     asks the flash chip itself and prints both numbers.
 *   - GP23/GP24/GP29 mean different things on V1.0 and V2.0 — include
 *     board.h, never hardcode them.
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <hardware/flash.h>

#include "board.h"

#define HEARTBEAT_MS  500u    /* toggling every 500 ms = 1 Hz blink */
#define REPORT_MS     5000u   /* serial report cadence              */

struct boot_log_t {
    uint32_t boots;           /* counts power-ups and resets        */
    uint32_t last_uptime_s;   /* how long the previous run lasted   */
};

static uint32_t last_beat_ms   = 0;
static uint32_t last_report_ms = 0;
static bool     led_state      = false;
static uint32_t flash_bytes    = 0;   /* filled in once, in setup() */

/*
 * Read the flash chip's JEDEC ID (0x9F) and turn the capacity byte into
 * bytes: id[2] is log2(size). flash_do_cmd() runs from RAM but stops XIP
 * while it talks to the chip, so the SDK's own rule applies — call it during
 * startup, with interrupts off, before core1 or any flash-resident IRQ can
 * run. Doing it later, from a running application, is what hangs.
 */
static uint32_t read_flash_size_bytes()
{
    uint8_t tx[4] = { 0x9F, 0, 0, 0 };
    uint8_t rx[4] = { 0 };

    noInterrupts();
    flash_do_cmd(tx, rx, sizeof(tx));
    interrupts();

    /* rx[0] is the echo of the command byte; rx[1..3] = manufacturer, type,
     * capacity. Sane capacities are 2^16 (64 KB) .. 2^25 (32 MB). */
    if (rx[3] < 16 || rx[3] > 25) {
        return 0;
    }
    return 1ul << rx[3];
}

void setup()
{
    flash_bytes = read_flash_size_bytes();   /* first, before anything else */

    pinMode(BOARD_PIN_LED, OUTPUT);
#if BOARD_REV == 10
    pinMode(BOARD_PIN_LED2, OUTPUT);
    pinMode(BOARD_PIN_KEY, INPUT);           /* 5.1 kOhm pull-up on board;
                                                INPUT_PULLDOWN here would be
                                                fighting it and would also hit
                                                erratum RP2350-E9 */
#else
    pinMode(BOARD_PIN_VBUS_SENSE, INPUT);
    pinMode(BOARD_PIN_SMPS_MODE, OUTPUT);
    digitalWrite(BOARD_PIN_SMPS_MODE, LOW);  /* PFM: the power-on default */
#endif

    /* Native 12 bits; without this analogRead() returns 0-1023. */
    analogReadResolution(12);

    Serial.begin(115200);   /* baud ignored on USB CDC; harmless */

    boot_log_t log;
    EEPROM.begin(sizeof(log));
    EEPROM.get(0, log);     /* reads never wear the flash sector */
    if (log.boots == 0xFFFFFFFFul) {   /* first run on a blank chip */
        log.boots = 0;
        log.last_uptime_s = 0;
    }
    log.boots++;
    EEPROM.put(0, log);     /* put() = update semantics */
    EEPROM.commit();        /* without this nothing persists — silently */

    /* Prints made before the host opens the port are dropped, so the banner
     * is repeated from loop() instead of gating startup on Serial. */
}

void loop()
{
    const uint32_t now = millis();

    if (now - last_beat_ms >= HEARTBEAT_MS) {   /* never delay() in loop() */
        last_beat_ms = now;
        led_state = !led_state;
        digitalWrite(BOARD_PIN_LED, led_state);
#if BOARD_REV == 10
        digitalWrite(BOARD_PIN_LED2, !led_state);   /* the two alternate */
#endif
    }

    if (now - last_report_ms >= REPORT_MS) {
        last_report_ms = now;

        boot_log_t log;
        EEPROM.get(0, log);
        log.last_uptime_s = now / 1000;
        EEPROM.put(0, log);
        EEPROM.commit();    /* one 4 KB sector erase per changed commit */

        Serial.print(F("# RP2350A_V"));
        Serial.print(BOARD_REV / 10);
        Serial.print(F(".0 up "));
        Serial.print(now / 1000);
        Serial.print(F(" s, boot "));
        Serial.print(log.boots);

        Serial.print(F(", flash "));
        if (flash_bytes) {
            Serial.print(flash_bytes / (1024ul * 1024ul));
            Serial.print(F(" MB on chip / "));
        } else {
            Serial.print(F("? MB on chip / "));
        }
        Serial.print(PICO_FLASH_SIZE_BYTES / (1024ul * 1024ul));
        Serial.print(F(" MB in the build"));

        Serial.print(F(", A0 "));
        Serial.print(analogRead(BOARD_PIN_ADC_A0));
        Serial.print(F(" (12-bit), die "));
        Serial.print(analogReadTemp(), 1);
        Serial.print(F(" C"));

#if BOARD_REV == 20
        /* ~VSYS/3 through the on-board sense network — see board.h. */
        const float vsys_v = analogRead(BOARD_PIN_VSYS_ADC) * 3.3f / 4095.0f * 3.0f;
        Serial.print(F(", VSYS ~"));
        Serial.print(vsys_v, 2);
        Serial.print(F(" V, VBUS "));
        Serial.print(digitalRead(BOARD_PIN_VBUS_SENSE) ? F("present") : F("absent"));
#else
        Serial.print(F(", A3 "));
        Serial.print(analogRead(BOARD_PIN_ADC_A3));
        Serial.print(F(", KEY "));
        Serial.print(digitalRead(BOARD_PIN_KEY) ? F("up") : F("down"));
#endif
        Serial.println();
    }
}
