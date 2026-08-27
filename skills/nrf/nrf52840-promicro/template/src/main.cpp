/*
 * ProMicro nRF52840 (V1940) — full template.
 *
 * Non-blocking LED heartbeat, a USB-CDC status line, a 12-bit ADC reading on
 * A1 and the die temperature. No BLE (see variants/ble/main.cpp for that).
 *
 * Three things in here are board-specific and worth keeping:
 *   - Adafruit_TinyUSB.h is what defines `Serial`. Without it `Serial` is
 *     Serial1 (the header UART) on some core versions and you get nothing
 *     on USB.
 *   - Nothing waits for `Serial` to open. `while (!Serial)` on a board with
 *     no USB host attached hangs forever, which looks exactly like a crash.
 *   - The ADC is read on A1 (P0.02). A0 is P1.15 and has no ADC channel.
 */

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>   // defines the USB CDC `Serial` object
#include "board.h"

#ifndef BLINK_ALL_LED_CANDIDATES
#define BLINK_ALL_LED_CANDIDATES 0
#endif

static const uint8_t LED_PINS[] = {
    BOARD_PIN_LED,
#if BLINK_ALL_LED_CANDIDATES
    BOARD_PIN_LED_ALT1,
    BOARD_PIN_LED_ALT2,
#endif
};
static const size_t LED_COUNT = sizeof(LED_PINS) / sizeof(LED_PINS[0]);

static const uint32_t BLINK_INTERVAL_MS  = 500;
static const uint32_t REPORT_INTERVAL_MS = 2000;

static uint32_t lastToggle = 0;
static uint32_t lastReport = 0;
static bool     ledIsOn    = false;

void setup() {
  Serial.begin(115200);   // USB CDC; do NOT block on `while (!Serial)`

  for (size_t i = 0; i < LED_COUNT; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    ledOff(LED_PINS[i]);
  }

  // 12 of the SAADC's 14 bits; the core default is 10.
  analogReadResolution(12);

  lastToggle = millis();
  lastReport = millis();
}

void loop() {
  const uint32_t now = millis();

  if (now - lastToggle >= BLINK_INTERVAL_MS) {
    lastToggle = now;
    ledIsOn = !ledIsOn;

    // ledOn()/ledOff() apply LED_STATE_ON from variant.h, so this stays
    // correct on a revision where the LED is active LOW.
    for (size_t i = 0; i < LED_COUNT; i++) {
      ledIsOn ? ledOn(LED_PINS[i]) : ledOff(LED_PINS[i]);
    }
  }

  if (now - lastReport >= REPORT_INTERVAL_MS) {
    lastReport = now;

    const int   raw = analogRead(BOARD_PIN_A1);        // P0.02 / AIN0
    const float mv  = raw * (BOARD_ADC_FULLSCALE_MV / 4096.0f);

    Serial.printf("up=%lus  led=%s  A1=%d (%.0f mV)  die=%.1f C\n",
                  (unsigned long)(now / 1000),
                  ledIsOn ? "on" : "off",
                  raw, mv,
                  readCPUTemperature());
  }
}
