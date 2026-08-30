#include <Arduino.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <soc/gpio_reg.h>
#include "board.h"
#include "app.h"

static const char *resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "POWERON";
    case ESP_RST_EXT:      return "EXT (EN pin)";
    case ESP_RST_SW:       return "SW (esp_restart)";
    case ESP_RST_PANIC:    return "PANIC — read the backtrace above";
    case ESP_RST_INT_WDT:  return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT:      return "WDT (other)";
    case ESP_RST_DEEPSLEEP:return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT — the 3V3 rail sagged, not a code bug";
    case ESP_RST_SDIO:     return "SDIO";
    default:               return "UNKNOWN";
  }
}

void reportBoard(void) {
  esp_chip_info_t chip;
  esp_chip_info(&chip);

  uint32_t flash_bytes = 0;
  esp_flash_get_size(NULL, &flash_bytes);

  Serial.println();
  Serial.println("=== ESP32-S3-WROOM CAM, 40-pin — board report ===");
  Serial.printf("chip        : ESP32-S3 rev %d, %d core(s) @ %lu MHz\n",
                chip.revision, chip.cores, (unsigned long)getCpuFrequencyMhz());
  Serial.printf("reset reason: %s\n", resetReasonName(esp_reset_reason()));
  Serial.printf("flash       : %lu MB (module marking N%lu...)\n",
                (unsigned long)(flash_bytes / (1024 * 1024)),
                (unsigned long)(flash_bytes / (1024 * 1024)));

  /* The single most useful line on this board: no PSRAM means no camera. */
  if (psramFound()) {
    Serial.printf("PSRAM       : %lu KB free of %lu KB — OK\n",
                  (unsigned long)(ESP.getFreePsram() / 1024),
                  (unsigned long)(ESP.getPsramSize() / 1024));
  } else {
    Serial.println("PSRAM       : *** NOT FOUND ***");
    Serial.println("              board_build.arduino.memory_type must be qio_opi");
    Serial.println("              and -DBOARD_HAS_PSRAM must be in build_flags.");
    Serial.println("              The camera cannot be initialised without it.");
  }

  Serial.printf("heap        : %lu KB free, %lu KB largest block\n",
                (unsigned long)(ESP.getFreeHeap() / 1024),
                (unsigned long)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024));
  Serial.printf("sketch      : %lu KB used of %lu KB partition\n",
                (unsigned long)(ESP.getSketchSize() / 1024),
                (unsigned long)((ESP.getSketchSize() + ESP.getFreeSketchSpace()) / 1024));

  /* Strapping latches survive until power-down; this is what the ROM read. */
  uint32_t in0 = REG_READ(GPIO_IN_REG);
  uint32_t in1 = REG_READ(GPIO_IN1_REG);
  Serial.printf("straps now  : IO0=%d IO3=%d IO45=%d IO46=%d\n",
                (int)((in0 >> 0) & 1), (int)((in0 >> 3) & 1),
                (int)((in1 >> (45 - 32)) & 1), (int)((in1 >> (46 - 32)) & 1));
  Serial.println("              IO0=0 at reset  -> download mode instead of your app");
  Serial.println("              IO45=1 at reset -> VDD_SPI 1.8 V, flash will not read");
  Serial.println("=================================================");
}
