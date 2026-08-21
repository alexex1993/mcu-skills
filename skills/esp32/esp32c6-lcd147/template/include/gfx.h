// Low-level graphics for the Waveshare ESP32-C6-LCD-1.47 (172x320 ST7789).
//
// Owns the RGB565 framebuffer, the SPI/panel setup, and the 1-bit font
// renderer used for title cards. Effect code talks to g_fb directly and
// pushes frames through gfx_present(), which is synchronous (it waits for
// the SPI DMA transfer to finish) so there is never any tearing.
#pragma once

#include <stdint.h>

#include "esp_lcd_panel_ops.h"

#include "board.h"

#define LCD_H_RES BOARD_LCD_H_RES
#define LCD_V_RES BOARD_LCD_V_RES

// RGB565 packed in native CPU order, then byte-swapped once to the big-endian
// wire order the ST7789 expects (the panel is configured BIG-endian below, so
// esp_lcd does not swap a second time).
#define GFX_RGB(r, g, b)                                                       \
    (uint16_t)(__builtin_bswap16((uint16_t)((((r) & 0xF8) << 8) |             \
                                     (((g) & 0xFC) << 3) | ((b) >> 3))))

// Framebuffer, row-major, indexed fb[y * LCD_H_RES + x].
extern uint16_t g_fb[LCD_H_RES * LCD_V_RES];
extern esp_lcd_panel_handle_t g_panel;

// Powers up the bus and the panel, sets up the LEDC channel that dims the
// backlight, and leaves the backlight OFF. Call once at startup, then raise the
// backlight with gfx_set_backlight() once the first frame is ready.
void gfx_init(void);

// Backlight brightness, 0-100 %. Waveshare's own warning: keep it at 50 % or
// below and do not run at full brightness for long -- the panel overheats and
// develops permanent dark shadows. Values above GFX_BACKLIGHT_MAX_PCT clamp.
#define GFX_BACKLIGHT_MAX_PCT 50
#define GFX_BACKLIGHT_DEFAULT_PCT 40
void gfx_set_backlight(int percent);

// Pushes the whole framebuffer to the panel and blocks until the DMA transfer
// is complete. Safe to overwrite g_fb as soon as it returns.
void gfx_present(void);

// ---- drawing primitives ----------------------------------------------------

void gfx_clear(uint16_t color);
void gfx_fill_rect(int x, int y, int w, int h, uint16_t color);

// Plots a single pixel with bounds-checking.
static inline void gfx_plot(int x, int y, uint16_t color)
{
    if ((unsigned)x < LCD_H_RES && (unsigned)y < LCD_V_RES) {
        g_fb[y * LCD_H_RES + x] = color;
    }
}

// ---- text (uses the 12x24 bitmap font) -------------------------------------

void gfx_draw_text(int x, int y, const char *text, uint16_t color, int scale);
// Largest scale up to max_scale that still fits the glass width.
void gfx_draw_text_centered_fit(int y, const char *text, uint16_t color,
                                int max_scale);
