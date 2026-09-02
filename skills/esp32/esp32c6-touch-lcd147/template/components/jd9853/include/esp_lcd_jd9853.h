/*
 * esp_lcd panel driver for the JD9853 (Jadard) TFT controller.
 *
 * Default init sequence targets the 1.47" 172x320 IPS panel used on the
 * Waveshare ESP32-C6-Touch-LCD-1.47.  The visible area sits at column
 * offset 34 of the controller's 240-column RAM, so the caller must set
 * esp_lcd_panel_set_gap(panel, 34, 0).
 */

#pragma once

#include "esp_lcd_panel_vendor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JD9853_LCD_H_RES    172     /*!< Horizontal resolution (visible) */
#define JD9853_LCD_V_RES    320     /*!< Vertical resolution   (visible) */
#define JD9853_LCD_X_GAP    34      /*!< Column offset into controller RAM */
#define JD9853_LCD_Y_GAP    0       /*!< Row offset into controller RAM */

/** @brief One entry of a panel initialisation sequence. */
typedef struct {
    int cmd;                /*!< LCD command */
    const void *data;       /*!< Command payload, NULL if the command takes none */
    size_t data_bytes;      /*!< Payload size in bytes */
    unsigned int delay_ms;  /*!< Delay applied after the command */
} jd9853_lcd_init_cmd_t;

/**
 * @brief Vendor configuration, passed through esp_lcd_panel_dev_config_t::vendor_config.
 *
 * Leave init_cmds NULL to use the built-in Waveshare 172x320 sequence.
 */
typedef struct {
    const jd9853_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
} jd9853_vendor_config_t;

/**
 * @brief Create an esp_lcd panel handle for a JD9853.
 *
 * @param[in]  io               Panel IO handle (4-wire SPI)
 * @param[in]  panel_dev_config Panel device configuration
 * @param[out] ret_panel        Returned panel handle
 */
esp_err_t esp_lcd_new_panel_jd9853(const esp_lcd_panel_io_handle_t io,
                                   const esp_lcd_panel_dev_config_t *panel_dev_config,
                                   esp_lcd_panel_handle_t *ret_panel);

#ifdef __cplusplus
}
#endif
