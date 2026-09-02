/*
 * Minimal polling driver for the AXS5106L capacitive touch controller
 * (I2C, address 0x63) as wired on the Waveshare ESP32-C6-Touch-LCD-1.47.
 *
 * No LVGL, no framework: read one point, get pixel coordinates.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct axs5106l_t *axs5106l_handle_t;

typedef struct {
    i2c_master_bus_handle_t i2c_bus;  /*!< Already initialised I2C master bus */
    gpio_num_t rst_gpio;              /*!< TP_RST, active low (-1 to skip) */
    gpio_num_t int_gpio;              /*!< TP_INT, active low (-1 to skip) */
    uint16_t width;                   /*!< Screen width in pixels */
    uint16_t height;                  /*!< Screen height in pixels */
    uint16_t raw_max_x;               /*!< Raw X full-scale, 0 = same as width */
    uint16_t raw_max_y;               /*!< Raw Y full-scale, 0 = same as height */
    bool swap_xy;                     /*!< Swap axes before mirroring */
    bool mirror_x;
    bool mirror_y;
} axs5106l_config_t;

/** @brief One touch sample. */
typedef struct {
    bool pressed;       /*!< True while a finger is on the panel */
    uint16_t x;         /*!< Screen X, 0..width-1  (valid when pressed) */
    uint16_t y;         /*!< Screen Y, 0..height-1 (valid when pressed) */
    uint16_t raw_x;     /*!< Controller X before mapping — useful for calibration */
    uint16_t raw_y;     /*!< Controller Y before mapping */
    uint8_t gesture;    /*!< Gesture byte reported by the controller */
    uint8_t points;     /*!< Number of contacts reported */
} axs5106l_data_t;

/**
 * @brief Reset the controller, probe it and prepare it for polling.
 */
esp_err_t axs5106l_new(const axs5106l_config_t *cfg, axs5106l_handle_t *out);

/**
 * @brief Read the current contact.
 *
 * @return ESP_OK on a successful I2C exchange (check data->pressed),
 *         ESP_ERR_TIMEOUT if the controller did not answer.
 */
esp_err_t axs5106l_read(axs5106l_handle_t handle, axs5106l_data_t *data);

/** @brief Firmware version and chip ID, for the boot-time log. */
esp_err_t axs5106l_read_info(axs5106l_handle_t handle, uint16_t *fw_ver, uint32_t *chip_id);

/** @brief Release the handle and its I2C device. */
esp_err_t axs5106l_del(axs5106l_handle_t handle);

#ifdef __cplusplus
}
#endif
