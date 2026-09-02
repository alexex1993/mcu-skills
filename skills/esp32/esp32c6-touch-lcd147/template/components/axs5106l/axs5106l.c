/*
 * Minimal polling driver for the AXS5106L capacitive touch controller.
 *
 * Register map and framing follow the Waveshare AXS5106L Arduino library for
 * the ESP32-C6-Touch-LCD-1.47:
 *   0x01  touch frame: gesture, points, then 6 bytes per contact
 *         (XH, XL, YH, YL, pressure, reserved). The whole 14-byte frame has
 *         to be read in one go — a short read leaves the chip misaligned and
 *         the following frames come back as garbage.
 *   0x05  firmware version, 2 bytes big-endian
 *   0x08  chip ID, 3 bytes
 */

#include "axs5106l.h"

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "axs5106l";

#define AXS5106L_I2C_ADDR    0x63
#define AXS5106L_REG_DATA    0x01
#define AXS5106L_REG_FW_VER  0x05
#define AXS5106L_REG_CHIP_ID 0x08

/* 2 header bytes + 2 contacts worth of 6-byte records. */
#define AXS5106L_FRAME_LEN   14

#define AXS5106L_I2C_HZ      400000
#define AXS5106L_I2C_TMO_MS  100
#define AXS5106L_I2C_RETRIES 3

struct axs5106l_t {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t dev;
    gpio_num_t rst_gpio;
    gpio_num_t int_gpio;
    uint16_t width;
    uint16_t height;
    uint16_t raw_max_x;
    uint16_t raw_max_y;
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
    uint8_t err_streak;    /*!< consecutive failed reads, for bus recovery */
    uint32_t reset_count;  /*!< bus resets so far, only to throttle the warning */
};

/* Two separate transactions on purpose: the AXS5106L does not answer a
 * combined write-then-read with a repeated START, only a write, a STOP and
 * then a fresh read. */
static bool axs_read_reg(axs5106l_handle_t h, uint8_t reg, uint8_t *data, size_t len)
{
    for (int i = 0; i < AXS5106L_I2C_RETRIES; i++) {
        if (i2c_master_transmit(h->dev, &reg, 1, AXS5106L_I2C_TMO_MS) == ESP_OK &&
            i2c_master_receive(h->dev, data, len, AXS5106L_I2C_TMO_MS) == ESP_OK) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return false;
}

/* The vendor library holds RST low for 200 ms and waits another 300 ms after
 * releasing it. Shorter pulses appear to work but leave the controller
 * reporting nonsense coordinates. */
static void axs_reset(axs5106l_handle_t h)
{
    if (h->rst_gpio < 0) {
        vTaskDelay(pdMS_TO_TICKS(300));
        return;
    }
    gpio_set_level(h->rst_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(h->rst_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(300));
}

/* Diagnostics for when the controller stays silent: list whatever does answer,
 * so a dead bus can be told apart from a wrong address. */
static void axs_scan_bus(axs5106l_handle_t h)
{
    ESP_LOGW(TAG, "scanning I2C bus, TP_INT=%d TP_RST=%d",
             h->int_gpio >= 0 ? gpio_get_level(h->int_gpio) : -1,
             h->rst_gpio >= 0 ? gpio_get_level(h->rst_gpio) : -1);

    int found = 0;
    for (uint16_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(h->bus, addr, 50) == ESP_OK) {
            ESP_LOGW(TAG, "  device answered at 0x%02X", (unsigned)addr);
            found++;
        }
    }
    if (found == 0) {
        ESP_LOGE(TAG, "  bus is silent - check SDA/SCL wiring and pull-ups");
    }
}

esp_err_t axs5106l_new(const axs5106l_config_t *cfg, axs5106l_handle_t *out)
{
    ESP_RETURN_ON_FALSE(cfg && out && cfg->i2c_bus, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(cfg->width && cfg->height, ESP_ERR_INVALID_ARG, TAG, "zero screen size");

    axs5106l_handle_t h = calloc(1, sizeof(struct axs5106l_t));
    ESP_RETURN_ON_FALSE(h, ESP_ERR_NO_MEM, TAG, "no mem");

    h->bus = cfg->i2c_bus;
    h->rst_gpio = cfg->rst_gpio;
    h->int_gpio = cfg->int_gpio;
    h->width = cfg->width;
    h->height = cfg->height;
    h->raw_max_x = cfg->raw_max_x ? cfg->raw_max_x : cfg->width;
    h->raw_max_y = cfg->raw_max_y ? cfg->raw_max_y : cfg->height;
    h->swap_xy = cfg->swap_xy;
    h->mirror_x = cfg->mirror_x;
    h->mirror_y = cfg->mirror_y;

    esp_err_t ret = ESP_OK;

    if (h->rst_gpio >= 0) {
        gpio_config_t rst_cfg = {
            .pin_bit_mask = 1ULL << h->rst_gpio,
            .mode = GPIO_MODE_OUTPUT,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&rst_cfg), err, TAG, "config TP_RST failed");
        gpio_set_level(h->rst_gpio, 1);
    }
    if (h->int_gpio >= 0) {
        gpio_config_t int_cfg = {
            .pin_bit_mask = 1ULL << h->int_gpio,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&int_cfg), err, TAG, "config TP_INT failed");
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXS5106L_I2C_ADDR,
        .scl_speed_hz = AXS5106L_I2C_HZ,
    };
    ESP_GOTO_ON_ERROR(i2c_master_bus_add_device(h->bus, &dev_cfg, &h->dev), err, TAG,
                      "add I2C device failed");

    axs_reset(h);

    /* Presence is decided by the address ACK, not by the chip-ID contents:
     * some firmware revisions report an all-zero ID and are perfectly alive. */
    bool alive = false;
    for (int i = 0; i < 5 && !alive; i++) {
        alive = (i2c_master_probe(h->bus, AXS5106L_I2C_ADDR, 50) == ESP_OK);
        if (!alive) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    if (!alive) {
        axs_scan_bus(h);
        ret = ESP_ERR_NOT_FOUND;
        ESP_LOGE(TAG, "no ACK from 0x%02X", AXS5106L_I2C_ADDR);
        goto err_dev;
    }

    *out = h;
    return ESP_OK;

err_dev:
    i2c_master_bus_rm_device(h->dev);
err:
    free(h);
    return ret;
}

esp_err_t axs5106l_read_info(axs5106l_handle_t h, uint16_t *fw_ver, uint32_t *chip_id)
{
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    if (fw_ver) {
        uint8_t v[2] = {0};
        if (!axs_read_reg(h, AXS5106L_REG_FW_VER, v, sizeof(v))) {
            return ESP_ERR_TIMEOUT;
        }
        *fw_ver = ((uint16_t)v[0] << 8) | v[1];
    }
    if (chip_id) {
        uint8_t id[3] = {0};
        if (!axs_read_reg(h, AXS5106L_REG_CHIP_ID, id, sizeof(id))) {
            return ESP_ERR_TIMEOUT;
        }
        *chip_id = ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
    }
    return ESP_OK;
}

esp_err_t axs5106l_read(axs5106l_handle_t h, axs5106l_data_t *data)
{
    ESP_RETURN_ON_FALSE(h && data, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    memset(data, 0, sizeof(*data));

    /* The controller is polled rather than INT-driven: TP_INT is only a hint,
     * and polling also gives us the release edge without an extra path. */
    uint8_t buf[AXS5106L_FRAME_LEN] = {0};
    if (!axs_read_reg(h, AXS5106L_REG_DATA, buf, sizeof(buf))) {
        /* Strong interference can wedge SDA low; 9 SCL pulses free the bus,
         * but only bother once the failures actually pile up. */
        if (++h->err_streak >= 3) {
            i2c_master_bus_reset(h->bus);
            h->err_streak = 0;
            if (++h->reset_count % 16 == 1) {
                ESP_LOGW(TAG, "I2C read keeps failing, bus reset #%u",
                         (unsigned)h->reset_count);
            }
        }
        return ESP_ERR_TIMEOUT;
    }
    h->err_streak = 0;

    data->gesture = buf[0];
    data->points = buf[1];

    uint16_t raw_x = ((uint16_t)(buf[2] & 0x0F) << 8) | buf[3];
    uint16_t raw_y = ((uint16_t)(buf[4] & 0x0F) << 8) | buf[5];
    data->raw_x = raw_x;
    data->raw_y = raw_y;

    if (data->points == 0 || data->points > 5) {
        return ESP_OK;   /* released, or a noise frame */
    }
    if (raw_x == 0x0FFF && raw_y == 0x0FFF) {
        return ESP_OK;   /* all-ones frame the chip emits while settling */
    }

    /* Scale from the controller's coordinate space onto the panel. */
    uint32_t sx = (h->raw_max_x == h->width)
        ? raw_x : (uint32_t)raw_x * h->width / h->raw_max_x;
    uint32_t sy = (h->raw_max_y == h->height)
        ? raw_y : (uint32_t)raw_y * h->height / h->raw_max_y;

    if (h->swap_xy) {
        uint32_t tmp = sx;
        sx = sy;
        sy = tmp;
    }
    if (sx >= h->width) {
        sx = h->width - 1;
    }
    if (sy >= h->height) {
        sy = h->height - 1;
    }
    if (h->mirror_x) {
        sx = h->width - 1 - sx;
    }
    if (h->mirror_y) {
        sy = h->height - 1 - sy;
    }

    data->x = (uint16_t)sx;
    data->y = (uint16_t)sy;
    data->pressed = true;
    return ESP_OK;
}

esp_err_t axs5106l_del(axs5106l_handle_t h)
{
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    if (h->dev) {
        i2c_master_bus_rm_device(h->dev);
    }
    if (h->rst_gpio >= 0) {
        gpio_reset_pin(h->rst_gpio);
    }
    free(h);
    return ESP_OK;
}
