# Copy-paste recipes — ESP32-C3 0.42" OLED

Every recipe is either extracted from `template/` (and therefore builds) or
marked **⚠︎ compile-checked only** / **⚠︎ untested**. ESP-IDF 6.0.1,
`platform-espressif32` 7.0.1.

Pin numbers come from `template/include/board.h`; do not retype them.

---

## 1. platformio.ini

```ini
[env:esp32c3-oled042]
platform = espressif32
board = esp32-c3-devkitm-1
framework = espidf

board_build.flash_mode = dio

upload_protocol = esptool
monitor_speed = 115200
monitor_filters = esp32_exception_decoder, direct
```

`esp32-c3-devkitm-1` is the generic C3 definition — there is none for this
board. `dfrobot_beetle_esp32c3` is equivalent under `framework = espidf`.

## 2. sdkconfig.defaults

```ini
# Without this the console goes to UART0 on GPIO20/GPIO21, which this board
# brings out to the header and connects to nothing. The monitor stays silent
# while the firmware runs perfectly.
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y

# ESP-IDF defaults to 2 MB no matter what the board definition says. The
# ESP32-C3FH4 here has 4 MB in-package.
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
```

## 3. board.h

```c
#define BOARD_I2C_SDA_GPIO 5
#define BOARD_I2C_SCL_GPIO 6
#define BOARD_I2C_FREQ_HZ  400000

#define BOARD_OLED_I2C_ADDR   0x3C
#define BOARD_OLED_W          72
#define BOARD_OLED_H          40
#define BOARD_OLED_PAGES      (BOARD_OLED_H / 8)  /* 5 */
#define BOARD_OLED_COL_OFFSET 28                  /* panel col 0 = GDDRAM col 28 */
#define BOARD_OLED_MUX_RATIO  40

#define BOARD_LED_GPIO   8   /* LOW = lit */
#define BOARD_LED_ON     0
#define BOARD_LED_OFF    1

#define BOARD_BOOT_GPIO    9 /* pressed = low, external pull-up fitted */
#define BOARD_BOOT_PRESSED 0

#define BOARD_UART0_TX_GPIO 21
#define BOARD_UART0_RX_GPIO 20
```

---

## 4. The OLED init sequence

The part that is specific to this panel, and the reason a stock SSD1306 driver
produces snow. Full driver in `template/src/oled.c`.

```c
static const uint8_t init_seq[] = {
    0xAE,       /* display off -- keep it off until the first clean frame */
    0xD5, 0x80, /* clock divide ratio / oscillator frequency */
    0xA8, 0x27, /* multiplex ratio = 40 rows. THE key difference: register
                 * takes rows-1, and a stock 128x64 init sends 0x3F here */
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
    0xAD, 0x30, /* internal IREF while the display is on */
    0x2E,       /* scrolling off */
    0xA4,       /* resume from RAM */
    0xA6,       /* normal, not inverted */
};
```

Order of the remaining init steps matters:

```c
oled_cmds(init_seq, sizeof(init_seq));
oled_clear_gddram();   /* all 8 pages x 128 columns -- power-on garbage */
oled_clear();          /* framebuffer */
oled_flush();          /* push a blank frame */
oled_cmd(0xAF);        /* NOW switch the display on */
```

Turning the display on before the first flush shows a fraction of a second of
random GDDRAM at every boot.

## 5. I2C bus + device

The ESP32-C3 has **one** I2C controller. `.i2c_port = -1` lets the driver pick
it.

```c
#include "driver/i2c_master.h"

i2c_master_bus_config_t bus_cfg = {
    .i2c_port = -1,
    .sda_io_num = BOARD_I2C_SDA_GPIO,
    .scl_io_num = BOARD_I2C_SCL_GPIO,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,  /* the board's own pull-ups do the work */
};
i2c_master_bus_handle_t bus;
ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = 0x3C,
    .scl_speed_hz = 400000,
};
i2c_master_dev_handle_t oled_dev;
ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &oled_dev));
```

Every I2C write to an SSD1306 is prefixed with a control byte: `0x00` for a
command stream, `0x40` for a data stream.

## 6. Flushing the framebuffer

360 bytes in five page transactions. The page and start column are re-sent per
page, so one failed transfer costs one band rather than shifting the frame.

```c
for (int page = 0; page < BOARD_OLED_PAGES; page++) {
    const uint8_t set_pos[] = {
        (uint8_t)(0xB0 | page),                              /* page address */
        (uint8_t)(0x00 | (BOARD_OLED_COL_OFFSET & 0x0F)),    /* column, low nibble */
        (uint8_t)(0x10 | (BOARD_OLED_COL_OFFSET >> 4)),      /* column, high nibble */
    };
    oled_cmds(set_pos, sizeof(set_pos));

    uint8_t tx[1 + BOARD_OLED_W];
    tx[0] = 0x40;
    memcpy(&tx[1], &framebuf[page * BOARD_OLED_W], BOARD_OLED_W);
    i2c_master_transmit(oled_dev, tx, sizeof(tx), 200);
}
```

At 400 kHz this is roughly 8 ms for a full frame — about 120 fps, so there is
no reason to implement partial updates on this panel.

## 7. Adding a second I2C device

The panel does **not** lock you out of I2C. Same bus, same pins, different
address. Do not create a second bus — there is only one controller, and
`i2c_new_master_bus()` a second time fails.

```c
/* reuse the `bus` handle from oled_init() */
i2c_device_config_t sensor_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = 0x68,          /* e.g. an IMU */
    .scl_speed_hz = 400000,
};
i2c_master_dev_handle_t sensor;
ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &sensor_cfg, &sensor));
```

If the new device carries its own pull-ups, consider removing them: the board
already has a pair, and stacking them over-drives the bus.

**⚠︎ compile-checked only** — the template does not ship a second device.

## 8. Scanning the bus

Useful first move when the panel is dark. A healthy board answers at exactly
one address, `0x3C`.

```c
for (uint8_t addr = 1; addr < 0x7F; addr++) {
    if (i2c_master_probe(bus, addr, 50) == ESP_OK) {
        ESP_LOGI(TAG, "device at 0x%02X", addr);
    }
}
```

**⚠︎ compile-checked only.**

## 9. The blue LED

```c
#include "driver/gpio.h"

gpio_config_t cfg = {
    .pin_bit_mask = 1ULL << BOARD_LED_GPIO,
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
};
ESP_ERROR_CHECK(gpio_config(&cfg));

gpio_set_level(BOARD_LED_GPIO, BOARD_LED_ON);   /* 0 -- lit */
gpio_set_level(BOARD_LED_GPIO, BOARD_LED_OFF);  /* 1 -- dark */
```

GPIO8 is a strapping pin. Driving it as an output *after* boot is fine; what
matters is that nothing external holds it low at reset.

## 10. The BOOT button as a user button

GPIO9 already has an external pull-up, so no internal one is needed. Pressed
reads 0.

```c
gpio_config_t cfg = {
    .pin_bit_mask = 1ULL << BOARD_BOOT_GPIO,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,   /* harmless; the board's is stronger */
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
};
ESP_ERROR_CHECK(gpio_config(&cfg));

bool pressed = gpio_get_level(BOARD_BOOT_GPIO) == BOARD_BOOT_PRESSED;
```

Debounce in software — 20–30 ms of agreement. Remember that this pin is also
read at reset to choose the boot mode, so holding it while the board resets
puts you in download mode rather than running your handler.

**⚠︎ compile-checked only.**

## 11. ADC

ADC1 only: GPIO0–GPIO4 are channels 0–4. ADC2's single channel is GPIO5, which
is the OLED's SDA line, and ADC2 is errata-limited on some C3 revisions — treat
it as unavailable.

```c
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

adc_oneshot_unit_handle_t adc1;
adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = ADC_UNIT_1 };
ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc1));

adc_oneshot_chan_cfg_t chan_cfg = {
    .bitwidth = ADC_BITWIDTH_DEFAULT,
    .atten = ADC_ATTEN_DB_12,          /* 0-2500 mV, +/-35 mV after calibration */
};
ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1, ADC_CHANNEL_0, &chan_cfg)); /* GPIO0 */

adc_cali_handle_t cali = NULL;
adc_cali_curve_fitting_config_t cali_cfg = {
    .unit_id = ADC_UNIT_1,
    .atten = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_DEFAULT,
};
adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali);   /* ADC1 is factory-calibrated */

int raw, mv;
ESP_ERROR_CHECK(adc_oneshot_read(adc1, ADC_CHANNEL_0, &raw));
if (cali) {
    adc_cali_raw_to_voltage(cali, raw, &mv);
}
```

**⚠︎ compile-checked only.**

## 12. Application UART on the header

The console lives on USB Serial/JTAG, so UART0's IO MUX pins are free for your
own protocol. Nothing special is needed — just do not expect `printf` to come
out of them.

```c
#include "driver/uart.h"

const uart_config_t uart_cfg = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};
ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0));
ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_cfg));
ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, BOARD_UART0_TX_GPIO, BOARD_UART0_RX_GPIO,
                             UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
```

**⚠︎ compile-checked only.**

## 13. U8g2 / Arduino, if you are not using ESP-IDF

Two constructors work. Prefer the first — it carries the 40-row init and needs
no offsets:

```cpp
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, /*clock=*/6, /*data=*/5);
```

Note the argument order: **clock (SCL) before data (SDA)**, i.e. 6 then 5. Get
it backwards and nothing answers.

The older approach, still seen in most forum posts, treats the panel as a
128 × 64 and draws into the middle:

```cpp
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5);
const int W = 72, H = 40, xOffset = 30, yOffset = 12;  // or 28, 24 -- reports differ
```

Reported offsets disagree (30/12 vs 28/24) because they depend on which init
sequence ran. That ambiguity is the reason to use the `72X40_ER` constructor.

**⚠︎ untested by the author of this skill** — the template is ESP-IDF.

## 14. Regenerating the font

```sh
python3 scripts/genfont.py            # rewrite include/font5x7.h
python3 scripts/genfont.py --preview  # print every glyph as ASCII art
```

Glyphs are edited as ASCII art in the script's `GLYPHS` table. Storage is
column-major with bit 0 as the top row — the same orientation the SSD1306 uses
for its pages, so drawing a character never needs a transpose.

A 5 × 7 cell with one column of spacing gives **12 characters across** and
**5 lines** on the 72 × 40 panel.
