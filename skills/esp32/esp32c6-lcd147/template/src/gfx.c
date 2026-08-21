// Graphics backend for the ESP32-C6-LCD-1.47.
//
// The hardware setup, byte-swap convention and 12x24 font renderer used to
// live in main.c; they are gathered here so the effect engine can reuse them.

#include <string.h>

#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "board.h"
#include "font12x24.h"
#include "gfx.h"

// Pin map lives in board.h; these are the local short names.
#define PIN_LCD_MOSI BOARD_PIN_LCD_MOSI
#define PIN_LCD_SCLK BOARD_PIN_LCD_SCLK
#define PIN_LCD_CS   BOARD_PIN_LCD_CS
#define PIN_LCD_DC   BOARD_PIN_LCD_DC
#define PIN_LCD_RST  BOARD_PIN_LCD_RST
#define PIN_LCD_BL   BOARD_PIN_LCD_BL

#define LCD_HOST       SPI2_HOST
#define LCD_PIXEL_CLK  BOARD_LCD_PIXEL_CLK_HZ
#define LCD_X_GAP      BOARD_LCD_X_GAP
#define LCD_Y_GAP      BOARD_LCD_Y_GAP

// Backlight PWM. 10-bit duty at 5 kHz is inaudible and far finer than the eye
// needs; LEDC keeps running through Light-sleep, so the panel does not flash.
#define BL_LEDC_TIMER      LEDC_TIMER_0
#define BL_LEDC_CHANNEL    LEDC_CHANNEL_0
#define BL_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define BL_LEDC_RES        LEDC_TIMER_10_BIT
#define BL_LEDC_FREQ_HZ    5000

uint16_t g_fb[LCD_H_RES * LCD_V_RES];
esp_lcd_panel_handle_t g_panel;

// Signaled from the SPI "color transfer done" callback; gfx_present() waits on
// it so the framebuffer is never mutated mid-transfer.
static SemaphoreHandle_t s_flush_sem;
static esp_lcd_panel_io_handle_t s_io;

static bool on_color_trans_done(esp_lcd_panel_io_handle_t io,
                                esp_lcd_panel_io_event_data_t *edata,
                                void *user_ctx)
{
    BaseType_t hpwoken = pdFALSE;
    xSemaphoreGiveFromISR(s_flush_sem, &hpwoken);
    return hpwoken == pdTRUE;
}

void gfx_init(void)
{
    s_flush_sem = xSemaphoreCreateCounting(1, 0);

    ledc_timer_config_t bl_timer = {
        .speed_mode      = BL_LEDC_MODE,
        .timer_num       = BL_LEDC_TIMER,
        .duty_resolution = BL_LEDC_RES,
        .freq_hz         = BL_LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&bl_timer));

    ledc_channel_config_t bl_ch = {
        .gpio_num   = PIN_LCD_BL,
        .speed_mode = BL_LEDC_MODE,
        .channel    = BL_LEDC_CHANNEL,
        .timer_sel  = BL_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&bl_ch));
    gfx_set_backlight(0);

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = PIN_LCD_CS,
        .dc_gpio_num = PIN_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = LCD_PIXEL_CLK,
        .trans_queue_depth = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .on_color_trans_done = on_color_trans_done,
        .user_ctx = NULL,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_HOST, &io_cfg, &s_io));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(s_io, &panel_cfg, &g_panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(g_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(g_panel));
    // IPS panels ship inverted; without INVON the image is a photo negative.
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(g_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(g_panel, LCD_X_GAP, LCD_Y_GAP));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(g_panel, true));
}

void gfx_set_backlight(int percent)
{
    if (percent < 0) {
        percent = 0;
    }
    if (percent > GFX_BACKLIGHT_MAX_PCT) {
        percent = GFX_BACKLIGHT_MAX_PCT;
    }
    const uint32_t max_duty = (1u << BL_LEDC_RES) - 1u;
    ESP_ERROR_CHECK(ledc_set_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL,
                                  (max_duty * (uint32_t)percent) / 100u));
    ESP_ERROR_CHECK(ledc_update_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL));
}

void gfx_present(void)
{
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(g_panel, 0, 0, LCD_H_RES,
                                              LCD_V_RES, g_fb));
    // Wait for the DMA engine to finish reading g_fb before the caller is
    // allowed to scribble over it again.
    xSemaphoreTake(s_flush_sem, portMAX_DELAY);
}

void gfx_clear(uint16_t color)
{
    for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) {
        g_fb[i] = color;
    }
}

void gfx_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > LCD_H_RES) { w = LCD_H_RES - x; }
    if (y + h > LCD_V_RES) { h = LCD_V_RES - y; }
    if (w <= 0 || h <= 0) {
        return;
    }

    for (int row = y; row < y + h; row++) {
        uint16_t *line = &g_fb[row * LCD_H_RES + x];
        for (int col = 0; col < w; col++) {
            line[col] = color;
        }
    }
}

// ---- text ------------------------------------------------------------------

// Decodes one UTF-8 codepoint from *p and advances *p past it. Malformed
// sequences collapse to '?' so a bad string can't run the pointer off the end.
static uint32_t utf8_next(const char **p)
{
    const uint8_t *s = (const uint8_t *)*p;
    uint32_t cp;
    int len;

    if (s[0] < 0x80) {
        cp = s[0]; len = 1;
    } else if ((s[0] & 0xE0) == 0xC0) {
        cp = s[0] & 0x1F; len = 2;
    } else if ((s[0] & 0xF0) == 0xE0) {
        cp = s[0] & 0x0F; len = 3;
    } else if ((s[0] & 0xF8) == 0xF0) {
        cp = s[0] & 0x07; len = 4;
    } else {
        *p += 1;
        return '?';
    }
    for (int i = 1; i < len; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            *p += 1;
            return '?';
        }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *p += len;
    return cp;
}

static int utf8_count(const char *text)
{
    int n = 0;
    for (const char *p = text; *p; ) {
        utf8_next(&p);
        n++;
    }
    return n;
}

static const uint16_t *glyph_for(uint32_t cp)
{
    if (cp >= FONT_FIRST_CHAR && cp <= FONT_LAST_CHAR) {
        return font_glyphs[cp - FONT_FIRST_CHAR];
    }
    if (cp >= FONT_CYRILLIC_FIRST_CHAR && cp <= FONT_CYRILLIC_LAST_CHAR) {
        return font_cyrillic_glyphs[cp - FONT_CYRILLIC_FIRST_CHAR];
    }
    return font_glyphs['?' - FONT_FIRST_CHAR];
}

static void draw_char(int x, int y, uint32_t cp, uint16_t color, int scale)
{
    const uint16_t *glyph = glyph_for(cp);
    for (int gy = 0; gy < FONT_HEIGHT; gy++) {
        uint16_t bits = glyph[gy];
        if (!bits) {
            continue;
        }
        for (int gx = 0; gx < FONT_WIDTH; gx++) {
            if (!(bits & (1u << (FONT_WIDTH - 1 - gx)))) {
                continue;
            }
            gfx_fill_rect(x + gx * scale, y + gy * scale, scale, scale, color);
        }
    }
}

void gfx_draw_text(int x, int y, const char *text, uint16_t color, int scale)
{
    for (const char *p = text; *p; ) {
        uint32_t cp = utf8_next(&p);
        draw_char(x, y, cp, color, scale);
        x += FONT_WIDTH * scale;
    }
}

void gfx_draw_text_centered_fit(int y, const char *text, uint16_t color,
                                int max_scale)
{
    int chars = utf8_count(text);
    int scale = max_scale;
    while (scale > 1 && chars * FONT_WIDTH * scale > LCD_H_RES) {
        scale--;
    }
    int width = chars * FONT_WIDTH * scale;
    gfx_draw_text((LCD_H_RES - width) / 2, y, text, color, scale);
}
