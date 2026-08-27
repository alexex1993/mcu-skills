/*
 * ProMicro nRF52840 (V1940) — minimal template.
 *
 * Blinks the on-board blue LED (P0.15) and nothing else. No USB CDC, so this
 * is the cheapest possible proof that the toolchain, the vendored board
 * definition, the variant pin map and the UF2 bootloader all work.
 *
 * If this does not blink after a successful upload, the problem is the LED
 * pin on your board revision (rebuild the full variant with
 * -DBLINK_ALL_LED_CANDIDATES=1), not your code.
 */

#include <Arduino.h>
#include "board.h"

static const uint32_t BLINK_INTERVAL_MS = 500;

static uint32_t lastToggle = 0;
static bool     ledIsOn    = false;

void setup() {
  pinMode(BOARD_PIN_LED, OUTPUT);
  ledOff(BOARD_PIN_LED);
  lastToggle = millis();
}

void loop() {
  const uint32_t now = millis();

  if (now - lastToggle >= BLINK_INTERVAL_MS) {
    lastToggle = now;
    ledIsOn = !ledIsOn;
    ledIsOn ? ledOn(BOARD_PIN_LED) : ledOff(BOARD_PIN_LED);
  }
}
