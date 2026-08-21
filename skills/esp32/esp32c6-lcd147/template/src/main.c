// Effect carousel for the Waveshare ESP32-C6-LCD-1.47.
//
// Boots the panel, then loops forever: show a title card for the next effect,
// run it for its duration, advance. The card is a short animated beat between
// full-screen effects so the switch feels intentional rather than abrupt.

#include <stdio.h>

#include "esp_log.h"
#include "esp_timer.h"

#include "effects.h"
#include "gfx.h"
#include "rgb_led.h"

static const char *TAG = "demo";

#define COLOR_BG     GFX_RGB(0x10, 0x14, 0x20)
#define COLOR_TITLE  GFX_RGB(0xFF, 0xFF, 0xFF)
#define COLOR_ACCENT GFX_RGB(0x35, 0xD0, 0xA5)
#define COLOR_DIM    GFX_RGB(0x6A, 0x73, 0x88)

#define CARD_MS 1300

// One accent colour per effect, mirrored onto the RGB LED so the board tells
// you where the carousel is even with the panel face-down.
static const uint8_t s_led_rgb[][3] = {
    { 0x20, 0x00, 0x28 },   // plasma  - violet
    { 0x00, 0x08, 0x28 },   // stars   - deep blue
    { 0x00, 0x20, 0x20 },   // tunnel  - cyan
    { 0x28, 0x08, 0x00 },   // fire    - amber
};

// A short animated splash naming the upcoming effect: a sweeping accent bar
// underneath the title, plus a one-line subtitle. Runs for CARD_MS.
static void show_title_card(const char *name, int index)
{
    const uint32_t start = esp_timer_get_time() / 1000;
    uint32_t now = start;

    do {
        now = esp_timer_get_time() / 1000;
        uint32_t elapsed = now - start;

        gfx_clear(COLOR_BG);

        gfx_draw_text_centered_fit(120, name, COLOR_TITLE, 3);

        // Sweeping underline: width follows a triangle wave over CARD_MS.
        int prog = (int)(elapsed * 256 / CARD_MS);
        if (prog > 255) prog = 255;
        int sweep = prog < 128 ? prog * 2 : (255 - prog) * 2;   // 0..255..0
        int bar_w = 20 + (sweep * (LCD_H_RES - 60)) / 256;
        gfx_fill_rect((LCD_H_RES - bar_w) / 2, 200, bar_w, 3, COLOR_ACCENT);

        char sub[24];
        snprintf(sub, sizeof(sub), "%d / %d", index + 1, g_effect_count);
        gfx_draw_text_centered_fit(230, sub, COLOR_DIM, 1);
        gfx_draw_text_centered_fit(262, "ESP32-C6 demo", COLOR_DIM, 1);

        gfx_present();
    } while ((now - start) < CARD_MS);
}

void app_main(void)
{
    ESP_LOGI(TAG, "starting up");
    gfx_init();
    rgb_led_init();
    gfx_set_backlight(GFX_BACKLIGHT_DEFAULT_PCT);
    ESP_LOGI(TAG, "panel ready, entering carousel");

    int index = 0;
    while (1) {
        const effect_t *e = g_effects[index];

        const uint8_t *led = s_led_rgb[index % (sizeof(s_led_rgb) / 3)];
        rgb_led_set(led[0], led[1], led[2]);

        show_title_card(e->name, index);

        e->init();
        const uint32_t start = esp_timer_get_time() / 1000;
        uint32_t now;
        do {
            now = esp_timer_get_time() / 1000;
            e->frame(now - start);
            gfx_present();
        } while ((now - start) < e->duration_ms);
        e->done();

        index = (index + 1) % g_effect_count;
    }
}
