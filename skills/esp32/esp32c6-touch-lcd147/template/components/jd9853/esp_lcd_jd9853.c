/*
 * esp_lcd panel driver for the JD9853 (Jadard) TFT controller.
 *
 * The built-in initialisation sequence is the one Waveshare ships for the
 * 1.47" 172x320 IPS panel of the ESP32-C6-Touch-LCD-1.47.
 */

#include <stdlib.h>
#include <sys/cdefs.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_lcd_jd9853.h"

static const char *TAG = "jd9853";

static esp_err_t panel_jd9853_del(esp_lcd_panel_t *panel);
static esp_err_t panel_jd9853_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_jd9853_init(esp_lcd_panel_t *panel);
static esp_err_t panel_jd9853_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start,
                                          int x_end, int y_end, const void *color_data);
static esp_err_t panel_jd9853_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_jd9853_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_jd9853_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_jd9853_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_jd9853_disp_on_off(esp_lcd_panel_t *panel, bool on_off);

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val;
    uint8_t colmod_val;
    const jd9853_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
} jd9853_panel_t;

/* Waveshare ESP32-C6-Touch-LCD-1.47 (JD9853, 1.47" 172x320 IPS).
 * 0xDE selects the register page; 0xDF unlocks the vendor registers. */
static const jd9853_lcd_init_cmd_t vendor_specific_init_default[] = {
    {LCD_CMD_SLPOUT, NULL, 0, 120},

    /* Unlock vendor registers, page 0 */
    {0xDF, (uint8_t[]){0x98, 0x53}, 2, 0},
    {0xDE, (uint8_t[]){0x00}, 1, 0},

    /* Power / timing */
    {0xB2, (uint8_t[]){0x23}, 1, 0},
    {0xB7, (uint8_t[]){0x00, 0x47, 0x00, 0x6F}, 4, 0},
    {0xBB, (uint8_t[]){0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0}, 6, 0},
    {0xC0, (uint8_t[]){0x44, 0xA4}, 2, 0},
    {0xC1, (uint8_t[]){0x16}, 1, 0},
    {0xC3, (uint8_t[]){0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77}, 8, 0},
    {0xC4, (uint8_t[]){0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16,
                       0x79, 0x0B, 0x0A, 0x16, 0x82}, 12, 0},

    /* Gamma */
    {0xC8, (uint8_t[]){
        0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
        0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
        0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
        0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00
    }, 32, 0},

    {0xD0, (uint8_t[]){0x04, 0x06, 0x6B, 0x0F, 0x00}, 5, 0},
    {0xD7, (uint8_t[]){0x00, 0x30}, 2, 0},
    {0xE6, (uint8_t[]){0x14}, 1, 0},

    /* Page 1 */
    {0xDE, (uint8_t[]){0x01}, 1, 0},
    {0xB7, (uint8_t[]){0x03, 0x13, 0xEF, 0x35, 0x35}, 5, 0},
    {0xC1, (uint8_t[]){0x14, 0x15, 0xC0}, 3, 0},
    {0xC2, (uint8_t[]){0x06, 0x3A}, 2, 0},
    {0xC4, (uint8_t[]){0x72, 0x12}, 2, 0},
    {0xBE, (uint8_t[]){0x00}, 1, 0},

    /* Page 2 */
    {0xDE, (uint8_t[]){0x02}, 1, 0},
    {0xE5, (uint8_t[]){0x00, 0x02, 0x00}, 3, 0},
    {0xE5, (uint8_t[]){0x01, 0x02, 0x00}, 3, 0},

    /* Back to page 0 */
    {0xDE, (uint8_t[]){0x00}, 1, 0},
    {LCD_CMD_TEON, (uint8_t[]){0x00}, 1, 0},
    {LCD_CMD_COLMOD, (uint8_t[]){0x05}, 1, 0},                      /* RGB565 */
    {LCD_CMD_CASET, (uint8_t[]){0x00, 0x22, 0x00, 0xCD}, 4, 0},     /* col 34..205 */
    {LCD_CMD_RASET, (uint8_t[]){0x00, 0x00, 0x01, 0x3F}, 4, 0},     /* row  0..319 */

    {0xDE, (uint8_t[]){0x02}, 1, 0},
    {0xE5, (uint8_t[]){0x00, 0x02, 0x00}, 3, 0},
    {0xDE, (uint8_t[]){0x00}, 1, 0},

    {LCD_CMD_MADCTL, (uint8_t[]){0x00}, 1, 0},
    {LCD_CMD_INVON, NULL, 0, 10},   /* this panel is inverted */
};

esp_err_t esp_lcd_new_panel_jd9853(const esp_lcd_panel_io_handle_t io,
                                   const esp_lcd_panel_dev_config_t *panel_dev_config,
                                   esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    jd9853_panel_t *jd9853 = NULL;

    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG,
                      "invalid argument");

    jd9853 = (jd9853_panel_t *)calloc(1, sizeof(jd9853_panel_t));
    ESP_GOTO_ON_FALSE(jd9853, ESP_ERR_NO_MEM, err, TAG, "no mem for jd9853 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_ele_order) {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        jd9853->madctl_val = 0;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        jd9853->madctl_val = LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported rgb element order");
        break;
    }

    switch (panel_dev_config->bits_per_pixel) {
    case 16:
        jd9853->colmod_val = 0x55;
        jd9853->fb_bits_per_pixel = 16;
        break;
    case 18:
        jd9853->colmod_val = 0x66;
        jd9853->fb_bits_per_pixel = 24;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    jd9853->io = io;
    jd9853->reset_gpio_num = panel_dev_config->reset_gpio_num;
    jd9853->reset_level = panel_dev_config->flags.reset_active_high;

    if (panel_dev_config->vendor_config) {
        const jd9853_vendor_config_t *vendor =
            (const jd9853_vendor_config_t *)panel_dev_config->vendor_config;
        jd9853->init_cmds = vendor->init_cmds;
        jd9853->init_cmds_size = vendor->init_cmds_size;
    }

    jd9853->base.del = panel_jd9853_del;
    jd9853->base.reset = panel_jd9853_reset;
    jd9853->base.init = panel_jd9853_init;
    jd9853->base.draw_bitmap = panel_jd9853_draw_bitmap;
    jd9853->base.invert_color = panel_jd9853_invert_color;
    jd9853->base.set_gap = panel_jd9853_set_gap;
    jd9853->base.mirror = panel_jd9853_mirror;
    jd9853->base.swap_xy = panel_jd9853_swap_xy;
    jd9853->base.disp_on_off = panel_jd9853_disp_on_off;

    *ret_panel = &(jd9853->base);
    return ESP_OK;

err:
    if (jd9853) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(jd9853);
    }
    return ret;
}

static esp_err_t panel_jd9853_del(esp_lcd_panel_t *panel)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);

    if (jd9853->reset_gpio_num >= 0) {
        gpio_reset_pin(jd9853->reset_gpio_num);
    }
    free(jd9853);
    return ESP_OK;
}

static esp_err_t panel_jd9853_reset(esp_lcd_panel_t *panel)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);

    if (jd9853->reset_gpio_num >= 0) {
        gpio_set_level(jd9853->reset_gpio_num, jd9853->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(jd9853->reset_gpio_num, !jd9853->reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(jd9853->io, LCD_CMD_SWRESET, NULL, 0),
                            TAG, "SWRESET failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    return ESP_OK;
}

static esp_err_t panel_jd9853_init(esp_lcd_panel_t *panel)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);

    /* The vendor sequence already contains SLPOUT, COLMOD and MADCTL, so no
     * separate calls here — sending them twice upsets the register paging. */
    const jd9853_lcd_init_cmd_t *init_cmds = jd9853->init_cmds ?: vendor_specific_init_default;
    uint16_t init_cmds_size = jd9853->init_cmds
        ? jd9853->init_cmds_size
        : sizeof(vendor_specific_init_default) / sizeof(vendor_specific_init_default[0]);

    for (int i = 0; i < init_cmds_size; i++) {
        /* Remember MADCTL/COLMOD so later mirror()/swap_xy() build on them. */
        if (init_cmds[i].cmd == LCD_CMD_MADCTL && init_cmds[i].data) {
            jd9853->madctl_val = ((const uint8_t *)init_cmds[i].data)[0];
        } else if (init_cmds[i].cmd == LCD_CMD_COLMOD && init_cmds[i].data) {
            jd9853->colmod_val = ((const uint8_t *)init_cmds[i].data)[0];
        }

        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(jd9853->io, init_cmds[i].cmd,
                                                      init_cmds[i].data, init_cmds[i].data_bytes),
                            TAG, "send init command 0x%02X failed", init_cmds[i].cmd);
        if (init_cmds[i].delay_ms) {
            vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
        }
    }

    ESP_LOGI(TAG, "panel initialized (%d commands)", init_cmds_size);
    return ESP_OK;
}

static esp_err_t panel_jd9853_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start,
                                          int x_end, int y_end, const void *color_data)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);
    esp_lcd_panel_io_handle_t io = jd9853->io;

    assert((x_start < x_end) && (y_start < y_end) && "invalid coordinates");

    x_start += jd9853->x_gap;
    x_end += jd9853->x_gap;
    y_start += jd9853->y_gap;
    y_end += jd9853->y_gap;

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_CASET, (uint8_t[]){
        (x_start >> 8) & 0xFF, x_start & 0xFF,
        ((x_end - 1) >> 8) & 0xFF, (x_end - 1) & 0xFF,
    }, 4), TAG, "CASET failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_RASET, (uint8_t[]){
        (y_start >> 8) & 0xFF, y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF, (y_end - 1) & 0xFF,
    }, 4), TAG, "RASET failed");

    size_t len = (size_t)(x_end - x_start) * (y_end - y_start) * jd9853->fb_bits_per_pixel / 8;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len),
                        TAG, "RAMWR failed");
    return ESP_OK;
}

static esp_err_t panel_jd9853_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);
    int command = invert_color_data ? LCD_CMD_INVON : LCD_CMD_INVOFF;
    return esp_lcd_panel_io_tx_param(jd9853->io, command, NULL, 0);
}

static esp_err_t panel_jd9853_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);

    if (mirror_x) {
        jd9853->madctl_val |= LCD_CMD_MX_BIT;
    } else {
        jd9853->madctl_val &= ~LCD_CMD_MX_BIT;
    }
    if (mirror_y) {
        jd9853->madctl_val |= LCD_CMD_MY_BIT;
    } else {
        jd9853->madctl_val &= ~LCD_CMD_MY_BIT;
    }
    return esp_lcd_panel_io_tx_param(jd9853->io, LCD_CMD_MADCTL,
                                     (uint8_t[]){jd9853->madctl_val}, 1);
}

static esp_err_t panel_jd9853_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);

    if (swap_axes) {
        jd9853->madctl_val |= LCD_CMD_MV_BIT;
    } else {
        jd9853->madctl_val &= ~LCD_CMD_MV_BIT;
    }
    return esp_lcd_panel_io_tx_param(jd9853->io, LCD_CMD_MADCTL,
                                     (uint8_t[]){jd9853->madctl_val}, 1);
}

static esp_err_t panel_jd9853_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);
    jd9853->x_gap = x_gap;
    jd9853->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_jd9853_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);
    int command = on_off ? LCD_CMD_DISPON : LCD_CMD_DISPOFF;
    return esp_lcd_panel_io_tx_param(jd9853->io, command, NULL, 0);
}
