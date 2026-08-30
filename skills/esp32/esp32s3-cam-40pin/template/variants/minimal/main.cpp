/* Minimal variant — proves the toolchain, the flashing route, the console,
 * both LEDs and the PSRAM mapping. Nothing else. Flash this first on a board
 * you have not used before.
 *
 * Its outputs fail independently:
 *   nothing on the serial port      -> wrong USB-C socket, or wrong port
 *   console but no LED on GPIO2     -> a clone that fitted only the WS2812
 *   "PSRAM: NOT FOUND"              -> memory_type is not qio_opi; the camera
 *                                      will never work until it is
 */
#include <Arduino.h>
#include "board.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(BOARD_LED, OUTPUT);

  Serial.println();
  Serial.printf("ESP32-S3 rev %d, %lu MHz, flash %lu MB\n",
                ESP.getChipRevision(), (unsigned long)getCpuFrequencyMhz(),
                (unsigned long)(ESP.getFlashChipSize() / (1024 * 1024)));
  Serial.printf("PSRAM: %s (%lu KB)\n",
                psramFound() ? "found" : "*** NOT FOUND — fix memory_type ***",
                (unsigned long)(ESP.getPsramSize() / 1024));
}

void loop() {
  digitalWrite(BOARD_LED, HIGH);
  neopixelWrite(BOARD_RGB_LED, 0, 16, 0);
  Serial.printf("tick %lu\n", (unsigned long)(millis() / 1000));
  delay(500);

  digitalWrite(BOARD_LED, LOW);
  neopixelWrite(BOARD_RGB_LED, 0, 0, 0);
  delay(500);
}
