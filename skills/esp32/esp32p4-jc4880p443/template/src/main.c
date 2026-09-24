/*
 * Guition JC4880P443C_I_W — board self-test (ESP-IDF).
 *
 * Console: the board passport (silicon revision, measured clock, flash, PSRAM,
 * benchmarks, die temperature), an I2C scan, the ES8311 chip ID, then one
 * telemetry line a second.
 *
 * Screen: colour bars, a status block and a drawing area — touch it and red
 * dots appear under your finger.  SW1 (BOOT, GPIO35) toggles the backlight
 * between 100 % and 20 %.
 */

#include <inttypes.h>
#include <stdio.h>

#include "app.h"
#include "board_pins.h"
#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "jc4880";

#define WHITE RGB565(255, 255, 255)
#define BLACK RGB565(0, 0, 0)
#define GREY RGB565(40, 40, 40)
#define RED RGB565(255, 0, 0)
#define DRAW_Y 360

static void draw_static(void)
{
    static const uint16_t bars[] = {
        RGB565(255, 255, 255), RGB565(255, 255, 0), RGB565(0, 255, 255), RGB565(0, 255, 0),
        RGB565(255, 0, 255), RGB565(255, 0, 0), RGB565(0, 0, 255), RGB565(0, 0, 0),
    };
    display_fill_rect(0, 0, LCD_H_RES, LCD_V_RES, BLACK);
    for (int i = 0; i < 8; i++) display_fill_rect(i * 60, 0, 60, 60, bars[i]);
    display_text(12, 76, "JC4880P443 SELF-TEST", 3, WHITE, BLACK);
    display_fill_rect(0, DRAW_Y, LCD_H_RES, LCD_V_RES - DRAW_Y, GREY);
    display_text(12, DRAW_Y + 8, "touch here", 2, WHITE, GREY);
}

static void draw_status(int line, const char *s)
{
    char buf[40];
    snprintf(buf, sizeof(buf), "%-26s", s); /* pad: overwrite the old text */
    display_text(12, 120 + line * 30, buf, 2, WHITE, BLACK);
}

void app_main(void)
{
    const gpio_config_t btn = {
        .pin_bit_mask = 1ULL << PIN_BOOT_BUTTON,
        .mode = GPIO_MODE_INPUT, /* 10K external pull-up R34 */
    };
    ESP_ERROR_CHECK(gpio_config(&btn));

    board_report_init();
    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_ERROR_CHECK(battery_init());

    printf("\n=========================================================\n");
    printf(" JC4880P443C_I_W -- self-test\n");
    printf("=========================================================\n");
    board_report_print_all();
    i2c_bus_scan_print();
    uint8_t id1 = 0, id2 = 0;
    esp_err_t codec = es8311_read_id(&id1, &id2);
    printf("CODEC    ES8311 id %02X %02X  [%s]\n", id1, id2,
           (codec == ESP_OK && id1 == ES8311_CHIP_ID1 && id2 == ES8311_CHIP_ID2) ? "OK" : "FAIL");

    ESP_ERROR_CHECK(display_init());
    draw_static();
    display_flush();
    int brightness = 100;
    display_set_brightness(brightness);

    esp_err_t tp = touch_init();
    printf("TOUCH    GT911: %s\n", esp_err_to_name(tp));
    printf("=========================================================\n\n");

    char line[40];
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    snprintf(line, sizeof(line), "P4 rev v%d.%d  PSRAM %u MB", chip.revision / 100, chip.revision % 100,
             (unsigned)(esp_psram_get_size() >> 20));
    draw_status(0, line);

    bool last_btn = true;
    int64_t next_report = 0;
    uint32_t touches = 0;

    while (1) {
        bool dirty = false;
        uint16_t x, y;

        if (tp == ESP_OK && touch_read(&x, &y)) {
            touches++;
            if (y > DRAW_Y + 30) {
                display_fill_rect(x - 5, y - 5, 10, 10, RED);
            }
            snprintf(line, sizeof(line), "touch %3u,%3u  n=%" PRIu32, x, y, touches);
            draw_status(3, line);
            dirty = true;
        }

        const bool pressed = gpio_get_level(PIN_BOOT_BUTTON) == 0;
        if (pressed && last_btn) {
            brightness = brightness == 100 ? 20 : 100;
            display_set_brightness(brightness);
            ESP_LOGI(TAG, "BOOT pressed -> backlight %d%%", brightness);
        }
        last_btn = !pressed;

        const int64_t now = esp_timer_get_time();
        if (now >= next_report) {
            next_report = now + 1000000;
            const float t = board_report_temperature();
            const int vbat = battery_mv();
            snprintf(line, sizeof(line), "die %.1f C  vbat %d mV", t, vbat);
            draw_status(1, line);
            snprintf(line, sizeof(line), "uptime %" PRId64 " s  BL %d%%", now / 1000000, brightness);
            draw_status(2, line);
            dirty = true;
            ESP_LOGI(TAG, "uptime %4" PRId64 " s | %.1f C | vbat %d mV | BL %d%% | touches %" PRIu32,
                     now / 1000000, t, vbat, brightness, touches);
        }

        if (dirty) display_flush();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
