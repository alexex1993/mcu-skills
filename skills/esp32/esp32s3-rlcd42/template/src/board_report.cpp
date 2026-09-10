// One paste that answers "is the toolchain, the module and the console right?"
// before any peripheral is involved.

#include <Arduino.h>
#include <esp_chip_info.h>
#include <esp_system.h>

#include "app.h"
#include "board_pins.h"

static const char *resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "power-on";
    case ESP_RST_EXT:      return "external";
    case ESP_RST_SW:       return "software";
    case ESP_RST_PANIC:    return "panic";
    case ESP_RST_INT_WDT:  return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT:      return "other watchdog";
    case ESP_RST_DEEPSLEEP: return "deep sleep wake";
    case ESP_RST_BROWNOUT: return "BROWNOUT — check the supply/battery";
    case ESP_RST_SDIO:     return "SDIO";
    default:               return "unknown";
  }
}

void reportBoard() {
  esp_chip_info_t chip;
  esp_chip_info(&chip);

  Serial.println();
  Serial.println("=== ESP32-S3-RLCD-4.2 ===");
  Serial.printf("chip        : ESP32-S3 rev %d, %d core(s)\n", chip.revision,
                chip.cores);
  Serial.printf("cpu         : %lu MHz\n", (unsigned long)getCpuFrequencyMhz());
  Serial.printf("flash       : %lu bytes\n",
                (unsigned long)ESP.getFlashChipSize());

  // The single most important line: on an N16R8 this must say 8388608. If it
  // says 0, board_build.arduino.memory_type is not qio_opi and nothing that
  // needs a large buffer will work.
  Serial.printf("psram       : %s, %lu bytes free of %lu\n",
                psramFound() ? "FOUND" : "*** NOT FOUND ***",
                (unsigned long)ESP.getFreePsram(),
                (unsigned long)ESP.getPsramSize());

  Serial.printf("heap        : %lu bytes free of %lu\n",
                (unsigned long)ESP.getFreeHeap(),
                (unsigned long)ESP.getHeapSize());
  Serial.printf("reset       : %s\n", resetReasonName(esp_reset_reason()));

  // Strapping and button levels, read as inputs. Both buttons idle high.
  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
  pinMode(PIN_BTN_KEY, INPUT_PULLUP);
  Serial.printf("BOOT (GPIO%d): %s\n", PIN_BTN_BOOT,
                digitalRead(PIN_BTN_BOOT) ? "released" : "PRESSED");
  Serial.printf("KEY  (GPIO%d): %s\n", PIN_BTN_KEY,
                digitalRead(PIN_BTN_KEY) ? "released" : "PRESSED");

  Serial.printf("battery     : %.2f V (GPIO%d, x%.0f divider)\n",
                batteryVolts(), PIN_BAT_ADC, BAT_DIVIDER);
  Serial.println("=========================");
}
