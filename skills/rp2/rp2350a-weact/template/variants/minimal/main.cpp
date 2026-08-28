/*
 * Minimal bring-up for the WeAct RP2350A Core Board: the classic Blink.
 *
 * GP25 is a user LED on both revisions (the only one on V2.0, "LED2" on
 * V1.0), anode to the pin through 5.1 kOhm, so HIGH = on and it is dim —
 * look at it in shade, not daylight. LED_BUILTIN from the rpipico2 variant
 * happens to be 25 as well; board.h says it out loud instead.
 *
 * Nothing else is touched. If this does not blink, the problem is the
 * toolchain, the cable or the BOOT dance, not the code. Watch the LED right
 * after upload: the board reboots itself when picotool finishes.
 */

#include <Arduino.h>

#include "board.h"

void setup()
{
    pinMode(BOARD_PIN_LED, OUTPUT);
}

void loop()
{
    digitalWrite(BOARD_PIN_LED, HIGH);
    delay(500);
    digitalWrite(BOARD_PIN_LED, LOW);
    delay(500);
}
