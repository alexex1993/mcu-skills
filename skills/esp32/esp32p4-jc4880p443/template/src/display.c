/*
 * JC4880P443 — ST7701S MIPI-DSI panel, backlight and a tiny framebuffer canvas.
 *
 * Order matters and every step is load-bearing:
 *   1. LDO channel 3 @ 2500 mV   (VDD_MIPI_DPHY — without it the DSI bus fails)
 *   2. DSI bus, 2 lanes @ 750 Mbps
 *   3. DBI command IO + DPI video panel with THIS panel's timings
 *   4. ST7701 driver with THIS panel's init_cmds (registry default is for a
 *      different glass and leaves the screen black or striped)
 *   5. reset (GPIO5) + init, then the backlight (GPIO23, 10K pull-down)
 */

#include <string.h>

#include "app.h"
#include "board_pins.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7701.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "font5x7.h"

static const char *TAG = "display";

static esp_lcd_panel_handle_t s_panel;
static uint16_t *s_fb;

/* Guition's sequence for this glass, verbatim from the vendored
 * esp_lcd_st7701 in the JC4880P443C_I_W archive (1-Demo/idf_examples/
 * ESP-IDF_5.5.4/common_components).  Guition patched it into the driver's
 * *default* table instead of passing it in, so a registry copy of the driver
 * does not carry it. */
static const st7701_lcd_init_cmd_t s_init_cmds[] = {
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]){0x08}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x63, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t[]){0x10, 0x08}, 2, 0},
    {0xCC, (uint8_t[]){0x10}, 1, 0},
    {0xB0, (uint8_t[]){0x80, 0x09, 0x53, 0x0C, 0xD0, 0x07, 0x0C, 0x09, 0x09, 0x28, 0x06, 0xD4, 0x13, 0x69, 0x2B, 0x71}, 16, 0},
    {0xB1, (uint8_t[]){0x80, 0x94, 0x5A, 0x10, 0xD3, 0x06, 0x0A, 0x08, 0x08, 0x25, 0x03, 0xD3, 0x12, 0x66, 0x6A, 0x0D}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x5D}, 1, 0},
    {0xB1, (uint8_t[]){0x58}, 1, 0},
    {0xB2, (uint8_t[]){0x87}, 1, 0},
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x4E}, 1, 0},
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x21}, 1, 0},
    {0xB9, (uint8_t[]){0x10, 0x1F}, 2, 0},
    {0xBB, (uint8_t[]){0x03}, 1, 0},
    {0xBC, (uint8_t[]){0x00}, 1, 0},
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xD0, (uint8_t[]){0x88}, 1, 0},
    {0xE0, (uint8_t[]){0x00, 0x3A, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x04, 0xA0, 0x00, 0xA0, 0x05, 0xA0, 0x00, 0xA0, 0x00, 0x40, 0x40}, 11, 0},
    {0xE2, (uint8_t[]){0x30, 0x00, 0x40, 0x40, 0x32, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00}, 13, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t[]){0x09, 0x2E, 0xA0, 0xA0, 0x0B, 0x30, 0xA0, 0xA0, 0x05, 0x2A, 0xA0, 0xA0, 0x07, 0x2C, 0xA0, 0xA0}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t[]){0x08, 0x2D, 0xA0, 0xA0, 0x0A, 0x2F, 0xA0, 0xA0, 0x04, 0x29, 0xA0, 0xA0, 0x06, 0x2B, 0xA0, 0xA0}, 16, 0},
    {0xEB, (uint8_t[]){0x00, 0x00, 0x4E, 0x4E, 0x00, 0x00, 0x00}, 7, 0},
    {0xEC, (uint8_t[]){0x08, 0x01}, 2, 0},
    {0xED, (uint8_t[]){0xB0, 0x2B, 0x98, 0xA4, 0x56, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0x65, 0x4A, 0x89, 0xB2, 0x0B}, 16, 0},
    {0xEF, (uint8_t[]){0x08, 0x08, 0x08, 0x45, 0x3F, 0x54}, 6, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x11, (uint8_t[]){0x00}, 1, 120}, /* sleep out */
    {0x29, (uint8_t[]){0x00}, 1, 20},  /* display on */
};

#define BL_LEDC_TIMER LEDC_TIMER_0
#define BL_LEDC_CH LEDC_CHANNEL_0

static esp_err_t backlight_init(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = BL_LEDC_TIMER,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "ledc timer");
    const ledc_channel_config_t ch = {
        .gpio_num = PIN_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BL_LEDC_CH,
        .timer_sel = BL_LEDC_TIMER,
        .duty = 0, /* keep dark until the first frame is in */
    };
    return ledc_channel_config(&ch);
}

esp_err_t display_set_brightness(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CH, 1023 * percent / 100), TAG, "duty");
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CH);
}

esp_err_t display_init(void)
{
    ESP_RETURN_ON_ERROR(backlight_init(), TAG, "backlight");

    esp_ldo_channel_handle_t ldo = NULL;
    const esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = LCD_DSI_LDO_CHAN,
        .voltage_mv = LCD_DSI_LDO_MV,
    };
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_cfg, &ldo), TAG, "DSI PHY LDO");

    esp_lcd_dsi_bus_handle_t bus = NULL;
    const esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id = 0,
        .num_data_lanes = LCD_DSI_LANES,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = LCD_DSI_LANE_MBPS,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_cfg, &bus), TAG, "DSI bus");

    esp_lcd_panel_io_handle_t io = NULL;
    const esp_lcd_dbi_io_config_t dbi_cfg = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(bus, &dbi_cfg, &io), TAG, "DBI IO");

    /* Do NOT use ST7701_480_360_PANEL_60HZ_DPI_CONFIG from the registry
     * driver: it is 480x360 @ 15 MHz for another panel. */
    const esp_lcd_dpi_panel_config_t dpi_cfg = {
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = LCD_DPI_CLK_MHZ,
        .virtual_channel = 0,
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
        .num_fbs = 1,
        .video_timing = {
            .h_size = LCD_H_RES,
            .v_size = LCD_V_RES,
            .hsync_pulse_width = LCD_HSYNC_PW,
            .hsync_back_porch = LCD_HSYNC_BP,
            .hsync_front_porch = LCD_HSYNC_FP,
            .vsync_pulse_width = LCD_VSYNC_PW,
            .vsync_back_porch = LCD_VSYNC_BP,
            .vsync_front_porch = LCD_VSYNC_FP,
        },
        .flags.use_dma2d = true,
    };
    st7701_vendor_config_t vendor_cfg = {
        .init_cmds = s_init_cmds,
        .init_cmds_size = sizeof(s_init_cmds) / sizeof(s_init_cmds[0]),
        .mipi_config = {
            .dsi_bus = bus,
            .dpi_config = &dpi_cfg,
        },
        .flags.use_mipi_interface = 1,
    };
    const esp_lcd_panel_dev_config_t dev_cfg = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_cfg,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7701(io, &dev_cfg, &s_panel), TAG, "ST7701");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "init");

    /* The DPI driver owns a PSRAM framebuffer; draw into it directly. */
    ESP_RETURN_ON_ERROR(esp_lcd_dpi_panel_get_frame_buffer(s_panel, 1, (void **)&s_fb), TAG, "fb");
    ESP_LOGI(TAG, "ST7701 up: %dx%d RGB565, fb @%p", LCD_H_RES, LCD_V_RES, s_fb);
    return ESP_OK;
}

/* Hand the framebuffer back to the DPI engine.  Passing the framebuffer's own
 * address to draw_bitmap makes the driver do only a cache write-back — without
 * it the CPU's writes sit in L2 cache and the panel shows stale pixels. */
void display_flush(void)
{
    esp_lcd_panel_draw_bitmap(s_panel, 0, 0, LCD_H_RES, LCD_V_RES, s_fb);
}

void display_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > LCD_H_RES) w = LCD_H_RES - x;
    if (y + h > LCD_V_RES) h = LCD_V_RES - y;
    for (int j = 0; j < h; j++) {
        uint16_t *row = s_fb + (y + j) * LCD_H_RES + x;
        for (int i = 0; i < w; i++) row[i] = color;
    }
}

void display_text(int x, int y, const char *s, int scale, uint16_t fg, uint16_t bg)
{
    for (; *s; s++, x += (FONT5X7_WIDTH + 1) * scale) {
        char c = *s;
        if (c < FONT5X7_FIRST_CHAR || c > FONT5X7_LAST_CHAR) c = '?';
        const uint8_t *g = &font5x7[(c - FONT5X7_FIRST_CHAR) * FONT5X7_WIDTH];
        for (int col = 0; col < FONT5X7_WIDTH + 1; col++) {
            uint8_t bits = col < FONT5X7_WIDTH ? g[col] : 0;
            for (int row = 0; row < FONT5X7_HEIGHT; row++) {
                display_fill_rect(x + col * scale, y + row * scale, scale, scale,
                                  (bits >> row) & 1 ? fg : bg);
            }
        }
    }
}
