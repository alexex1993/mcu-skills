/*
 * JC4880P443 — GT911 capacitive touch on the shared I2C bus.
 *
 * RST (GPIO22) and INT (GPIO21) ARE wired to FPC1 even though Guition's BSP
 * declares them NC.  Hand them to the driver WITH driver_data (below): only
 * then does it run the full reset + address-latch sequence and wait 60 ms
 * before the first read.
 */

#include "app.h"
#include "board_pins.h"
#include "esp_check.h"
#include "esp_lcd_touch_gt911.h"

static const char *TAG = "touch";
static esp_lcd_touch_handle_t s_tp;

esp_err_t touch_init(void)
{
    ESP_RETURN_ON_ERROR(i2c_bus_init(), TAG, "i2c");

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_cfg.scl_speed_hz = 400000;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_bus_handle(), &io_cfg, &io), TAG, "io");

    /* driver_data is what switches the driver from "bare 10 ms reset, then
     * read" (which fails here: the GT911 is not ready) to the proper
     * address-latch sequence with a 60 ms settle. */
    static esp_lcd_touch_io_gt911_config_t gt911_cfg = {.dev_addr = I2C_ADDR_GT911};
    const esp_lcd_touch_config_t cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = PIN_TOUCH_RST,
        .int_gpio_num = PIN_TOUCH_INT,
        .levels = {.reset = 0, .interrupt = 0},
        .driver_data = &gt911_cfg,
    };
    return esp_lcd_touch_new_i2c_gt911(io, &cfg, &s_tp);
}

bool touch_read(uint16_t *x, uint16_t *y)
{
    esp_lcd_touch_point_data_t pt[1];
    uint8_t n = 0;
    if (esp_lcd_touch_read_data(s_tp) != ESP_OK) return false;
    if (esp_lcd_touch_get_data(s_tp, pt, &n, 1) != ESP_OK || n == 0) return false;
    *x = pt[0].x;
    *y = pt[0].y;
    return true;
}
