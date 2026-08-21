# Copy-paste recipes — Waveshare ESP32-C6-LCD-1.47

Extracted from `template/`, which builds clean against ESP-IDF 6.0.1. Anything that
is **not** in the template is marked **⚠︎ compile-checked only** — it compiles, but it
has not been run on hardware by the author of this skill.

Contents: [1 platformio.ini](#1-platformioini) · [2 sdkconfig](#2-sdkconfigdefaults) ·
[3 board.h](#3-boardh) · [4 LCD bring-up](#4-lcd-bring-up) ·
[5 backlight](#5-backlight-pwm) · [6 present + tearing](#6-presenting-a-frame) ·
[7 partial redraw](#7-partial-redraw-getting-past-45-fps) ·
[8 RGB LED](#8-rgb-led-ws2812-over-rmt) · [9 TF card](#9-tf-card-on-the-shared-bus) ·
[10 ADC](#10-adc-on-gpio0gpio3) · [11 BOOT button](#11-the-boot-button-as-an-input) ·
[12 font tables](#12-regenerating-the-font)

---

## 1. platformio.ini

```ini
[env:esp32-c6-lcd-1_47]
platform = espressif32
board = esp32-c6-devkitc-1     ; no board definition exists for this board
framework = espidf

; The Waveshare module carries 4 MB, not the DevKitC definition's 8 MB.
board_upload.flash_size = 4MB
board_upload.maximum_size = 4194304

; The Type-C port is the SoC's built-in USB-Serial-JTAG; there is no bridge chip.
upload_protocol = esptool
monitor_speed = 115200
monitor_filters = esp32_exception_decoder, direct
```

## 2. sdkconfig.defaults

```
# Without this, printf/ESP_LOG go to UART0 (GPIO16/17), which nothing on this
# board is wired to, and the monitor stays silent while the firmware runs.
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y

# The ESP32-C6FH4 has 4 MB of in-package flash, not the DevKitC's 8 MB.
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
```

## 3. board.h

```c
#define BOARD_PIN_LCD_MOSI 6    // shared with the TF card slot
#define BOARD_PIN_LCD_SCLK 7    // shared with the TF card slot
#define BOARD_PIN_LCD_CS   14
#define BOARD_PIN_LCD_DC   15
#define BOARD_PIN_LCD_RST  21
#define BOARD_PIN_LCD_BL   22   // LEDC PWM, never a plain high level

#define BOARD_PIN_SD_CS    4
#define BOARD_PIN_SD_MISO  5
#define BOARD_PIN_SD_MOSI  BOARD_PIN_LCD_MOSI
#define BOARD_PIN_SD_SCLK  BOARD_PIN_LCD_SCLK

#define BOARD_PIN_RGB_LED  8
#define BOARD_PIN_BOOT     9    // pressed = low

#define BOARD_LCD_H_RES    172
#define BOARD_LCD_V_RES    320
#define BOARD_LCD_X_GAP    34   // (240 - 172) / 2
#define BOARD_LCD_Y_GAP    0
#define BOARD_LCD_PIXEL_CLK_HZ (40 * 1000 * 1000)
```

## 4. LCD bring-up

Four settings here are the difference between a working panel and a plausible-looking
mess: `data_endian = BIG`, `invert_color(true)`, `set_gap(34, 0)`, and
`miso_io_num = -1`.

```c
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define LCD_HOST SPI2_HOST

static SemaphoreHandle_t s_flush_sem;
static esp_lcd_panel_io_handle_t s_io;
esp_lcd_panel_handle_t g_panel;

static bool on_color_trans_done(esp_lcd_panel_io_handle_t io,
                                esp_lcd_panel_io_event_data_t *edata,
                                void *user_ctx)
{
    BaseType_t hpwoken = pdFALSE;
    xSemaphoreGiveFromISR(s_flush_sem, &hpwoken);
    return hpwoken == pdTRUE;
}

void lcd_init(void)
{
    s_flush_sem = xSemaphoreCreateCounting(1, 0);

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = BOARD_PIN_LCD_MOSI,
        .miso_io_num = -1,                    // write-only panel
        .sclk_io_num = BOARD_PIN_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_LCD_H_RES * BOARD_LCD_V_RES * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = BOARD_PIN_LCD_CS,
        .dc_gpio_num = BOARD_PIN_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = BOARD_LCD_PIXEL_CLK_HZ,    // 40 MHz is the ceiling here
        .trans_queue_depth = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .on_color_trans_done = on_color_trans_done,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_HOST, &io_cfg, &s_io));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BOARD_PIN_LCD_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian    = LCD_RGB_DATA_ENDIAN_BIG,   // the panel is big-endian
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(s_io, &panel_cfg, &g_panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(g_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(g_panel));
    // IPS panels ship inverted; without this the image is a photo negative.
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(g_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(g_panel, BOARD_LCD_X_GAP, BOARD_LCD_Y_GAP));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(g_panel, true));
}
```

Pack pixels big-endian once, at compile time, so nothing swaps per pixel at runtime:

```c
#define GFX_RGB(r, g, b)                                                       \
    (uint16_t)(__builtin_bswap16((uint16_t)((((r) & 0xF8) << 8) |              \
                                     (((g) & 0xFC) << 3) | ((b) >> 3))))
```

## 5. Backlight PWM

**Not `gpio_set_level()`.** Waveshare's own warning: above 50 % for long periods the
panel overheats and develops permanent dark shadows.

```c
#include "driver/ledc.h"

#define BL_LEDC_TIMER   LEDC_TIMER_0
#define BL_LEDC_CHANNEL LEDC_CHANNEL_0
#define BL_LEDC_MODE    LEDC_LOW_SPEED_MODE
#define BL_LEDC_RES     LEDC_TIMER_10_BIT
#define BL_MAX_PCT      50

void backlight_init(void)
{
    ledc_timer_config_t t = {
        .speed_mode = BL_LEDC_MODE, .timer_num = BL_LEDC_TIMER,
        .duty_resolution = BL_LEDC_RES, .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&t));

    ledc_channel_config_t ch = {
        .gpio_num = BOARD_PIN_LCD_BL, .speed_mode = BL_LEDC_MODE,
        .channel = BL_LEDC_CHANNEL, .timer_sel = BL_LEDC_TIMER,
        .duty = 0, .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
}

void backlight_set(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > BL_MAX_PCT) percent = BL_MAX_PCT;
    const uint32_t max_duty = (1u << BL_LEDC_RES) - 1u;
    ESP_ERROR_CHECK(ledc_set_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL,
                                  (max_duty * (uint32_t)percent) / 100u));
    ESP_ERROR_CHECK(ledc_update_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL));
}
```

LEDC also fades in hardware — `ledc_set_fade_with_time()` after
`ledc_fade_func_install(0)` — which is the polite way to bring the panel up once the
first frame is composed.

## 6. Presenting a frame

Blocking on the transfer-done semaphore is what makes tearing impossible: the caller
cannot touch `g_fb` while GDMA is reading it.

```c
uint16_t g_fb[BOARD_LCD_H_RES * BOARD_LCD_V_RES];   // 110,080 bytes of .bss

void gfx_present(void)
{
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(g_panel, 0, 0,
                                              BOARD_LCD_H_RES, BOARD_LCD_V_RES,
                                              g_fb));
    xSemaphoreTake(s_flush_sem, portMAX_DELAY);
}
```

## 7. Partial redraw (getting past 45 fps)

A full frame is 22 ms on the wire. If the UI only changes in a strip, send the strip:

```c
// Push rows [y0, y1) only. 172x40 is 2.75 ms instead of 22 ms.
static void gfx_present_rows(int y0, int y1)
{
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(g_panel, 0, y0,
                                              BOARD_LCD_H_RES, y1,
                                              &g_fb[y0 * BOARD_LCD_H_RES]));
    xSemaphoreTake(s_flush_sem, portMAX_DELAY);
}
```

`esp_lcd`'s coordinates are `(x_start, y_start, x_end, y_end)` with the end
**exclusive**, and the buffer must be the top-left corner of that rectangle in a
row-major buffer whose stride equals the rectangle width — which for full-width bands
is just an offset into `g_fb`, and for narrower rectangles means compositing into a
scratch buffer first.

## 8. RGB LED (WS2812 over RMT)

GRB wire order, `msb_first`, 0.1 µs ticks. Keep the values low — the LED is under
clear acrylic next to the panel and is far brighter than the numbers suggest.

```c
#include "driver/rmt_tx.h"

static rmt_channel_handle_t s_chan;
static rmt_encoder_handle_t s_encoder;

void rgb_led_init(void)
{
    rmt_tx_channel_config_t chan_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = BOARD_PIN_RGB_LED,
        .mem_block_symbols = 64,
        .resolution_hz = 10000000,          // 0.1 us per tick
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&chan_cfg, &s_chan));

    rmt_bytes_encoder_config_t enc_cfg = {
        .bit0 = { .level0 = 1, .duration0 = 3, .level1 = 0, .duration1 = 9 },
        .bit1 = { .level0 = 1, .duration0 = 9, .level1 = 0, .duration1 = 3 },
        .flags.msb_first = 1,
    };
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&enc_cfg, &s_encoder));
    ESP_ERROR_CHECK(rmt_enable(s_chan));
}

void rgb_led_set(uint8_t r, uint8_t g, uint8_t b)
{
    const uint8_t grb[3] = { g, r, b };     // WS2812 wire order
    rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    ESP_ERROR_CHECK(rmt_transmit(s_chan, s_encoder, grb, sizeof(grb), &tx_cfg));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(s_chan, -1));
}
```

## 9. TF card on the shared bus

**⚠︎ compile-checked only.**

The card sits on the *same* SPI2 bus as the panel. Initialise the bus exactly once —
if the LCD came up first, skip `spi_bus_initialize()` here or it returns
`ESP_ERR_INVALID_STATE`.

```c
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static sdmmc_card_t *s_card;

esp_err_t sdcard_mount(void)
{
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    // The bus is shared with a 40 MHz panel; the card is the slower device.
    host.max_freq_khz = 20000;

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = BOARD_PIN_SD_CS;
    slot.host_id = SPI2_HOST;

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
    };
    return esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot, &mount_cfg, &s_card);
}
```

If the card is the *only* SPI device, initialise the bus yourself first with
`.mosi_io_num = 6, .miso_io_num = 5, .sclk_io_num = 7`. Note `miso_io_num = 5` —
the panel recipe uses `-1`, so a project that wants both must initialise the bus with
MISO present.

1-bit SPI mode is the only option: `SD_D1`/`SD_D2` are not connected on this board.
Do card I/O between frames rather than inside a redraw — a full-frame flush occupies
the bus for 22 ms.

## 10. ADC on GPIO0–GPIO3

**⚠︎ compile-checked only.** GPIO4–GPIO6 carry the card slot and the SPI bus, so
`ADC1_CH0`–`CH3` are the only channels this board leaves you.

```c
#include "esp_adc/adc_oneshot.h"

static adc_oneshot_unit_handle_t s_adc;

void adc_init(void)
{
    adc_oneshot_unit_init_cfg_t unit = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit, &s_adc));

    adc_oneshot_chan_cfg_t chan = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten    = ADC_ATTEN_DB_12,        // full 0-3.3 V range, +-40 mV
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, ADC_CHANNEL_0, &chan));
}

int adc_read_mv_raw(void)
{
    int raw = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(s_adc, ADC_CHANNEL_0, &raw));
    return raw;
}
```

`ADC_CHANNEL_0`–`ADC_CHANNEL_3` map to GPIO0–GPIO3. The datasheet's accuracy figures
were measured with Wi-Fi disabled; expect worse with the radio active.

## 11. The BOOT button as an input

**⚠︎ compile-checked only.** GPIO9 has the BOOT button on it and a weak internal
pull-up, so it reads high when idle and low when pressed. Perfectly usable as a user
button *after* boot — just never drive it low across a reset unless you want download
mode.

```c
#include "driver/gpio.h"

void boot_button_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BOARD_PIN_BOOT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
}

bool boot_button_pressed(void)
{
    return gpio_get_level(BOARD_PIN_BOOT) == 0;
}
```

## 12. Regenerating the font

`template/include/font12x24.h` is generated, not hand-written: ASCII U+0020–U+007E
plus Cyrillic U+0400–U+045F, 1 bit per pixel, 24 `uint16_t` rows per glyph, MSB =
leftmost pixel.

```sh
pip install pillow
python3 scripts/genfont.py [/path/to/Mono.ttf]     # writes include/font12x24.h
```

Any monospaced TTF works; the default is macOS's Menlo, whose 0.6 em advance puts
20 pt exactly on the 12 px cell. The script prints an ASCII-art preview so you can
eyeball the result before flashing.
