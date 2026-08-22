/*
 * Minimal bring-up: the classic Blink, over USB. LED_BUILTIN is GPIO25 (the
 * on-board green LED), HIGH = on. Nothing else is touched — if this does not
 * blink, the problem is the toolchain, the cable or the BOOTSEL dance, not
 * the code. Watch the LED right after upload: the board reboots itself when
 * picotool finishes.
 */

#include <Arduino.h>

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
}
