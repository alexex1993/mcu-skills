/*
 * Raspberry Pi Pico (RP2040) — full bring-up.
 *
 * Non-blocking LED heartbeat, USB-CDC status report, ADC samples (A0, die
 * temperature, VSYS through the internal A3 divider), VBUS sense and a
 * flash-backed EEPROM boot counter. Demonstrates the things every Pico
 * project needs to get right:
 *
 *   - `Serial` is USB CDC, not UART0. Bytes printed before the host opens
 *     the port are dropped; never block startup on the port being open.
 *     The physical pins GPIO0/1 are Serial1.
 *   - analogRead() is 10-bit by default; analogReadResolution(12) selects
 *     the native 12 bits. The ADC is ratiometric to a filtered 3.3 V rail
 *     with ~30 mV of built-in offset — see the skill's analog section
 *     before trusting absolute numbers.
 *   - A3/GPIO29 reads VSYS/3 through an on-board divider — multiply by 3.
 *   - EEPROM is emulated in the last 4 KB flash sector: begin() maps it,
 *     put() stages changes (update semantics), commit() burns the sector.
 */

#include <Arduino.h>
#include <EEPROM.h>

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

void setup()
{
    pinMode(BOARD_PIN_LED, OUTPUT);
    pinMode(BOARD_PIN_VBUS_SENSE, INPUT);

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

    /* Prints made before the host opens the port are dropped, so repeat
     * the banner from loop() instead of gating startup on Serial. */
}

void loop()
{
    const uint32_t now = millis();

    if (now - last_beat_ms >= HEARTBEAT_MS) {   /* never delay() in loop() */
        last_beat_ms = now;
        led_state = !led_state;
        digitalWrite(BOARD_PIN_LED, led_state);
    }

    if (now - last_report_ms >= REPORT_MS) {
        last_report_ms = now;

        boot_log_t log;
        EEPROM.get(0, log);
        log.last_uptime_s = now / 1000;
        EEPROM.put(0, log);
        EEPROM.commit();    /* one 4 KB sector erase per changed commit */

        /* VSYS through the internal divider: A3 = VSYS/3. */
        const float vsys_v = analogRead(BOARD_PIN_VSYS_ADC) * 3.3f / 4095.0f * 3.0f;

        Serial.print(F("# pico up "));
        Serial.print(now / 1000);
        Serial.print(F(" s, boot "));
        Serial.print(log.boots);
        Serial.print(F(", A0 "));
        Serial.print(analogRead(BOARD_PIN_ADC_A0));
        Serial.print(F(" (12-bit), die "));
        Serial.print(analogReadTemp(), 1);
        Serial.print(F(" C, VSYS "));
        Serial.print(vsys_v, 2);
        Serial.print(F(" V, VBUS "));
        Serial.println(digitalRead(BOARD_PIN_VBUS_SENSE) ? F("present") : F("absent"));
    }
}
