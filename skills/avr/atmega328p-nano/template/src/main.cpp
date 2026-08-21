/*
 * Arduino Nano (ATmega328P) — full bring-up.
 *
 * Non-blocking LED heartbeat, USB-serial status report, ADC sample, EEPROM
 * boot counter and a free-RAM watch. Demonstrates the four things every
 * Nano project needs to get right:
 *
 *   - timing without delay()          (Timer0 and millis() stay untouched)
 *   - the F() macro                   (string literals stay in flash — the
 *                                      2 KB SRAM wall is the real limit)
 *   - EEPROM with update semantics    (put() only writes bytes that changed,
 *                                      100 k cycles/cell endurance)
 *   - free-RAM monitoring             (heap/stack collision is invisible
 *                                      until the sketch corrupts itself)
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <avr/wdt.h>

#include "board.h"

#define HEARTBEAT_MS   500u   /* toggling every 500 ms = 1 Hz blink */
#define REPORT_MS      5000u  /* serial report cadence */

struct boot_log_t {
    uint32_t boots;          /* increments once per power-up/reset cycle */
    uint32_t last_uptime_s;  /* how long the previous run lasted        */
};

static uint32_t last_beat_ms  = 0;
static uint32_t last_report_ms = 0;
static bool     led_state      = false;

/* Free bytes between the heap end and the current stack pointer. On a 2 KB
 * part this is the number to watch: below ~200 B, restructure before the
 * heap meets the stack — the crash, when it comes, looks like random
 * variable corruption, not an out-of-memory error. */
static int free_ram(void)
{
    extern int __heap_start, *__brkval;
    int v;
    return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void setup()
{
    /* Kill the watchdog first: the old ATmegaBOOT bootloader does not
     * disable it on entry, so a sketch that leaves it enabled reset-loops
     * those boards before setup() ever runs again. */
    MCUSR = 0;
    wdt_disable();

    pinMode(BOARD_PIN_LED, OUTPUT);

    Serial.begin(115200);

    boot_log_t log;
    EEPROM.get(0, log);   /* reads never wear the cell */
    if (log.boots == 0xFFFFFFFFul) {   /* first run on a blank chip */
        log.boots = 0;
        log.last_uptime_s = 0;
    }
    log.boots++;
    EEPROM.put(0, log);   /* put() = update semantics: only changed bytes burn */

    Serial.print(F("# Arduino Nano up, boot "));
    Serial.print(log.boots);
    Serial.print(F(", previous run "));
    Serial.print(log.last_uptime_s);
    Serial.println(F(" s"));
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

        Serial.print(F("up "));
        Serial.print(now / 1000);
        Serial.print(F(" s, free RAM "));
        Serial.print(free_ram());
        Serial.print(F(" B, A0 "));
        Serial.println(analogRead(BOARD_PIN_ADC_A0));
    }
}
