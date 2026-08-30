/*
 * Beetle (ATmega32U4) — minimal bring-up: blink the onboard D13 LED.
 *
 * Flash this first on an unfamiliar Beetle. It proves toolchain -> build ->
 * 1200 bps touch -> Caterina bootloader -> run, with nothing wired to the
 * board. If it does not blink, the problem is one of those five things, not
 * your code — go to the skill's flashing section, not to a logic analyser.
 *
 * Deliberately no Serial: this sketch must stay uploadable no matter what,
 * so it never touches USB beyond what the core does on its own. Note that
 * the core still enumerates the CDC port for you — the ~4 KB of USB stack in
 * the size report is not optional on this chip.
 *
 * D13 = PC7, active HIGH, not brought out to a pad. A clone with no LED
 * fitted there stays dark while running perfectly; see include/board.h.
 */

#include <Arduino.h>

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
}
