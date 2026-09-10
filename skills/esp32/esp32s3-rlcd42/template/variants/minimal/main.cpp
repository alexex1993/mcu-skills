// Minimal ESP32-S3-RLCD-4.2 firmware: proves the toolchain, the flashing
// route, the USB CDC console and the PSRAM mapping — and touches nothing else.
//
// Flash this first on a board you have not used before. This board has no user
// LED, so the console IS the "is it alive?" signal: if nothing appears, the
// problem is the port, the CDC flags or the boot mode, not your code.

#include <Arduino.h>

#include "board_pins.h"

void setup() {
  Serial.begin(115200);
  // Native USB CDC only exists once the host has enumerated it. Without this
  // wait the banner below is printed into a void and the board looks dead.
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000) delay(10);

  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
  pinMode(PIN_BTN_KEY, INPUT_PULLUP);

  Serial.println();
  Serial.println("ESP32-S3-RLCD-4.2 minimal");
  Serial.printf("flash : %lu bytes\n", (unsigned long)ESP.getFlashChipSize());
  Serial.printf("psram : %s (%lu bytes)\n",
                psramFound() ? "FOUND" : "*** NOT FOUND — check "
                                         "board_build.arduino.memory_type ***",
                (unsigned long)ESP.getPsramSize());
}

void loop() {
  static uint32_t n = 0;
  Serial.printf("alive %lu   BOOT=%d KEY=%d\n", (unsigned long)n++,
                digitalRead(PIN_BTN_BOOT), digitalRead(PIN_BTN_KEY));
  delay(1000);
}
