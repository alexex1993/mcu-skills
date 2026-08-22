/*
 * board_report.c — what the board says about itself at boot.
 *
 * On a bare devkit this is the whole diagnostic surface: there is no display
 * and no debug header, so the first flash of a new board is judged entirely
 * by what appears on UART0.
 */
#include <inttypes.h>
#include <string.h>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/gpio_reg.h"

#include "board.h"
#include "app.h"

static const char *TAG = "board";

void board_report_chip(void)
{
    esp_chip_info_t info;
    esp_chip_info(&info);

    ESP_LOGI(TAG, "%s", BOARD_NAME);
    ESP_LOGI(TAG, "  header      %d pins, %d GPIOs broken out",
             BOARD_HEADER_PINS, BOARD_EXPOSED_GPIOS);
    ESP_LOGI(TAG, "  silicon     ESP32 rev v%d.%d, %d core%s",
             info.revision / 100, info.revision % 100,
             info.cores, info.cores == 1 ? "" : "s");
    ESP_LOGI(TAG, "  radio      %s%s%s",
             (info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/BGN " : "",
             (info.features & CHIP_FEATURE_BT)       ? "BT "       : "",
             (info.features & CHIP_FEATURE_BLE)      ? "BLE"       : "");
    ESP_LOGI(TAG, "  idf         v%d.%d.%d",
             ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH);

    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
        ESP_LOGI(TAG, "  sta mac     %02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}

static const char *reset_reason_str(esp_reset_reason_t r)
{
    switch (r) {
    case ESP_RST_POWERON:  return "power-on";
    case ESP_RST_EXT:      return "EN button / external reset";
    case ESP_RST_SW:       return "esp_restart()";
    case ESP_RST_PANIC:    return "panic — check the backtrace above";
    case ESP_RST_INT_WDT:  return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT:      return "other watchdog";
    case ESP_RST_DEEPSLEEP:return "wake from deep sleep";
    case ESP_RST_BROWNOUT: return "BROWNOUT — the 3V3 rail sagged";
    case ESP_RST_SDIO:     return "SDIO";
    default:               return "unknown";
    }
}

void board_report_reset(void)
{
    esp_reset_reason_t r = esp_reset_reason();
    ESP_LOGI(TAG, "  reset       %s", reset_reason_str(r));

    if (r == ESP_RST_BROWNOUT) {
        ESP_LOGW(TAG, "  brownout means the supply, not the firmware. A USB port or");
        ESP_LOGW(TAG, "  a thin cable that cannot deliver the ~300 mA Wi-Fi TX burst");
        ESP_LOGW(TAG, "  produces exactly this, usually at the first radio packet.");
    }

    /* The strapping latches freeze at reset-release and stay readable for the
     * life of the boot. ESP32 TRM Register 6.13: GPIO_STRAP_REG bit5..bit0 are
     * MTDI, GPIO0, GPIO2, GPIO4, MTDO, GPIO5 — high bit first, which is the
     * reverse of the order you would guess from the pin list. */
    uint32_t strap = REG_READ(GPIO_STRAP_REG);
    ESP_LOGI(TAG, "  strapping   0x%02" PRIx32 "  MTDI(12)=%d GPIO0=%d GPIO2=%d GPIO4=%d MTDO(15)=%d GPIO5=%d",
             strap & 0x3f,
             (int)((strap >> 5) & 1),   /* MTDI / GPIO12 */
             (int)((strap >> 4) & 1),   /* GPIO0  */
             (int)((strap >> 3) & 1),   /* GPIO2  */
             (int)((strap >> 2) & 1),   /* GPIO4  */
             (int)((strap >> 1) & 1),   /* MTDO / GPIO15 */
             (int)((strap >> 0) & 1));  /* GPIO5  */

    if ((strap >> 4) & 1) {
        ESP_LOGI(TAG, "  GPIO0 was high at reset: normal SPI boot from flash.");
    } else {
        ESP_LOGW(TAG, "  GPIO0 was LOW at reset — the chip entered download mode.");
        ESP_LOGW(TAG, "  If nothing is holding BOOT down, suspect a stuck auto-reset");
        ESP_LOGW(TAG, "  circuit or something pulling GPIO0 low on the header.");
    }

    if ((strap >> 5) & 1) {
        ESP_LOGE(TAG, "  MTDI (GPIO12) was HIGH at reset. That straps VDD_SDIO to 1.8 V");
        ESP_LOGE(TAG, "  while the module's flash is a 3.3 V part. Boot is unreliable and");
        ESP_LOGE(TAG, "  the failure looks like flash corruption. Remove the pull-up.");
    }
}

void board_report_memory(void)
{
    uint32_t flash_size = 0;
    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        ESP_LOGI(TAG, "  flash       %" PRIu32 " MB", flash_size / (1024 * 1024));
    }
    ESP_LOGI(TAG, "  heap        %u B free, %u B largest block, %u B low-water",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));

    esp_partition_iterator_t it =
        esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    for (; it != NULL; it = esp_partition_next(it)) {
        const esp_partition_t *p = esp_partition_get(it);
        ESP_LOGI(TAG, "  partition   %-8s @ 0x%06" PRIx32 "  %7" PRIu32 " B",
                 p->label, p->address, p->size);
    }
    esp_partition_iterator_release(it);
}

void board_report_all(void)
{
    ESP_LOGI(TAG, "----------------------------------------------------------");
    board_report_chip();
    board_report_reset();
    board_report_memory();
    ESP_LOGI(TAG, "----------------------------------------------------------");
}
