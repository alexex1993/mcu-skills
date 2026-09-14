/*
 * JC-ESP32P4-M3-DEV — minimal variant.
 *
 * Touches exactly four things: the toolchain, the silicon-revision setting, the
 * console route and the PSRAM mapping.  Flash this first on a board you have
 * not used before — on a board with no user LED it is the only way to separate
 * "wrong build configuration" from "wrong peripheral code".
 *
 * If this prints, the pioarduino platform resolved, ESP-IDF built for the right
 * ESP32-P4 revision, the USB-Serial/JTAG console is live, and the 32 MB PSRAM
 * mapped.  If it does not, none of your peripheral code matters yet.
 *
 * The board has no user LED.  BLINK_GPIO is a free JP1 pin configured as
 * INPUT_OUTPUT, so the read-back proves the output works with nothing attached.
 */

#include <inttypes.h>

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

static const char *TAG = "p4";

void app_main(void)
{
    const gpio_config_t led_cfg = {
        .pin_bit_mask = 1ULL << BLINK_GPIO,
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&led_cfg));

    esp_chip_info_t chip;
    esp_chip_info(&chip);

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    const size_t psram = esp_psram_get_size();

    printf("\nJC-ESP32P4-M3-DEV -- minimal\n");
    printf("ESP32-P4 rev v%d.%d, %d cores, %d MHz configured\n",
           chip.revision / 100, chip.revision % 100, chip.cores,
           CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
    printf("flash %" PRIu32 " MB, PSRAM %u MB %s\n",
           flash_size >> 20, (unsigned)(psram >> 20),
           psram ? "mapped" : "*** NOT MAPPED — check CONFIG_SPIRAM in sdkconfig.defaults ***");
    printf("blinking GPIO%d\n\n", BLINK_GPIO);

    bool led = false;
    uint32_t toggles = 0;

    while (1) {
        led = !led;
        toggles++;
        gpio_set_level(BLINK_GPIO, led);

        /* 500 ms blink, one line every other toggle: the line is emitted on the
         * ON edge only, so the printed level never aliases against the blink. */
        if (led) {
            ESP_LOGI(TAG, "uptime %4" PRId64 " s | GPIO%d pad reads %d | %" PRIu32 " toggles | heap %u KB",
                     esp_timer_get_time() / 1000000, BLINK_GPIO, gpio_get_level(BLINK_GPIO),
                     toggles, (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >> 10));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
