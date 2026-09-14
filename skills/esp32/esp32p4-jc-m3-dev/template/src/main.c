/*
 * Guition JC-ESP32P4-M3-DEV — board self-test and live telemetry (ESP-IDF).
 *
 * At start it prints the board passport once — chip and silicon revision, MAC,
 * reset reason, the core frequency as actually measured, flash size and JEDEC
 * id, the app partition, internal and external memory, PSRAM and SRAM
 * bandwidth, an integer CPU benchmark, die temperature, an I2C scan and a GPIO
 * read-back test.  Then one telemetry line per second.
 *
 * THERE IS NO USER LED ON THIS BOARD — only the red power indicator, which is
 * not on a GPIO.  Wire an external one to a free pin of the JP1 header:
 *
 *     GPIO20 --- [330R] --- anode(+) LED cathode(-) --- GND
 *
 * The LED is optional: BLINK_GPIO is configured as INPUT_OUTPUT, so the level
 * is read back from the pad and the toggle counter in the telemetry line proves
 * the output works with nothing attached.
 *
 * Free JP1 pins: 1, 2, 3, 4, 5, 20, 32, 33, 45, 46, 47.  Change the pin with
 * -DBLINK_GPIO=NN in platformio.ini.  Do not pick 28/29/30/31/34/35/49/50/51/52
 * (Ethernet), 14-19 or 54 (the ESP32-C6 link), 39-44 (the TF card), or 35 (the
 * boot strap) — see include/board_pins.h.
 */

#include <inttypes.h>

#include "app.h"
#include "board_pins.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TICK_PERIOD_MS 50
#define BLINK_PERIOD_MS 500
#define REPORT_PERIOD_MS 1100

static const char *TAG = "p4";

static void test_gpio(void)
{
    gpio_set_level(BLINK_GPIO, 1);
    const int high = gpio_get_level(BLINK_GPIO);
    gpio_set_level(BLINK_GPIO, 0);
    const int low = gpio_get_level(BLINK_GPIO);

    printf("GPIO%-2d   drove 1 -> read %d, drove 0 -> read %d  [%s]\n",
           BLINK_GPIO, high, low, (high == 1 && low == 0) ? "OK" : "FAIL");
}

void app_main(void)
{
    const gpio_config_t led_cfg = {
        .pin_bit_mask = 1ULL << BLINK_GPIO,
        .mode = GPIO_MODE_INPUT_OUTPUT, /* INPUT_OUTPUT so the pad can be read back */
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&led_cfg));

    board_report_init();

    printf("\n=========================================================\n");
    printf(" JC-ESP32P4-M3-DEV -- self-test\n");
    printf("=========================================================\n");
    board_report_print_all();
    i2c_scan_print();
    test_gpio();
    printf("=========================================================\n");
    printf("Then once a second: uptime, LED state, temperature, free\n");
    printf("memory, a word from the hardware RNG.\n\n");

    /* Blink and report run on separate deadlines over one common tick.  Sleeping
     * BLINK_PERIOD_MS and printing every REPORT_PERIOD_MS would make the periods
     * commensurate, so the report would always sample the same phase and the pin
     * would look stuck at one level. */
    bool led = false;
    uint32_t toggles = 0;
    int64_t next_blink = 0;
    int64_t next_report = 0;

    while (1) {
        const int64_t now_us = esp_timer_get_time();

        if (now_us >= next_blink) {
            next_blink = now_us + BLINK_PERIOD_MS * 1000;
            led = !led;
            toggles++;
            gpio_set_level(BLINK_GPIO, led);
        }

        if (now_us >= next_report) {
            next_report = now_us + REPORT_PERIOD_MS * 1000;

            ESP_LOGI(TAG,
                     "uptime %4" PRId64 " s | LED %s (pad reads %d, %" PRIu32 " toggles) | %.1f C | "
                     "heap %u KB | PSRAM %u KB | rnd 0x%08" PRIx32,
                     now_us / 1000000, led ? "ON " : "OFF", gpio_get_level(BLINK_GPIO), toggles,
                     board_report_temperature(),
                     (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >> 10),
                     (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >> 10),
                     esp_random());
        }

        vTaskDelay(pdMS_TO_TICKS(TICK_PERIOD_MS));
    }
}
