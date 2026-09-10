// ESP32-S3-RLCD-4.2 board self-test.
//
// Boots, reports the module, scans the I2C bus, then draws a temperature /
// humidity / battery dashboard on the reflective panel and parks the panel in
// low power mode. KEY redraws; BOOT is left to the bootloader.
//
// The panel is reflective and has no backlight: it forms its image from
// ambient light and holds it without being refreshed. Read it in a lit room.

#include <Arduino.h>

#include "app.h"
#include "board_pins.h"

static uint32_t refreshes = 0;

static void redraw() {
  float tempC = NAN, rh = NAN;
  if (!shtc3Read(&tempC, &rh)) Serial.println("SHTC3 did not answer");

  float battV = batteryVolts();

  // A redraw has to happen in HPM: writing while the panel is in LPM can take
  // up to a full self-refresh period (~1 s) to become visible.
  displayLeaveLowPower();
  displayDrawDashboard(tempC, rh, battV, ++refreshes);
  displayEnterLowPower();

  Serial.printf("refresh #%lu: %.1f C, %.0f %%RH, %.2f V\n",
                (unsigned long)refreshes, tempC, rh, battV);
}

void setup() {
  Serial.begin(115200);
  // Native USB CDC: the host has to enumerate before anything is readable, so
  // the first second or so of output is lost without this wait.
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000) delay(10);

  sensorsBegin();
  reportBoard();
  i2cScan();

  struct tm now;
  if (pcf85063ReadTime(&now)) {
    Serial.printf("rtc         : %04d-%02d-%02d %02d:%02d:%02d\n",
                  now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour,
                  now.tm_min, now.tm_sec);
  } else {
    Serial.println("rtc         : oscillator stop flag set (no RTC cell fitted,"
                   " or never set) — time is not trustworthy");
  }

  displayBegin();
  redraw();

  pinMode(PIN_BTN_KEY, INPUT_PULLUP);
}

void loop() {
  // KEY is active low. Redraw on the press edge, with a crude debounce.
  static bool wasDown = false;
  bool isDown = digitalRead(PIN_BTN_KEY) == LOW;
  if (isDown && !wasDown) {
    delay(30);
    if (digitalRead(PIN_BTN_KEY) == LOW) redraw();
  }
  wasDown = isDown;

  // Otherwise refresh once a minute. A reflective panel holds its image with
  // no host involvement, so there is nothing to gain from redrawing faster.
  static uint32_t lastMs = 0;
  if (millis() - lastMs >= 60000) {
    lastMs = millis();
    redraw();
  }

  delay(20);
}
