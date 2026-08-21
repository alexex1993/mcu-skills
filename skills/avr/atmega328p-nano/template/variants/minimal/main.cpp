/*
 * Minimal bring-up: the classic Blink. This exact sketch was verified on an
 * Arduino Nano A000005 (v3.x, Optiboot) — LED_BUILTIN is D13 ("L"),
 * HIGH = on. Proves toolchain -> build -> flash -> run with nothing else
 * connected; if this does not blink, the problem is the toolchain or the
 * bootloader era, not your code.
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
