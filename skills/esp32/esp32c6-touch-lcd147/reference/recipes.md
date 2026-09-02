# Copy-paste recipes — Waveshare ESP32-C6-Touch-LCD-1.47

Everything here is extracted from `template/`, which builds clean on ESP-IDF 6.1.0 and
runs on the board. Recipes marked **⚠︎ compile-checked only** were built but never
exercised on hardware — say so if you hand one to a user.

Background for each: [board-hardware.md](board-hardware.md) for the pins and the
electrical story, [touch-axs5106l.md](touch-axs5106l.md) for the touch protocol.

---

## 1. platformio.ini

```ini
[env:esp32-c6-touch-lcd-1_47]
platform  = espressif32@7.1.0
board     = esp32-c6-devkitc-1
framework = espidf

monitor_speed = 115200
monitor_filters = esp32_exception_decoder, direct

; Native USB Serial/JTAG: no bridge chip, so there is no DTR/RTS to drive.
monitor_rts = 0
monitor_dtr = 0

; Without these PlatformIO takes the first /dev/cu.* it finds — on macOS a
; Bluetooth pseudo-port — and the monitor silently attaches to nothing.
; Linux: /dev/ttyACM*   Windows: the COM port it enumerates as.
monitor_port = /dev/cu.usbmodem*
upload_port  = /dev/cu.usbmodem*
```

No `board_upload.flash_size` override is needed: the DevKitC definition's 8 MB matches
the ESP32-C6FH8. (The *non-touch* board needs one — it is a 4 MB part.)

## 2. sdkconfig.defaults

```ini
# ESP32-C6FH8: 8 MB in package.
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y

# Console over the Type-C port. Without this, printf/ESP_LOG go to UART0 on
# GPIO16/17, which only reach the header: the monitor stays silent.
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y

CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
```

## 3. board_pins.h

The full file is `template/include/board_pins.h`. The load-bearing half:

```c
#define BSP_LCD_SPI_HOST    SPI2_HOST
#define BSP_LCD_SCLK        GPIO_NUM_1    /* shared with the TF card */
#define BSP_LCD_MOSI        GPIO_NUM_2    /* shared with the TF card */
#define BSP_LCD_CS          GPIO_NUM_14
#define BSP_LCD_DC          GPIO_NUM_15
#define BSP_LCD_RST         GPIO_NUM_22
#define BSP_LCD_BL          GPIO_NUM_23   /* NPN base via 1k; LEDC PWM, active high */

#define BSP_LCD_H_RES       172
#define BSP_LCD_V_RES       320
#define BSP_LCD_PIXEL_CLK   (40 * 1000 * 1000)

#define BSP_I2C_PORT        I2C_NUM_0
#define BSP_I2C_SDA         GPIO_NUM_18   /* shared with the QMI8658A IMU */
#define BSP_I2C_SCL         GPIO_NUM_19
#define BSP_TP_RST          GPIO_NUM_20
#define BSP_TP_INT          GPIO_NUM_21

#define BSP_IMU_I2C_ADDR    0x6B          /* SA0 = GND */
#define BSP_IMU_INT1        GPIO_NUM_5
#define BSP_IMU_INT2        GPIO_NUM_6

#define BSP_BOOT_BTN        GPIO_NUM_9    /* pressed = low — NOT GPIO8 */

#define BSP_SD_CS           GPIO_NUM_4
#define BSP_SD_MISO         GPIO_NUM_3

#define BSP_BAT_ADC_GPIO    GPIO_NUM_0    /* ADC1_CH0; VBAT = 3 x V(GPIO0) */
```

## 4. SPI bus and JD9853 panel bring-up

From `display_init()` in `template/src/main.c`. Note the order: park the card's CS,
open the bus once, create the semaphore *before* the panel IO that will signal it.

```c
#define TILE_ROWS 32   /* staging-buffer height in pixels */

static esp_lcd_panel_handle_t s_panel;
static uint16_t *s_tile;
static SemaphoreHandle_t s_flush_done;

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
        .miso_io_num = -1,            /* the panel is write-only */
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
        .pclk_hz = BSP_LCD_PIXEL_CLK,     /* 40 MHz — the GPIO-Matrix ceiling */
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
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));      /* sends the vendor batch */
    /* The visible 172 columns start at column 34 of the controller's RAM. */
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_panel, JD9853_LCD_X_GAP, JD9853_LCD_Y_GAP));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    s_tile = heap_caps_malloc(BSP_LCD_H_RES * TILE_ROWS * sizeof(uint16_t), MALLOC_CAP_DMA);
    ESP_ERROR_CHECK(s_tile ? ESP_OK : ESP_ERR_NO_MEM);
}
```

Do **not** add `esp_lcd_panel_invert_color()`, `swap_xy()` or a `COLMOD`/`MADCTL` of your
own: the vendor batch in `components/jd9853/` already ends with `INVON` and sets both
registers, and sending them again upsets the register paging.

Colours are pre-swapped at compile time because the panel is big-endian:

```c
#define RGB565(r, g, b)  ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define COLOR(r, g, b)   ((uint16_t)((RGB565(r, g, b) << 8) | (RGB565(r, g, b) >> 8)))
```

## 5. Flushing a rectangle without corrupting the buffer

`flush_rect()` from `template/src/main.c` — the pattern that makes the asynchronous
`draw_bitmap` safe, and the one that keeps RAM at 11 KB instead of 110 KB.

```c
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
```

`compose(canvas_t *)` paints the whole scene clipped to one band. Redrawing a small
rectangle is the whole performance story here: `move_dot()` repaints two 19 × 19 boxes,
not the screen.

## 6. Backlight PWM

GPIO23 drives an NPN base through 1 kΩ — active high, PWM-able straight from LEDC. The
C6 has **no high-speed mode**, so `LEDC_LOW_SPEED_MODE` is not a choice.

```c
static void backlight_init(void)      /* call this FIRST, before the panel */
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
        .duty = 0,                    /* 0 % until there is something to show */
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
```

Order in `app_main()`:

```c
backlight_init();     /* claims GPIO23 at 0 % — the reset pull-up had it lit */
display_init();
flush_all();
backlight_set(90);    /* only now is there a picture worth lighting */
```

## 7. I²C bus and a scan

One bus, two onboard devices, 10 kΩ pull-ups already fitted.

```c
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
```

The scan from `template/variants/minimal/main.c` — the first thing to run on a board
that is not behaving:

```c
for (uint16_t addr = 0x08; addr < 0x78; addr++) {
    if (i2c_master_probe(bus, addr, 50) == ESP_OK) {
        ESP_LOGI(TAG, "  0x%02X  %s", (unsigned)addr,
                 addr == 0x63 ? "AXS5106L touch" :
                 addr == 0x6B ? "QMI8658A IMU"   : "unknown");
    }
}
```

A healthy board answers at **0x63** and **0x6B**. Silence on both means the bus, not the
devices.

## 8. AXS5106L touch

Read [touch-axs5106l.md](touch-axs5106l.md) before writing any of your own. Using the
vendored driver:

```c
axs5106l_config_t tp_cfg = {
    .i2c_bus = i2c_bus,
    .rst_gpio = BSP_TP_RST,
    .int_gpio = BSP_TP_INT,
    .width = BSP_LCD_H_RES,
    .height = BSP_LCD_V_RES,
    /* rotation 0 for this panel = mirror X only: */
    .mirror_x = true,
};
axs5106l_handle_t tp = NULL;
ESP_ERROR_CHECK(axs5106l_new(&tp_cfg, &tp));

uint16_t fw_ver = 0;
uint32_t chip_id = 0;
if (axs5106l_read_info(tp, &fw_ver, &chip_id) == ESP_OK) {
    ESP_LOGI(TAG, "AXS5106L: chip ID 0x%06X, firmware V%u",
             (unsigned)chip_id, (unsigned)fw_ver);   /* an all-zero ID is normal */
}

while (true) {
    axs5106l_data_t t;
    if (axs5106l_read(tp, &t) == ESP_OK && t.pressed) {
        ESP_LOGI(TAG, "touch  x=%3d y=%3d  (raw %u,%u)", t.x, t.y, t.raw_x, t.raw_y);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
}
```

`t.x` / `t.y` are screen coordinates after the configured transform; `t.raw_x` /
`t.raw_y` are what the controller reported, which is what a calibration needs.

The two things inside the driver that you must not "simplify":

```c
/* Two transactions on purpose: this chip does not answer a repeated START. */
i2c_master_transmit(h->dev, &reg, 1, AXS5106L_I2C_TMO_MS);
i2c_master_receive(h->dev, data, len, AXS5106L_I2C_TMO_MS);

/* RST low 200 ms, release, wait 300 ms. Shorter pulses leave it reporting nonsense. */
gpio_set_level(h->rst_gpio, 0);
vTaskDelay(pdMS_TO_TICKS(200));
gpio_set_level(h->rst_gpio, 1);
vTaskDelay(pdMS_TO_TICKS(300));
```

## 9. Three-point touch calibration, stored in NVS

The template leaves the driver's transform at identity and maps in the app, so one code
path serves both the stock mapping and a calibrated one.

```c
typedef struct {
    uint8_t swap;   /* screen X is driven by the controller's Y and vice versa */
    int16_t x_lo;   /* raw value seen at screen x = W/4   */
    int16_t x_hi;   /* raw value seen at screen x = 3*W/4 */
    int16_t y_lo;   /* raw value seen at screen y = H/4   */
    int16_t y_hi;   /* raw value seen at screen y = 3*H/4 */
} touch_cal_t;

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
```

The stock mapping expressed in the same form, so nothing special-cases it:

```c
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
```

Storage is a raw blob under namespace `touch`, key `cal_v2` — `nvs_get_blob` with a
length check, `nvs_set_blob` + `nvs_commit`. `nvs_flash_init()` must handle
`ESP_ERR_NVS_NO_FREE_PAGES` / `ESP_ERR_NVS_NEW_VERSION_FOUND` by erasing and retrying.
The full flow, including the target sequence and the release-detection, is `cal_run()`
in `template/src/main.c`.

## 10. The BOOT button as an input

**GPIO9**, pressed = low, with a 10 kΩ pull-up on the board. (GPIO8 is *not* the button —
board-hardware.md §4.2.)

```c
static bool boot_button_held(void)
{
    gpio_config_t btn = {
        .pin_bit_mask = 1ULL << BSP_BOOT_BTN,     /* GPIO_NUM_9 */
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&btn));
    return gpio_get_level(BSP_BOOT_BTN) == 0;
}
```

Sampling it once at boot, as the template does, is safe. Holding it low *across a reset*
is what puts the chip in download mode, so do not drive it low from firmware.

## 11. Battery voltage

⚠︎ **compile-checked only.** VBAT reaches GPIO0 (`ADC1_CH0`) through 200 kΩ / 100 kΩ, so
`VBAT = 3 × V(GPIO0)`. The 100 nF at the node is what lets a 66 kΩ source drive the SAR
ADC at all; still average, and calibrate the result against a meter.

```c
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;

static void battery_init(void)
{
    adc_oneshot_unit_init_cfg_t unit = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit, &s_adc));

    adc_oneshot_chan_cfg_t chan = {
        .atten = ADC_ATTEN_DB_12,       /* VBAT/3 = 1.4 V at 4.2 V — fits */
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, ADC_CHANNEL_0, &chan));

    adc_cali_curve_fitting_config_t cali = {
        .unit_id = ADC_UNIT_1,
        .chan = ADC_CHANNEL_0,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali, &s_cali) != ESP_OK) {
        s_cali = NULL;
    }
}

static int battery_mv(void)
{
    int32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        int raw = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(s_adc, ADC_CHANNEL_0, &raw));
        sum += raw;
    }
    int raw = (int)(sum / 16);

    int mv = raw;
    if (s_cali) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(s_cali, raw, &mv));
    }
    return mv * 3;                      /* the 200k/100k divider */
}
```

Requires `esp_adc` in the component's `REQUIRES`. Note the divider is wired permanently
and draws ~14 µA at 4.2 V, so it shows up in any deep-sleep budget.

## 12. QMI8658A IMU

⚠︎ **Address and wiring are schematic + datasheet; no code has been run against it.**

| | |
|---|---|
| Address | **0x6B** — the QST datasheet says SA0 pulled *down* gives 0x6B, up/open gives 0x6A. This board grounds SA0 (U3 pin 1 → GND) |
| Mode | I²C: `CS` (pin 12) is tied to 3V3 |
| Interrupts | `INT1` → GPIO5, `INT2` → GPIO6 (both also on the header) |
| `WHO_AM_I` | register 0x00, reads **0x05** |

Presence check — the same lesson as the touch controller, use the address ACK:

```c
if (i2c_master_probe(bus, BSP_IMU_I2C_ADDR, 50) != ESP_OK) {
    ESP_LOGE(TAG, "no QMI8658A at 0x%02X", BSP_IMU_I2C_ADDR);
}
```

The register map (CTRL1…CTRL9, the FIFO, the sensor-data block) is in `QMI8658A.pdf`.
Read it there rather than guessing; the accel/gyro configuration registers are not
symmetric with any other QST part.

Whether this chip needs two transactions per read like the AXS5106L is **unknown** — try
`i2c_master_transmit_receive()` first, and if reads fail while the address ACKs, fall
back to the write-STOP-read pattern of §8.

## 13. TF card on the shared bus

⚠︎ **compile-checked only.** The card is on the *same* SPI2 bus as the panel — do not
call `spi_bus_initialize()` a second time; it returns `ESP_ERR_INVALID_STATE`.

Two changes to §4 are needed before the card can work:

1. `.miso_io_num = BSP_SD_MISO` (GPIO3) in the bus config, instead of `-1`.
2. Drop the manual `gpio_set_level(BSP_SD_CS, 1)` — `esp_vfs_fat_sdspi_mount()` claims
   GPIO4 itself.

```c
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static sdmmc_card_t *s_card;

static esp_err_t sd_mount(void)
{
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = BSP_LCD_SPI_HOST;
    host.max_freq_khz = 20000;          /* the panel owns the bus at 40 MHz */

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = BSP_SD_CS;
    slot.host_id = BSP_LCD_SPI_HOST;

    esp_vfs_fat_sdmmc_mount_config_t mount = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
    };
    return esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot, &mount, &s_card);
}
```

Format cards **FAT32** (Waveshare's own note). The slot is SPI-only — `SD_D1`/`SD_D2` are
not connected — so 4-bit SDMMC mode is not available at any speed.

Sharing rules: the ESP-IDF SPI master serialises transactions per device, so the panel
and the card coexist, but a long card transfer stalls the display. If the display garbles
while the card is *idle*, the card's CS is not parked high.
