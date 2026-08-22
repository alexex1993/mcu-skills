/*
 * Minimal variant — LED blink and a console heartbeat, nothing else.
 *
 * Flash this first on a board you have not used before. It touches only the
 * toolchain, the flashing route, UART0 and one GPIO, so its two outputs fail
 * independently: if the LED blinks but the console is silent the problem is
 * the serial side, and if the console prints but the LED is dark the LED is
 * on a different pin than this board.h claims (or the board has none).
 */
#include <inttypes.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"

static const char *TAG = "blink";

void app_main(void)
{
    ESP_LOGI(TAG, "%s — minimal variant", BOARD_NAME);

#if BOARD_HAS_USER_LED
    gpio_config_t c = {
        .pin_bit_mask = 1ULL << BOARD_LED_GPIO,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&c));
    ESP_LOGI(TAG, "user LED on GPIO%d, active %s",
             BOARD_LED_GPIO, BOARD_LED_ACTIVE_LEVEL ? "high" : "low");
#else
    ESP_LOGW(TAG, "this board has no user LED — console heartbeat only");
#endif

    uint32_t n = 0;
    for (;;) {
        int on = (int)(n & 1);
#if BOARD_HAS_USER_LED
        gpio_set_level(BOARD_LED_GPIO, on == BOARD_LED_ACTIVE_LEVEL ? 1 : 0);
#endif
        ESP_LOGI(TAG, "tick %" PRIu32 " %s", n, on ? "on" : "off");
        n++;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
