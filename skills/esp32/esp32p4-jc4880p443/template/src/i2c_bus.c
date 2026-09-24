/*
 * JC4880P443 — the one I2C bus (SDA 7, SCL 8).  ES8311, GT911, camera SCCB,
 * CN3 and JP1 23/25 all share it: open it once and pass the handle around.
 */

#include <stdio.h>

#include "app.h"
#include "board_pins.h"

static i2c_master_bus_handle_t s_bus;

esp_err_t i2c_bus_init(void)
{
    if (s_bus) return ESP_OK;
    const i2c_master_bus_config_t cfg = {
        .i2c_port = 0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        /* ~1.5K external pull-ups are fitted; the driver's "check pull-up"
         * warning that follows is expected and harmless. */
        .flags.enable_internal_pullup = false,
    };
    return i2c_new_master_bus(&cfg, &s_bus);
}

i2c_master_bus_handle_t i2c_bus_handle(void) { return s_bus; }

static const char *name_of(int a)
{
    switch (a) {
    case I2C_ADDR_ES8311: return "ES8311 codec";
    case I2C_ADDR_GT911: return "GT911 touch";
    case I2C_ADDR_GT911_ALT: return "GT911 touch (alt addr)";
    case I2C_ADDR_OV02C10: return "OV02C10 camera";
    default: return "?";
    }
}

void i2c_bus_scan_print(void)
{
    printf("I2C      SDA %d / SCL %d:", PIN_I2C_SDA, PIN_I2C_SCL);
    int n = 0;
    for (int a = 0x08; a < 0x78; a++) {
        if (i2c_master_probe(s_bus, a, 20) == ESP_OK) {
            printf(" 0x%02X(%s)", a, name_of(a));
            n++;
        }
    }
    printf("%s\n", n ? "" : " nothing — check GPIO7/8");
}

/* ES8311 chip ID: registers 0xFD/0xFE read 0x83/0x11.  Proves the codec is
 * powered and addressed without needing a speaker. */
esp_err_t es8311_read_id(uint8_t *id1, uint8_t *id2)
{
    i2c_master_dev_handle_t dev;
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_ADDR_ES8311,
        .scl_speed_hz = 100000,
    };
    esp_err_t err = i2c_master_bus_add_device(s_bus, &cfg, &dev);
    if (err != ESP_OK) return err;
    uint8_t reg = 0xFD;
    err = i2c_master_transmit_receive(dev, &reg, 1, id1, 1, 50);
    reg = 0xFE;
    if (err == ESP_OK) err = i2c_master_transmit_receive(dev, &reg, 1, id2, 1, 50);
    i2c_master_bus_rm_device(dev);
    return err;
}
