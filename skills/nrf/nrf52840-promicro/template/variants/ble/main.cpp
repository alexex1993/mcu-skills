/*
 * ProMicro nRF52840 (V1940) — BLE template.
 *
 * Advertises a Nordic UART Service (BLEUart) and echoes it to USB CDC, plus
 * the LED heartbeat. Connect with nRF Connect or Adafruit Bluefruit Connect
 * and anything you type appears on the USB console and vice versa.
 *
 * Board-specific points:
 *   - Bluefruit.begin() starts the SoftDevice, which takes over the
 *     low-frequency clock. On a clone with no 32.768 kHz crystal this is the
 *     exact line that hangs if variant.h says USE_LFXO — the LED stops, USB
 *     stays enumerated, nothing else happens. Keep USE_LFRC.
 *   - Bluefruit.setTxPower() accepts only the values in the doc comment
 *     below; anything else is silently clamped.
 *   - The SoftDevice owns RAM below 0x20006000 and several peripherals
 *     (TIMER0, RTC0, part of PPI). Do not touch them directly once BLE is up.
 */

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <bluefruit.h>
#include "board.h"

BLEUart bleuart;

static const uint32_t BLINK_INTERVAL_MS = 500;
static uint32_t lastToggle = 0;
static bool     ledIsOn    = false;

static void startAdvertising() {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);   // units of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);     // seconds in fast mode
  Bluefruit.Advertising.start(0);               // 0 = advertise forever
}

void setup() {
  Serial.begin(115200);   // no `while (!Serial)` — see src/main.cpp

  pinMode(BOARD_PIN_LED, OUTPUT);
  ledOff(BOARD_PIN_LED);

  Bluefruit.begin();                     // starts SoftDevice S140
  Bluefruit.setTxPower(4);               // -40 -20 -16 -12 -8 -4 0 +2 +3 +4 +5 +6 +7 +8
  Bluefruit.setName("ProMicro-nRF52840");

  bleuart.begin();
  startAdvertising();

  lastToggle = millis();
}

void loop() {
  const uint32_t now = millis();

  if (now - lastToggle >= BLINK_INTERVAL_MS) {
    lastToggle = now;
    ledIsOn = !ledIsOn;
    ledIsOn ? ledOn(BOARD_PIN_LED) : ledOff(BOARD_PIN_LED);
  }

  while (bleuart.available()) {
    Serial.write((char)bleuart.read());
  }
  while (Serial.available()) {
    bleuart.write((char)Serial.read());
  }
}
