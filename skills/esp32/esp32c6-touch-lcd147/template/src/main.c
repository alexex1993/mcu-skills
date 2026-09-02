/*
 * Waveshare ESP32-C6-Touch-LCD-1.47 — "Hello World" + touch marker.
 *
 * Prints Hello World on the serial console and on the 1.47" JD9853 panel,
 * then follows the AXS5106L touch controller: every press draws a dot at the
 * contact point, shows its coordinates on screen and logs them to the monitor.
 *
 * How the touch is mapped: the controller's coordinate system varies between
 * panels — axes may be swapped, mirrored, or run over a different full scale.
 * Rather than hard-coding a guess, the app asks for three taps on known
 * targets the first time it runs, derives the mapping from them and stores it
 * in NVS. Hold BOOT while resetting to run the calibration again.
 *
 * The screen is redrawn tile by tile straight from the scene description, so
 * no full framebuffer is kept in RAM — only one small DMA staging buffer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "axs5106l.h"
#include "board_pins.h"
#include "esp_lcd_jd9853.h"
#include "font5x7.h"

static const char *TAG = "hello";

/* The panel takes RGB565 big-endian, the CPU is little-endian: keep every
 * colour pre-swapped so the blitters can stay dumb. */
#define RGB565(r, g, b)  ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define COLOR(r, g, b)   ((uint16_t)((RGB565(r, g, b) << 8) | (RGB565(r, g, b) >> 8)))

#define C_BG        COLOR(0x0B, 0x0E, 0x1A)
#define C_TEXT      COLOR(0xF2, 0xF4, 0xF8)
#define C_MUTED     COLOR(0x7A, 0x86, 0xA0)
#define C_ACCENT    COLOR(0x2E, 0xC4, 0xB6)
#define C_DOT       COLOR(0xFF, 0x4D, 0x6A)
#define C_DOT_RING  COLOR(0xFF, 0xD1, 0x66)

#define TILE_ROWS   32                       /* staging buffer height, in pixels */
#define DOT_RADIUS  7
#define DOT_BOX     (2 * (DOT_RADIUS + 2) + 1)

#define COORD_TEXT_X     8
#define COORD_TEXT_Y     288
#define COORD_TEXT_SCALE 2
#define COORD_BOX_H      (FONT5X7_HEIGHT * COORD_TEXT_SCALE + 4)

#define CROSS_ARM   12                       /* calibration target half-width */

#define CAL_NVS_NAMESPACE "touch"
#define CAL_NVS_KEY       "cal_v2"

/* ------------------------------------------------------------------ */
/*  Touch calibration                                                  */
/* ------------------------------------------------------------------ */

/* Two independent affine maps, one per screen axis, plus a flag telling which
 * raw axis feeds which screen axis. Mirroring falls out of the sign of the
 * slope, so it needs no separate flag. */
typedef struct {
    uint8_t swap;   /*!< screen X is driven by the controller's Y and vice versa */
    int16_t x_lo;   /*!< raw value seen at screen x = W/4   */
    int16_t x_hi;   /*!< raw value seen at screen x = 3*W/4 */
    int16_t y_lo;   /*!< raw value seen at screen y = H/4   */
    int16_t y_hi;   /*!< raw value seen at screen y = 3*H/4 */
} touch_cal_t;

static touch_cal_t s_cal;

/* The vendor library's rotation 0 for this panel: X runs against the screen,
 * Y runs with it, both over the panel's own resolution. Expressed through the
 * same two-point form the calibration produces, so there is one code path. */
static touch_cal_t cal_default(void)
{
    return (touch_cal_t){
        .swap = 0,
        .x_lo = BSP_LCD_H_RES - 1 - BSP_LCD_H_RES / 4,
        .x_hi = BSP_LCD_H_RES - 1 - BSP_LCD_H_RES * 3 / 4,
        .y_lo = BSP_LCD_V_RES / 4,
        .y_hi = BSP_LCD_V_RES * 3 / 4,
    };
}

static void cal_apply(const touch_cal_t *cal, uint16_t raw_x, uint16_t raw_y,
                      int *out_x, int *out_y)
{
    int ux = cal->swap ? raw_y : raw_x;
    int uy = cal->swap ? raw_x : raw_y;

    int span_x = cal->x_hi - cal->x_lo;
    int span_y = cal->y_hi - cal->y_lo;

    int x = span_x ? BSP_LCD_H_RES / 4 + (ux - cal->x_lo) * (BSP_LCD_H_RES / 2) / span_x : 0;
    int y = span_y ? BSP_LCD_V_RES / 4 + (uy - cal->y_lo) * (BSP_LCD_V_RES / 2) / span_y : 0;

    if (x < 0) { x = 0; }
    if (y < 0) { y = 0; }
    if (x >= BSP_LCD_H_RES) { x = BSP_LCD_H_RES - 1; }
    if (y >= BSP_LCD_V_RES) { y = BSP_LCD_V_RES - 1; }

    *out_x = x;
    *out_y = y;
}

static bool cal_load(touch_cal_t *cal)
{
    nvs_handle_t nvs;
    if (nvs_open(CAL_NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }
    size_t len = sizeof(*cal);
    esp_err_t err = nvs_get_blob(nvs, CAL_NVS_KEY, cal, &len);
    nvs_close(nvs);
    return err == ESP_OK && len == sizeof(*cal);
}

static void cal_save(const touch_cal_t *cal)
{
    nvs_handle_t nvs;
    if (nvs_open(CAL_NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        ESP_LOGW(TAG, "cannot open NVS, calibration will be asked for again");
        return;
    }
    if (nvs_set_blob(nvs, CAL_NVS_KEY, cal, sizeof(*cal)) == ESP_OK) {
        nvs_commit(nvs);
    }
    nvs_close(nvs);
}

/* ------------------------------------------------------------------ */
/*  Scene state                                                        */
/* ------------------------------------------------------------------ */

typedef enum {
    SCENE_CALIBRATE,
    SCENE_RUN,
} scene_mode_t;

static esp_lcd_panel_handle_t s_panel;
static uint16_t *s_tile;                     /* DMA staging buffer */
static SemaphoreHandle_t s_flush_done;       /* signalled when the DMA is done with s_tile */

static scene_mode_t s_mode = SCENE_RUN;
static int s_target_x, s_target_y;           /* calibration crosshair */
static char s_cal_step[16];

static bool s_has_dot;
static int s_dot_x, s_dot_y;
static char s_coord_line[24] = "X: --- Y: ---";

/* ------------------------------------------------------------------ */
/*  Tiny clipped 2D blitter over the staging buffer                    */
/* ------------------------------------------------------------------ */

typedef struct {
    int x0, y0;      /* tile origin in screen coordinates */
    int w, h;        /* tile size */
    uint16_t *buf;
} canvas_t;

static inline void put_pixel(canvas_t *c, int x, int y, uint16_t color)
{
    int lx = x - c->x0;
    int ly = y - c->y0;
    if (lx < 0 || ly < 0 || lx >= c->w || ly >= c->h) {
        return;
    }
    c->buf[ly * c->w + lx] = color;
}

/* Clipped up front: compose() repaints the background of the whole scene for
 * every band, so a per-pixel reject would dominate the redraw cost. */
static void fill_rect(canvas_t *c, int x, int y, int w, int h, uint16_t color)
{
    int lx0 = x - c->x0;
    int ly0 = y - c->y0;
    int lx1 = lx0 + w;
    int ly1 = ly0 + h;

    if (lx0 < 0) { lx0 = 0; }
    if (ly0 < 0) { ly0 = 0; }
    if (lx1 > c->w) { lx1 = c->w; }
    if (ly1 > c->h) { ly1 = c->h; }

    for (int ly = ly0; ly < ly1; ly++) {
        uint16_t *row = &c->buf[ly * c->w];
        for (int lx = lx0; lx < lx1; lx++) {
            row[lx] = color;
        }
    }
}

static void draw_char(canvas_t *c, int x, int y, char ch, int scale, uint16_t color)
{
    if (x + FONT5X7_WIDTH * scale <= c->x0 || x >= c->x0 + c->w ||
        y + FONT5X7_HEIGHT * scale <= c->y0 || y >= c->y0 + c->h) {
        return;   /* glyph is entirely outside this band */
    }
    if (ch < FONT5X7_FIRST_CHAR || ch > FONT5X7_LAST_CHAR) {
        ch = '?';
    }
    const uint8_t *glyph = &font5x7[(ch - FONT5X7_FIRST_CHAR) * FONT5X7_WIDTH];

    for (int col = 0; col < FONT5X7_WIDTH; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < FONT5X7_HEIGHT; row++) {
            if (!(bits & (1 << row))) {
                continue;
            }
            if (scale == 1) {
                put_pixel(c, x + col, y + row, color);
            } else {
                fill_rect(c, x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static int text_width(const char *s, int scale)
{
    int n = (int)strlen(s);
    return n ? (n * (FONT5X7_WIDTH + 1) - 1) * scale : 0;
}

static void draw_text(canvas_t *c, int x, int y, const char *s, int scale, uint16_t color)
{
    for (; *s; s++) {
        draw_char(c, x, y, *s, scale, color);
        x += (FONT5X7_WIDTH + 1) * scale;
    }
}

static void draw_text_centered(canvas_t *c, int y, const char *s, int scale, uint16_t color)
{
    draw_text(c, (BSP_LCD_H_RES - text_width(s, scale)) / 2, y, s, scale, color);
}

static void fill_circle(canvas_t *c, int cx, int cy, int r, uint16_t color)
{
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) {
                put_pixel(c, cx + dx, cy + dy, color);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Scene composition + flushing                                       */
/* ------------------------------------------------------------------ */

static void compose(canvas_t *c)
{
    fill_rect(c, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, C_BG);

    if (s_mode == SCENE_CALIBRATE) {
        draw_text_centered(c, 148, "CALIBRATION", 2, C_TEXT);
        draw_text_centered(c, 174, "TAP THE CROSS", 1, C_MUTED);
        draw_text_centered(c, 188, s_cal_step, 1, C_ACCENT);

        fill_rect(c, s_target_x - CROSS_ARM, s_target_y - 1, 2 * CROSS_ARM + 1, 3, C_ACCENT);
        fill_rect(c, s_target_x - 1, s_target_y - CROSS_ARM, 3, 2 * CROSS_ARM + 1, C_ACCENT);
        fill_circle(c, s_target_x, s_target_y, 4, C_DOT);
        return;
    }

    draw_text_centered(c, 40, "Hello, World!", 2, C_TEXT);
    fill_rect(c, 36, 66, BSP_LCD_H_RES - 72, 2, C_ACCENT);
    draw_text_centered(c, 80, "ESP32-C6 Touch LCD 1.47", 1, C_MUTED);
    draw_text_centered(c, 96, "JD9853 + AXS5106L", 1, C_MUTED);
    draw_text_centered(c, 130, "Tap the screen", 1, C_ACCENT);

    draw_text(c, COORD_TEXT_X, COORD_TEXT_Y, s_coord_line, COORD_TEXT_SCALE, C_TEXT);

    if (s_has_dot) {
        fill_circle(c, s_dot_x, s_dot_y, DOT_RADIUS + 2, C_DOT_RING);
        fill_circle(c, s_dot_x, s_dot_y, DOT_RADIUS, C_DOT);
    }
}

/* Render one screen rectangle from the scene and push it to the panel. */
static void flush_rect(int x, int y, int w, int h)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > BSP_LCD_H_RES) { w = BSP_LCD_H_RES - x; }
    if (y + h > BSP_LCD_V_RES) { h = BSP_LCD_V_RES - y; }
    if (w <= 0 || h <= 0) {
        return;
    }

    /* Split into bands that fit the staging buffer. */
    int max_rows = (BSP_LCD_H_RES * TILE_ROWS) / w;
    if (max_rows < 1) {
        max_rows = 1;
    }

    for (int band_y = y; band_y < y + h; band_y += max_rows) {
        int band_h = (band_y + max_rows > y + h) ? (y + h - band_y) : max_rows;
        canvas_t c = { .x0 = x, .y0 = band_y, .w = w, .h = band_h, .buf = s_tile };
        compose(&c);
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel, x, band_y,
                                                  x + w, band_y + band_h, s_tile));
        /* draw_bitmap only queues the DMA transfer, so the next band must not
         * start painting over s_tile until the panel has consumed it. */
        xSemaphoreTake(s_flush_done, portMAX_DELAY);
    }
}

static void flush_all(void)
{
    flush_rect(0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES);
}

/* Move the marker, repainting only the two boxes that changed. */
static void move_dot(int x, int y)
{
    int old_x = s_dot_x;
    int old_y = s_dot_y;
    bool had_dot = s_has_dot;

    s_dot_x = x;
    s_dot_y = y;
    s_has_dot = true;

    if (had_dot) {
        flush_rect(old_x - DOT_BOX / 2, old_y - DOT_BOX / 2, DOT_BOX, DOT_BOX);
    }
    flush_rect(x - DOT_BOX / 2, y - DOT_BOX / 2, DOT_BOX, DOT_BOX);
}

static void set_coord_line(const char *text)
{
    if (strcmp(s_coord_line, text) == 0) {
        return;
    }
    snprintf(s_coord_line, sizeof(s_coord_line), "%s", text);
    flush_rect(0, COORD_TEXT_Y - 2, BSP_LCD_H_RES, COORD_BOX_H);
}

/* ------------------------------------------------------------------ */
/*  Calibration flow                                                   */
/* ------------------------------------------------------------------ */

/* Wait for one complete press and return its averaged raw position. */
static void cal_capture(axs5106l_handle_t tp, uint16_t *raw_x, uint16_t *raw_y)
{
    axs5106l_data_t t;

    /* First make sure the finger from the previous target is gone. */
    for (int idle = 0; idle < 5; ) {
        idle = (axs5106l_read(tp, &t) == ESP_OK && !t.pressed) ? idle + 1 : 0;
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    uint32_t sum_x = 0, sum_y = 0, samples = 0;
    while (true) {
        if (axs5106l_read(tp, &t) == ESP_OK) {
            if (t.pressed) {
                sum_x += t.raw_x;
                sum_y += t.raw_y;
                samples++;
            } else if (samples >= 3) {
                break;                  /* released after a real press */
            } else {
                sum_x = sum_y = samples = 0;   /* too short, treat as noise */
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    *raw_x = (uint16_t)(sum_x / samples);
    *raw_y = (uint16_t)(sum_y / samples);
}

/* Three targets: one origin, one displaced along screen X only, one along
 * screen Y only. That is exactly enough to tell which raw axis drives which
 * screen axis, in which direction, and over what range. */
static touch_cal_t cal_run(axs5106l_handle_t tp)
{
    static const struct {
        int x, y;
        const char *label;
    } targets[3] = {
        { BSP_LCD_H_RES / 4,     BSP_LCD_V_RES / 4,     "1 of 3" },
        { BSP_LCD_H_RES * 3 / 4, BSP_LCD_V_RES / 4,     "2 of 3" },
        { BSP_LCD_H_RES / 4,     BSP_LCD_V_RES * 3 / 4, "3 of 3" },
    };

    touch_cal_t cal = {0};
    s_mode = SCENE_CALIBRATE;

    while (true) {
        uint16_t rx[3], ry[3];

        for (int i = 0; i < 3; i++) {
            s_target_x = targets[i].x;
            s_target_y = targets[i].y;
            snprintf(s_cal_step, sizeof(s_cal_step), "%s", targets[i].label);
            flush_all();

            cal_capture(tp, &rx[i], &ry[i]);
            ESP_LOGI(TAG, "calibration %s: screen (%3d,%3d) -> raw (%u,%u)",
                     targets[i].label, targets[i].x, targets[i].y, rx[i], ry[i]);
        }

        /* Target 1 -> 2 moved along screen X only, so whichever raw axis moved
         * is the one feeding screen X. */
        cal.swap = abs((int)ry[1] - (int)ry[0]) > abs((int)rx[1] - (int)rx[0]);

        if (cal.swap) {
            cal.x_lo = ry[0]; cal.x_hi = ry[1];
            cal.y_lo = rx[0]; cal.y_hi = rx[2];
        } else {
            cal.x_lo = rx[0]; cal.x_hi = rx[1];
            cal.y_lo = ry[0]; cal.y_hi = ry[2];
        }

        if (abs(cal.x_hi - cal.x_lo) >= 8 && abs(cal.y_hi - cal.y_lo) >= 8) {
            break;
        }
        ESP_LOGW(TAG, "calibration taps too close together, starting over");
    }

    s_mode = SCENE_RUN;
    ESP_LOGI(TAG, "calibration: swap=%u x:[%d..%d] y:[%d..%d]",
             cal.swap, cal.x_lo, cal.x_hi, cal.y_lo, cal.y_hi);
    return cal;
}

/* ------------------------------------------------------------------ */
/*  Hardware bring-up                                                  */
/* ------------------------------------------------------------------ */

/* GPIO23 drives the base of an NPN through 1k, so a high level lights the
 * backlight. The pin also has the SoC's internal weak pull-up at reset, which
 * is enough to bias that transistor on: from power-on until this runs, the
 * panel is lit and showing whatever its RAM held. So claim the pin at 0 %
 * before touching the panel, and raise the duty only after the first flush. */
static void backlight_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel = {
        .gpio_num = BSP_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

static void backlight_set(uint8_t percent)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0,
                                  percent * 255 / 100));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
}

static bool IRAM_ATTR on_color_trans_done(esp_lcd_panel_io_handle_t io,
                                          esp_lcd_panel_io_event_data_t *edata,
                                          void *user_ctx)
{
    BaseType_t higher_prio_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_flush_done, &higher_prio_woken);
    return higher_prio_woken == pdTRUE;
}

static void display_init(void)
{
    /* The TF card shares SCLK/MOSI with the panel — park its CS high. */
    gpio_config_t sd_cs = {
        .pin_bit_mask = 1ULL << BSP_SD_CS,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&sd_cs));
    gpio_set_level(BSP_SD_CS, 1);

    spi_bus_config_t bus = {
        .sclk_io_num = BSP_LCD_SCLK,
        .mosi_io_num = BSP_LCD_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BSP_LCD_H_RES * TILE_ROWS * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(BSP_LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

    s_flush_done = xSemaphoreCreateBinary();
    ESP_ERROR_CHECK(s_flush_done ? ESP_OK : ESP_ERR_NO_MEM);

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = BSP_LCD_CS,
        .dc_gpio_num = BSP_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = BSP_LCD_PIXEL_CLK,
        .trans_queue_depth = 1,
        .on_color_trans_done = on_color_trans_done,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_HOST,
                                             &io_cfg, &io));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BSP_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_jd9853(io, &panel_cfg, &s_panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    /* The visible 172 columns start at column 34 of the controller's RAM. */
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_panel, JD9853_LCD_X_GAP, JD9853_LCD_Y_GAP));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    s_tile = heap_caps_malloc(BSP_LCD_H_RES * TILE_ROWS * sizeof(uint16_t), MALLOC_CAP_DMA);
    ESP_ERROR_CHECK(s_tile ? ESP_OK : ESP_ERR_NO_MEM);
}

static i2c_master_bus_handle_t i2c_init(void)
{
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
    return bus;
}

static void nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static bool boot_button_held(void)
{
    gpio_config_t btn = {
        .pin_bit_mask = 1ULL << BSP_BOOT_BTN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&btn));
    return gpio_get_level(BSP_BOOT_BTN) == 0;
}

/* ------------------------------------------------------------------ */
/*  Application                                                        */
/* ------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "Hello World from ESP32-C6-Touch-LCD-1.47");
    printf("Hello World\n");

    backlight_init();          /* 0 % duty first -- see backlight_init() */
    nvs_init();
    display_init();
    flush_all();
    backlight_set(90);
    ESP_LOGI(TAG, "display ready: %dx%d", BSP_LCD_H_RES, BSP_LCD_V_RES);

    i2c_master_bus_handle_t i2c_bus = i2c_init();

    /* The driver's own transform stays at identity — the calibration below
     * works straight from the controller's raw coordinates. */
    axs5106l_config_t tp_cfg = {
        .i2c_bus = i2c_bus,
        .rst_gpio = BSP_TP_RST,
        .int_gpio = BSP_TP_INT,
        .width = BSP_LCD_H_RES,
        .height = BSP_LCD_V_RES,
    };
    axs5106l_handle_t tp = NULL;
    esp_err_t err = axs5106l_new(&tp_cfg, &tp);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "touch init failed: %s — display only", esp_err_to_name(err));
        set_coord_line("NO TOUCH IC");
        vTaskDelay(portMAX_DELAY);
    }

    uint16_t fw_ver = 0;
    uint32_t chip_id = 0;
    if (axs5106l_read_info(tp, &fw_ver, &chip_id) == ESP_OK) {
        ESP_LOGI(TAG, "AXS5106L ready: chip ID 0x%06X, firmware V%u",
                 (unsigned)chip_id, (unsigned)fw_ver);
    }

    /* The stock mapping is known and correct for this panel, so nothing is
     * asked of the user on a fresh board. Calibration is only there to rescue
     * a panel that behaves differently: hold BOOT while resetting. */
    if (boot_button_held()) {
        ESP_LOGI(TAG, "BOOT held — running touch calibration");
        s_cal = cal_run(tp);
        cal_save(&s_cal);
    } else if (cal_load(&s_cal)) {
        ESP_LOGI(TAG, "using stored calibration: swap=%u x:[%d..%d] y:[%d..%d]",
                 s_cal.swap, s_cal.x_lo, s_cal.x_hi, s_cal.y_lo, s_cal.y_hi);
    } else {
        s_cal = cal_default();
        ESP_LOGI(TAG, "using the panel's stock touch mapping");
    }

    flush_all();
    ESP_LOGI(TAG, "touch the screen — coordinates are printed here");

    bool was_pressed = false;
    int last_x = -1, last_y = -1;

    while (true) {
        axs5106l_data_t t;
        if (axs5106l_read(tp, &t) == ESP_OK) {
            if (t.pressed) {
                int x, y;
                cal_apply(&s_cal, t.raw_x, t.raw_y, &x, &y);

                if (!was_pressed || x != last_x || y != last_y) {
                    ESP_LOGI(TAG, "%s  x=%3d  y=%3d   (raw x=%u y=%u)",
                             was_pressed ? "move " : "press", x, y, t.raw_x, t.raw_y);

                    char line[sizeof(s_coord_line)];
                    snprintf(line, sizeof(line), "X:%3d Y:%3d", x, y);
                    set_coord_line(line);
                    move_dot(x, y);

                    last_x = x;
                    last_y = y;
                }
                was_pressed = true;
            } else {
                if (was_pressed) {
                    ESP_LOGI(TAG, "release  x=%3d  y=%3d", last_x, last_y);
                }
                was_pressed = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
