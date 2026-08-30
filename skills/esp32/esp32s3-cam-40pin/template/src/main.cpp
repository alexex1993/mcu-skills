/* Self-test for the 40-pin ESP32-S3-WROOM CAM board.
 *
 * Boots, prints a board report, brings up the microSD card and the camera,
 * then saves one JPEG to /camera on the card every time BOOT is pressed.
 *
 * WS2812 colour code:  red = a subsystem failed   green = ready   blue = busy
 */
#include <Arduino.h>
#include "board.h"
#include "app.h"

static bool s_cam = false;
static bool s_sd  = false;

static void rgb(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(BOARD_RGB_LED, r, g, b);
}

void setup() {
  Serial.begin(115200);
  delay(300);                       /* let the USB-UART bridge settle */

  pinMode(BOARD_LED, OUTPUT);
  pinMode(BOARD_BOOT_BTN, INPUT_PULLUP);
  rgb(8, 8, 0);

  reportBoard();

  s_sd  = sdBegin();
  s_cam = cameraBegin();

  sdReport();
  cameraReport();

  rgb(s_cam && s_sd ? 0 : 16, s_cam && s_sd ? 16 : 0, 0);
  Serial.println(s_cam && s_sd
                 ? "ready — press BOOT to take a photo"
                 : "degraded — see the messages above");
}

void loop() {
  static uint32_t last = 0;

  if (digitalRead(BOARD_BOOT_BTN) == LOW) {
    delay(20);
    if (digitalRead(BOARD_BOOT_BTN) == LOW) {
      while (digitalRead(BOARD_BOOT_BTN) == LOW) delay(10);
      rgb(0, 0, 16);
      bool ok = s_cam && s_sd && cameraCaptureToSd("/camera");
      rgb(ok ? 0 : 16, ok ? 16 : 0, 0);
    }
  }

  if (millis() - last >= 1000) {
    last = millis();
    digitalWrite(BOARD_LED, !digitalRead(BOARD_LED));
    Serial.printf("tick %lus  heap %luKB  psram %luKB\n",
                  (unsigned long)(millis() / 1000),
                  (unsigned long)(ESP.getFreeHeap() / 1024),
                  (unsigned long)(ESP.getFreePsram() / 1024));
  }
}
