/* ESP32-C3 0.42" OLED -- minimal template.
 *
 * No display, no I2C. Blinks the on-board blue LED and prints a heartbeat.
 * Flash this first on a board you have not used before: the two outputs fail
 * independently, so it tells you which half of the chain is broken.
 *
 *   LED blinks, no log  -> the console is not on USB Serial/JTAG. Check
 *                          CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y.
 *   log, no LED         -> GPIO8 is wired the other way on your board, or
 *                          you have BOARD_LED_ON inverted.
 *   neither             -> the image is not running. Re-flash from download
 *                          mode: hold BOOT, tap RESET, release BOOT.
 */

#include "board.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "c3-oled042";

void app_main(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BOARD_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));

    ESP_LOGI(TAG, "minimal template up, LED on GPIO%d (LOW = lit)",
             BOARD_LED_GPIO);

    uint32_t tick = 0;
    while (1) {
        gpio_set_level(BOARD_LED_GPIO, tick % 2 ? BOARD_LED_ON : BOARD_LED_OFF);
        if (tick % 10 == 0) {
            ESP_LOGI(TAG, "alive, tick %lu", (unsigned long)tick);
        }
        tick++;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
