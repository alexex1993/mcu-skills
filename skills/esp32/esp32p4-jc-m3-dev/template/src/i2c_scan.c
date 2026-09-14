/*
 * Probe the one shared I2C bus (GPIO7 SDA / GPIO8 SCL).
 *
 * Everything on this board that speaks I2C sits here: the ES8311 codec at 0x18,
 * the DSI panel's GT911 touch controller at 0x5D or 0x14, that panel's
 * brightness register at 0x45, the CSI camera's SCCB, and whatever is plugged
 * into CN3 or JP1 pins 23/25.  A device you add must avoid those addresses.
 *
 * The bus already has 5.1K pull-ups to 3V3 on board (R65/R71), so the internal
 * ones are left off here.
 */

#include "app.h"
#include "board_pins.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "i2c";

static const char *known_device(uint8_t addr)
{
    switch (addr) {
    case I2C_ADDR_ES8311:   return "ES8311 audio codec";
    case I2C_ADDR_GT911_B:  return "GT911 touch (alternate address)";
    case I2C_ADDR_PANEL_BL: return "10.1\" DSI panel brightness register";
    case I2C_ADDR_GT911_A:  return "GT911 touch (primary address)";
    default:                return NULL;
    }
}

void i2c_scan_print(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {.enable_internal_pullup = false},
    };

    i2c_master_bus_handle_t bus = NULL;
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &bus);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        return;
    }

    printf("I2C      scanning SDA=%d SCL=%d\n", PIN_I2C_SDA, PIN_I2C_SCL);

    int found = 0;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(bus, addr, 50) != ESP_OK) {
            continue;
        }
        found++;
        const char *name = known_device(addr);
        printf("         0x%02x  %s\n", addr, name ? name : "(unknown)");
    }
    if (found == 0) {
        printf("         nothing answered — with no panel and no camera attached "
               "only the ES8311 at 0x18 is expected\n");
    }

    i2c_del_master_bus(bus);
}
