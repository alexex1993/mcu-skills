/*
 * Beetle (ATmega32U4) — full bring-up.
 *
 * Non-blocking LED heartbeat, USB-CDC status report, ADC sample, EEPROM boot
 * counter and a free-RAM watch. Demonstrates the four things a 32U4 project
 * has to get right and that an ATmega328P project does not:
 *
 *   - never block on USB          (`while (!Serial)` hangs forever off a PC;
 *                                  `if (Serial)` is the non-blocking form)
 *   - keep the sketch responsive  (uploads go through the running sketch's
 *                                  CDC port at 1200 baud — a sketch stuck in
 *                                  a tight loop with interrupts off cannot be
 *                                  replaced without a manual ICSP reset)
 *   - the F() macro               (2,560 B of SRAM, of which the USB CDC
 *                                  stack has already spent ~150 B)
 *   - EEPROM with update semantics (put() writes only changed bytes;
 *                                  100 k cycles/cell)
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <avr/wdt.h>

#include "board.h"

#define HEARTBEAT_MS   500u   /* toggling every 500 ms = 1 Hz blink */
#define REPORT_MS     5000u   /* serial report cadence              */

struct boot_log_t {
    uint32_t boots;          /* increments once per power-up/reset cycle */
    uint32_t last_uptime_s;  /* how long the previous run lasted         */
};

static uint32_t last_beat_ms   = 0;
static uint32_t last_report_ms = 0;
static bool     led_state      = false;

/* Free bytes between the heap end and the current stack pointer. Below
 * ~250 B, stop adding features: the heap/stack collision on this part shows
 * up as random variable corruption, never as an out-of-memory error. */
static int free_ram(void)
{
    extern int __heap_start, *__brkval;
    int v;
    return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void setup()
{
    /* Kill the watchdog first. Caterina (the Leonardo/Beetle bootloader)
     * uses the WDT itself to hand control to the sketch, so a sketch that
     * leaves the WDT armed with a short timeout reset-loops the board
     * faster than the 8 s bootloader window can be caught. */
    MCUSR = 0;
    wdt_disable();

    pinMode(BOARD_PIN_LED, OUTPUT);

    /* The baud rate is a formality: Serial here is USB CDC, not a UART. */
    Serial.begin(115200);

    boot_log_t log;
    EEPROM.get(0, log);                 /* reads never wear the cell */
    if (log.boots == 0xFFFFFFFFul) {    /* first run on a blank chip */
        log.boots = 0;
        log.last_uptime_s = 0;
    }
    log.boots++;
    EEPROM.put(0, log);   /* put() = update semantics: only changed bytes burn */

    /* No `while (!Serial)` — that waits for a host to open the port and
     * hangs a battery-powered Beetle in setup() forever. Give the host a
     * moment to enumerate, then carry on regardless. */
    const uint32_t deadline = millis() + 2000u;
    while (!Serial && millis() < deadline) { /* spin, but with an exit */ }

    Serial.print(F("# Beetle up, boot "));
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

        /* `if (Serial)` is false while no host has the port open. Skipping
         * the print keeps the sketch running at full speed off USB power. */
        if (Serial) {
            Serial.print(F("up "));
            Serial.print(now / 1000);
            Serial.print(F(" s, free RAM "));
            Serial.print(free_ram());
            Serial.print(F(" B, A0 "));
            Serial.println(analogRead(BOARD_PIN_A0));
        }
    }
}
