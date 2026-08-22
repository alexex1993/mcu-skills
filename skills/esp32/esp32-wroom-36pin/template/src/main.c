/*
 * main.c — board self-test for a bare ESP32-WROOM-32 devkit.
 *
 * The board has no display, no sensors and (at most) one LED, so "does it
 * work?" has to be answered on the console. This runs the checks in the order
 * that isolates faults: console first, then GPIO, then analog, then the
 * radio — each stage only depends on the ones before it.
 *
 * Hold BOOT for ~1 s at any time to re-run the report.
 */
#include <inttypes.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "app.h"
#include "board.h"

static const char *TAG = "main";

/* ---------------------------------------------------------------- boot count */

static uint32_t bump_boot_counter(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* First boot after a partition-table change, or a truncated write. */
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    nvs_handle_t h;
    if (nvs_open("board", NVS_READWRITE, &h) != ESP_OK) return 0;

    uint32_t n = 0;
    nvs_get_u32(h, "boots", &n);
    n++;
    nvs_set_u32(h, "boots", n);
    nvs_commit(h);
    nvs_close(h);
    return n;
}

/* ----------------------------------------------------------------- heartbeat */
/*
 * LEDC rather than gpio_set_level(): a hard on/off blink and a stuck-high pin
 * look identical at a glance, whereas a fade is unmistakably "code is running".
 * LEDC low-speed channels can also keep running during light sleep.
 */
#if BOARD_HAS_USER_LED
#define LED_TIMER   LEDC_TIMER_0
#define LED_CHANNEL LEDC_CHANNEL_0
#define LED_RES     LEDC_TIMER_10_BIT
#define LED_MAX     ((1 << 10) - 1)

static void led_init(void)
{
    ledc_timer_config_t t = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = LED_TIMER,
        .duty_resolution = LED_RES,
        .freq_hz         = 5000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&t));

    ledc_channel_config_t c = {
        .gpio_num   = BOARD_LED_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LED_CHANNEL,
        .timer_sel  = LED_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&c));
}

static void led_set(uint32_t duty)
{
#if BOARD_LED_ACTIVE_LEVEL == 0
    duty = LED_MAX - duty;
#endif
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LED_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LED_CHANNEL);
}
#else
static void led_init(void) {}
static void led_set(uint32_t duty) { (void)duty; }
#endif

/* -------------------------------------------------------------- BOOT button */

static void button_init(void)
{
#if BOARD_HAS_BOOT_BUTTON
    gpio_config_t c = {
        .pin_bit_mask = 1ULL << BOARD_BOOT_BTN_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&c));
#endif
}

static bool button_pressed(void)
{
#if BOARD_HAS_BOOT_BUTTON
    return gpio_get_level(BOARD_BOOT_BTN_GPIO) == BOARD_BOOT_BTN_PRESSED;
#else
    return false;
#endif
}

/* ------------------------------------------------------------------- selftest */

static void run_selftest(void)
{
    board_report_all();

    if (analog_init() == ESP_OK) {
        int raw = 0, mv = 0;
        for (int i = 0; i < 3; i++) {
            if (analog_read_mv(&raw, &mv) == ESP_OK) {
                if (mv >= 0) {
                    ESP_LOGI(TAG, "adc  GPIO%d  raw %4d  %4d mV",
                             BOARD_ADC_DEMO_GPIO, raw, mv);
                } else {
                    ESP_LOGI(TAG, "adc  GPIO%d  raw %4d  (uncalibrated)",
                             BOARD_ADC_DEMO_GPIO, raw);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        ESP_LOGI(TAG, "adc  a floating input reads as noise, not zero — that is correct");
        analog_deinit();
    }

    /* Radio last: it is the only stage that can brown the board out, so
     * everything above it has already been reported by the time it runs. */
    if (wifi_scan_once() != ESP_OK) {
        ESP_LOGE(TAG, "wifi scan failed — suspect the 3V3 supply before the code");
    }
}

/* ---------------------------------------------------------------------- main */

void app_main(void)
{
    /* The ROM bootloader has already spat its own banner at 115200 by now. */
    uint32_t boots = bump_boot_counter();

    led_init();
    button_init();

    ESP_LOGI(TAG, "boot #%" PRIu32, boots);
    run_selftest();
    ESP_LOGI(TAG, "self-test done — hold BOOT for 1 s to repeat");

    uint32_t phase = 0;
    int64_t  press_start = 0;

    for (;;) {
        /* Triangle fade, integer only. */
        uint32_t p = phase % 2048;
        led_set(p < 1024 ? p : 2047 - p);
        phase += 16;

        if (button_pressed()) {
            if (press_start == 0) press_start = esp_timer_get_time();
            if (esp_timer_get_time() - press_start > 1000 * 1000) {
                press_start = 0;
                led_set(0);
                ESP_LOGI(TAG, "BOOT held — re-running self-test");
                run_selftest();
            }
        } else {
            press_start = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
