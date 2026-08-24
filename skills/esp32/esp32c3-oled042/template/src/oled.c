#include "oled.h"

#include <string.h>

#include "board.h"
#include "driver/i2c_master.h"
#include "font5x7.h"

_Static_assert(OLED_CHAR_W == FONT5X7_W && OLED_CHAR_H == FONT5X7_H,
               "oled.h text metrics do not match the generated font");

static uint8_t framebuf[BOARD_OLED_W * BOARD_OLED_PAGES];
static i2c_master_dev_handle_t oled_dev;

/* Control byte 0x00 = "the bytes that follow are commands",
 *              0x40 = "the bytes that follow are display data". */
static esp_err_t oled_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};
    return i2c_master_transmit(oled_dev, buf, sizeof(buf), 100);
}

static esp_err_t oled_cmds(const uint8_t *cmds, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        esp_err_t err = oled_cmd(cmds[i]);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

/* Wipes all eight pages across the full 128 columns. Only 72x40 of the
 * GDDRAM is bonded to glass, but the unbonded margin still holds random
 * bits after power-on and they bleed into the visible edge. Once, at init. */
static esp_err_t oled_clear_gddram(void)
{
    uint8_t tx[1 + 128];
    memset(tx, 0, sizeof(tx));
    tx[0] = 0x40;

    for (int page = 0; page < 8; page++) {
        const uint8_t set_pos[] = {
            (uint8_t)(0xB0 | page), /* page address */
            0x00,                   /* column 0, low nibble */
            0x10,                   /* column 0, high nibble */
        };
        esp_err_t err = oled_cmds(set_pos, sizeof(set_pos));
        if (err != ESP_OK) {
            return err;
        }
        err = i2c_master_transmit(oled_dev, tx, sizeof(tx), 200);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t oled_flush(void)
{
    /* The page address and start column are re-sent for every page, so a
     * failed transfer costs one band rather than shifting the whole frame. */
    for (int page = 0; page < BOARD_OLED_PAGES; page++) {
        const uint8_t set_pos[] = {
            (uint8_t)(0xB0 | page),
            (uint8_t)(0x00 | (BOARD_OLED_COL_OFFSET & 0x0F)),
            (uint8_t)(0x10 | (BOARD_OLED_COL_OFFSET >> 4)),
        };
        esp_err_t err = oled_cmds(set_pos, sizeof(set_pos));
        if (err != ESP_OK) {
            return err;
        }

        uint8_t tx[1 + BOARD_OLED_W];
        tx[0] = 0x40;
        memcpy(&tx[1], &framebuf[page * BOARD_OLED_W], BOARD_OLED_W);
        err = i2c_master_transmit(oled_dev, tx, sizeof(tx), 200);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t oled_set_contrast(uint8_t contrast)
{
    esp_err_t err = oled_cmd(0x81);
    return err == ESP_OK ? oled_cmd(contrast) : err;
}

void oled_clear(void)
{
    memset(framebuf, 0, sizeof(framebuf));
}

void oled_set_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= BOARD_OLED_W || y < 0 || y >= BOARD_OLED_H) {
        return;
    }
    uint8_t *cell = &framebuf[(y / 8) * BOARD_OLED_W + x];
    uint8_t mask = (uint8_t)(1u << (y % 8));
    if (on) {
        *cell |= mask;
    } else {
        *cell &= (uint8_t)~mask;
    }
}

void oled_draw_char(int x, int y, char c)
{
    unsigned char uc = (unsigned char)c;
    if (uc < FONT5X7_FIRST || uc > FONT5X7_LAST) {
        uc = '?';
    }
    const uint8_t *glyph = font5x7[uc - FONT5X7_FIRST];

    for (int col = 0; col < OLED_CHAR_W; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < OLED_CHAR_H; row++) {
            if (bits & (1u << row)) {
                oled_set_pixel(x + col, y + row, true);
            }
        }
    }
}

void oled_draw_text(int x, int y, const char *s)
{
    while (*s) {
        oled_draw_char(x, y, *s++);
        x += OLED_CHAR_STEP;
    }
}

int oled_text_width(const char *s)
{
    int len = (int)strlen(s);
    return len > 0 ? len * OLED_CHAR_STEP - (OLED_CHAR_STEP - OLED_CHAR_W) : 0;
}

esp_err_t oled_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = -1, /* let the driver pick the one port this chip has */
        .sda_io_num = BOARD_I2C_SDA_GPIO,
        .scl_io_num = BOARD_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &bus);
    if (err != ESP_OK) {
        return err;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BOARD_OLED_I2C_ADDR,
        .scl_speed_hz = BOARD_I2C_FREQ_HZ,
    };
    err = i2c_master_bus_add_device(bus, &dev_cfg, &oled_dev);
    if (err != ESP_OK) {
        return err;
    }

    static const uint8_t init_seq[] = {
        0xAE,       /* display off -- stays off until the first clean frame */
        0xD5, 0x80, /* clock divide ratio / oscillator frequency */
        0xA8, (BOARD_OLED_MUX_RATIO - 1), /* multiplex ratio = 40 rows */
        0xD3, 0x00, /* display offset = 0 */
        0x40,       /* display start line = 0 */
        0x8D, 0x14, /* charge pump on */
        0x20, 0x02, /* memory addressing mode = page */
        0xA1,       /* segment remap: column 127 -> SEG0 */
        0xC8,       /* COM scan direction remapped */
        0xDA, 0x12, /* COM pins: alternative, no left/right remap */
        0x81, 0xFF, /* contrast */
        0xD9, 0xF1, /* pre-charge period */
        0xDB, 0x30, /* VCOMH deselect = 0.83 x Vcc */
        0xAD, 0x30, /* internal IREF enabled while the display is on */
        0x2E,       /* scrolling off */
        0xA4,       /* resume from RAM */
        0xA6,       /* normal, not inverted */
    };
    err = oled_cmds(init_seq, sizeof(init_seq));
    if (err != ESP_OK) {
        return err;
    }

    err = oled_clear_gddram();
    if (err != ESP_OK) {
        return err;
    }

    oled_clear();
    err = oled_flush();
    if (err != ESP_OK) {
        return err;
    }

    return oled_cmd(0xAF); /* display on, with a known-blank frame showing */
}
