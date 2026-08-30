/*
 * Beetle (ATmega32U4) — USB HID keyboard, the board's signature use case,
 * written so it cannot lock you out of your own machine.
 *
 * THE RULE THIS FILE EXISTS TO ENFORCE: a HID sketch must never type on its
 * own at boot. The Beetle enumerates ~2 s after plug-in and a sketch that
 * starts sending keystrokes immediately types into whatever window has focus
 * — including the editor you are about to fix it in — and keeps doing it
 * every time you plug the board in to reprogram it. Recovering means
 * shorting the ICSP reset pads on the back while the host is still fighting
 * the keystrokes.
 *
 * Two guards, both cheap, both here:
 *   1. ARM_PIN must be held LOW for HID output to happen at all. Nothing
 *      wired = nothing typed. A button, a jumper, or a wire to "-".
 *   2. BOOT_GRACE_MS of doing nothing after reset, so there is always a
 *      window to re-flash before anything is sent.
 *
 * Keyboard/Mouse are NOT in the Arduino AVR core that PlatformIO installs —
 * only the low-level HID library is. platformio.ini needs:
 *     lib_deps = arduino-libraries/Keyboard
 * (the Arduino IDE bundles them, which is why this compiles there and not
 * here until the dependency is added.)
 */

#include <Arduino.h>
#include <Keyboard.h>

#include "board.h"

#define ARM_PIN         BOARD_PIN_D9   /* hold LOW to arm; floats HIGH via pull-up */
#define BOOT_GRACE_MS   5000u          /* dead time after reset — do not shorten */
#define REPEAT_MS       3000u          /* minimum gap between HID bursts          */

static uint32_t last_send_ms = 0;
static bool     armed        = false;

void setup()
{
    pinMode(BOARD_PIN_LED, OUTPUT);
    pinMode(ARM_PIN, INPUT_PULLUP);    /* open = HIGH = disarmed */

    /* Grace window: blink fast, send nothing. If a previous version of this
     * sketch is misbehaving, this is when you re-flash it. */
    for (uint32_t t = 0; t < BOOT_GRACE_MS; t += 100) {
        digitalWrite(BOARD_PIN_LED, (t / 100) & 1);
        delay(100);
    }
    digitalWrite(BOARD_PIN_LED, LOW);

    /* Keyboard.begin() re-enumerates the device with a HID interface added
     * to the CDC one. The board keeps its serial port — a composite device,
     * so uploads still work normally. */
    Keyboard.begin();
    last_send_ms = millis();
}

void loop()
{
    const uint32_t now = millis();

    /* Re-read the arm pin every pass: pulling the jumper mid-run stops
     * output immediately rather than at the next reset. */
    armed = (digitalRead(ARM_PIN) == LOW);
    digitalWrite(BOARD_PIN_LED, armed);

    if (!armed) {
        Keyboard.releaseAll();   /* never leave a key stuck down */
        return;
    }

    if (now - last_send_ms >= REPEAT_MS) {
        last_send_ms = now;

        /* Keyboard.print() sends a US-layout scancode sequence. On a host
         * set to any other layout the characters that come out differ —
         * HID carries key positions, not letters. Stick to letters and
         * digits unless you are willing to handle layouts. */
        Keyboard.print(F("beetle"));
        Keyboard.write(KEY_RETURN);
    }
}
