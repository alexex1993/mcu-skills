/*
 * JC4880P443 — minimal variant.
 *
 * Touches exactly five things: the toolchain, the silicon-revision setting,
 * the console route, the PSRAM mapping — and the backlight, which is the only
 * light on this board you can control.  There is no user LED: the red D1 is a
 * power indicator on the 3V3 rail.
 *
 * GPIO23 is the MP3202 boost enable (10K pull-down R72).  Toggling it blinks
 * the panel backlight even though the ST7701 is never initialised: the glass
 * shows an unlit/grey field that brightens and darkens once a second.  If you
 * see that and the console prints, the build chain is sound and any later
 * failure is in peripheral code.
 */

#include <inttypes.h>
#include <stdio.h>

#include "board_pins.h"
#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "jc4880";

void app_main(void)
{
    const gpio_config_t bl = {
        .pin_bit_mask = 1ULL << PIN_LCD_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&bl));

    esp_chip_info_t chip;
    esp_chip_info(&chip);
    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);
    const size_t psram = esp_psram_get_size();

    printf("\nJC4880P443C_I_W -- minimal\n");
    printf("ESP32-P4 rev v%d.%d, %d cores, %d MHz configured\n",
           chip.revision / 100, chip.revision % 100, chip.cores, CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
    printf("flash %" PRIu32 " MB, PSRAM %u MB %s\n", flash_size >> 20, (unsigned)(psram >> 20),
           psram ? "mapped" : "*** NOT MAPPED - check CONFIG_SPIRAM in sdkconfig.defaults ***");
    printf("blinking the backlight on GPIO%d\n\n", PIN_LCD_BL);

    bool on = false;
    while (1) {
        on = !on;
        gpio_set_level(PIN_LCD_BL, on);
        if (on) {
            ESP_LOGI(TAG, "uptime %4" PRId64 " s | backlight ON | heap %u KB",
                     esp_timer_get_time() / 1000000,
                     (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >> 10));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
