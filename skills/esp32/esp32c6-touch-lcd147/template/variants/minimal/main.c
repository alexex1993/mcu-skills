/*
 * Waveshare ESP32-C6-Touch-LCD-1.47 -- minimal bring-up.
 *
 * Flash this first on a new board. It proves the toolchain, the flashing
 * route, the USB console and the shared I2C bus, and it does so with the
 * display kept out of the picture so nothing can confuse the diagnosis.
 *
 * Three independent signs of life:
 *   - a heartbeat line on the USB console every second,
 *   - the backlight breathing (LEDC PWM on GPIO23),
 *   - an I2C scan that should find 0x63 (AXS5106L touch) and 0x6B (QMI8658A).
 *
 * If the scan is silent but the heartbeat prints, the I2C bus is the problem.
 * If nothing prints at all, see the flashing section of the skill.
 */

#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_pins.h"

static const char *TAG = "minimal";

#define BL_TIMER    LEDC_TIMER_0
#define BL_CHANNEL  LEDC_CHANNEL_0
#define BL_MODE     LEDC_LOW_SPEED_MODE   /* the C6 has no high-speed mode */
#define BL_MAX_DUTY 255                   /* 8-bit resolution */

static void backlight_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = BL_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = BL_TIMER,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel = {
        .gpio_num = BSP_LCD_BL,
        .speed_mode = BL_MODE,
        .channel = BL_CHANNEL,
        .timer_sel = BL_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

static void backlight_set(int duty)
{
    ESP_ERROR_CHECK(ledc_set_duty(BL_MODE, BL_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(BL_MODE, BL_CHANNEL));
}

static void i2c_scan(i2c_master_bus_handle_t bus)
{
    int found = 0;
    for (uint16_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(bus, addr, 50) == ESP_OK) {
            const char *who = (addr == 0x63) ? "AXS5106L touch"
                            : (addr == 0x6B) ? "QMI8658A IMU"
                                             : "unknown";
            ESP_LOGI(TAG, "  0x%02X  %s", (unsigned)addr, who);
            found++;
        }
    }
    if (found == 0) {
        ESP_LOGE(TAG, "  nothing answered -- check GPIO18/19 are not reused elsewhere");
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-C6-Touch-LCD-1.47 minimal bring-up");

    /* GPIO23 has an internal weak pull-up at reset, which biases the backlight
     * transistor on. Claiming the pin with a 0 % duty is the first thing to do
     * so the panel is not lit while it still holds power-on garbage. */
    backlight_init();

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = BSP_I2C_PORT,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    ESP_LOGI(TAG, "scanning the shared I2C bus (SDA=%d SCL=%d)",
             BSP_I2C_SDA, BSP_I2C_SCL);
    i2c_scan(bus);

    for (uint32_t tick = 0;; tick++) {
        /* Triangle wave, capped at half brightness -- there is nothing on the
         * panel worth looking at and the LED string runs cooler this way. */
        int phase = tick % 40;
        int duty = (phase < 20 ? phase : 40 - phase) * BL_MAX_DUTY / 40;
        backlight_set(duty);

        if (tick % 20 == 0) {
            ESP_LOGI(TAG, "alive, %lu s", (unsigned long)(tick / 20));
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
