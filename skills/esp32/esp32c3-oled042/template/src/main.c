/* ESP32-C3 0.42" OLED -- full template.
 *
 * Three things run at once so that a failure is easy to localise:
 *   - a marquee on the OLED   (proves I2C and the 72x40 window offset)
 *   - the blue LED on GPIO8   (proves GPIO, and that LOW means lit)
 *   - a console heartbeat     (proves the USB Serial/JTAG console)
 *
 * If the log appears and the panel does not, the fault is on the I2C side,
 * not in the toolchain or the flashing route.
 */

#include <stdio.h>

#include "board.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "oled.h"

static const char *TAG = "c3-oled042";

#define FRAME_MS   35
#define MARQUEE    "Hello, world!"

static void led_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BOARD_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    gpio_set_level(BOARD_LED_GPIO, BOARD_LED_OFF);
}

void app_main(void)
{
    led_init();

    esp_err_t err = oled_init();
    if (err != ESP_OK) {
        /* Almost always ESP_ERR_TIMEOUT: nothing answered at 0x3C. Check that
         * SDA/SCL really are GPIO5/GPIO6 on your board revision. */
        ESP_LOGE(TAG, "oled_init failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "OLED %dx%d up, column offset %d",
             BOARD_OLED_W, BOARD_OLED_H, BOARD_OLED_COL_OFFSET);

    const int text_w = oled_text_width(MARQUEE);
    const int baseline_y = (BOARD_OLED_H - OLED_CHAR_H) / 2;
    int scroll_x = BOARD_OLED_W;
    uint32_t frame = 0;

    while (1) {
        oled_clear();
        oled_draw_text(scroll_x, baseline_y, MARQUEE);
        err = oled_flush();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "oled_flush failed: %s", esp_err_to_name(err));
        }

        if (--scroll_x < -text_w) {
            scroll_x = BOARD_OLED_W;
        }

        /* ~1 Hz on a 35 ms frame. LOW lights the LED. */
        gpio_set_level(BOARD_LED_GPIO,
                       (frame / 14) % 2 ? BOARD_LED_ON : BOARD_LED_OFF);

        if (frame % 100 == 0) {
            ESP_LOGI(TAG, "frame %lu", (unsigned long)frame);
        }
        frame++;

        vTaskDelay(pdMS_TO_TICKS(FRAME_MS));
    }
}
