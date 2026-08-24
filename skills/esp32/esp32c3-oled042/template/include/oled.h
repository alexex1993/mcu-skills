/* SSD1306 driver for the 72x40 panel on the ESP32-C3 0.42" OLED board.
 *
 * Page-addressed, software framebuffer, no scrolling hardware. The whole
 * framebuffer is 72 * 5 = 360 bytes, so there is no reason to draw partial
 * regions: oled_flush() pushes the lot in five I2C transactions.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/* Brings up the single I2C master bus on BOARD_I2C_SDA/SCL_GPIO, initialises
 * the panel, wipes all 128x64 of the controller's GDDRAM (power-on garbage
 * shows at the panel edges otherwise), and only then switches the display on.
 */
esp_err_t oled_init(void);

/* Text metrics for the built-in 5x7 font (include/font5x7.h). A line of
 * OLED_CHAR_STEP-wide cells gives 12 characters across the 72 px panel. */
#define OLED_CHAR_W    5
#define OLED_CHAR_H    7
#define OLED_CHAR_STEP (OLED_CHAR_W + 1)

/* Framebuffer operations -- none of these touch the bus. */
void oled_clear(void);
void oled_set_pixel(int x, int y, bool on);
void oled_draw_char(int x, int y, char c);
void oled_draw_text(int x, int y, const char *s);
int  oled_text_width(const char *s);

/* Pushes the framebuffer to the panel. */
esp_err_t oled_flush(void);

/* 0x00 - 0xFF. The panel is legible across the whole range; the factory
 * driver uses 0xFF. */
esp_err_t oled_set_contrast(uint8_t contrast);
