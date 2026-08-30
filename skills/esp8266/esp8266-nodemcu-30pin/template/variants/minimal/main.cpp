/*
 * NodeMCU 30-pin ESP8266 — minimal variant.
 *
 * Blinks the module LED on GPIO2 (D4) and prints a heartbeat. Nothing else:
 * no Wi-Fi, no filesystem. Flash this first on a board you have not used
 * before — its two outputs fail independently, so it tells you which half is
 * broken:
 *
 *   LED blinks, no serial   -> baud, cable data lines, or bridge driver
 *   serial, no LED          -> this module has no LED on GPIO2 (bare ESP-12
 *                              breakouts often do not); try GPIO16
 *   neither                 -> power, or GPIO15 held high at reset
 *
 * Both LEDs on this board are active LOW: digitalWrite(pin, LOW) lights them.
 */
#include <Arduino.h>

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.printf("\nNodeMCU alive: reset=%s core=%s cpu=%u MHz flash=%u B\n",
                ESP.getResetReason().c_str(), ESP.getCoreVersion().c_str(),
                ESP.getCpuFreqMHz(), ESP.getFlashChipRealSize());

  pinMode(LED_BUILTIN, OUTPUT);      /* GPIO2  — on the ESP-12E/F module */
  pinMode(LED_BUILTIN_AUX, OUTPUT);  /* GPIO16 — on the NodeMCU PCB      */
}

void loop()
{
  digitalWrite(LED_BUILTIN, LOW);          /* LOW = lit */
  digitalWrite(LED_BUILTIN_AUX, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(LED_BUILTIN_AUX, LOW);
  delay(500);

  static uint32_t n = 0;
  Serial.printf("tick %lu  heap %u\n", (unsigned long)++n, ESP.getFreeHeap());
}
