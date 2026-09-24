/* Module interfaces for the JC4880P443 self-test. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

/* display.c — ST7701S over MIPI-DSI, backlight, a minimal canvas. */
esp_err_t display_init(void);
esp_err_t display_set_brightness(int percent);
void display_fill_rect(int x, int y, int w, int h, uint16_t color);
void display_text(int x, int y, const char *s, int scale, uint16_t fg, uint16_t bg);
void display_flush(void);

/* i2c_bus.c — the one shared bus on GPIO7/GPIO8. */
esp_err_t i2c_bus_init(void);
i2c_master_bus_handle_t i2c_bus_handle(void);
void i2c_bus_scan_print(void);
esp_err_t es8311_read_id(uint8_t *id1, uint8_t *id2);

/* touch.c — GT911 on the shared bus. */
esp_err_t touch_init(void);
bool touch_read(uint16_t *x, uint16_t *y);

/* board_report.c — one-shot board passport. */
void board_report_init(void);
void board_report_print_all(void);
float board_report_temperature(void);

/* battery.c — IP5306 cell voltage through the 68K/100K divider on GPIO53. */
esp_err_t battery_init(void);
int battery_mv(void);
