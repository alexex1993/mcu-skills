/*
 * Minimal bring-up for the LGT8F328P-LQFP32 MiniEVB ("LGTBF32BP"): Blink.
 *
 * Flash this first on an unfamiliar board. It proves the whole chain —
 * pio-lgt8fx platform -> avr-gcc -> avrdude over the USB-serial bridge ->
 * the bootloader -> the chip runs your code. Nothing else is connected, so
 * a failure here is the toolchain, the port, or the bootloader, never your
 * application.
 *
 * If it blinks at half or double the expected rate, the clock is
 * misconfigured: see the f_osc note in platformio.ini.
 */

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
