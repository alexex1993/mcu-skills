# JC4880P443C_I_W recipes

Code that is known to build **and has run on the board** unless a section says otherwise.
Toolchain for all of it: PlatformIO Core 6.2.0, pioarduino 55.03.311, ESP-IDF 5.5.5,
`board = esp32-p4-evboard`, `framework = espidf`, a rev v1.3 ESP32-P4.

Sections 1-8 are extracted from `template/`, where the complete files live; open those
rather than reassembling a project from these fragments. Sections 9-13 were built and run
as separate test projects during the bring-up and are not in the template.

| § | Recipe | Status |
|---|---|---|
| 1 | `platformio.ini` | run |
| 2 | `sdkconfig.defaults` | run |
| 3 | component manifest + CMake | run |
| 4 | MIPI-DSI panel (ST7701S) | run, observed on the glass |
| 5 | Drawing into the framebuffer | run, observed |
| 6 | Backlight | run, observed |
| 7 | Touch (GT911) | run, coordinates logged |
| 8 | I2C bus, ES8311 ID, battery, die temperature | run |
| 9 | Wi-Fi through the ESP32-C6 | run, scans found 9-15 networks |
| 10 | 16 MB partition table | run, partitions read back |
| 11 | microSD | run with **no card** — reaches the expected timeout |
| 12 | Audio: ES8311 + NS4150 + mic | run; codec opens, mic delivers samples. **No speaker was attached** |
| 13 | Arduino | not run here; Guition's settings |

---

## 1. `platformio.ini`

```ini
[env:jc4880p443]
platform  = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip
board     = esp32-p4-evboard
framework = espidf

board_build.flash_mode  = qio      ; esptool only — see sdkconfig.defaults
board_upload.flash_size = 16MB
board_build.partitions  = partitions_singleapp_large.csv

upload_speed    = 921600
monitor_speed   = 115200
monitor_filters = direct, esp32_exception_decoder, time
```

## 2. `sdkconfig.defaults`

```
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y      # rev v1.3 silicon — else "Illegal instruction"
CONFIG_ESP32P4_REV_MIN_100=y              # (rev >= 3.0 boards: CONFIG_ESP32P4_REV_MIN_301=y instead)
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y          # the .ini flash_mode does not reach the app
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_SPI_FLASH_SUPPORT_BOYA_CHIP=y      # the flash is a Boya part, JEDEC 0x684018
CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_HEX=y
CONFIG_SPIRAM_SPEED_200M=y
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192      # 10240 once Wi-Fi is started from app_main
```

## 3. Component manifest and CMake

`src/idf_component.yml`:

```yaml
dependencies:
  idf: ">=5.5"
  espressif/esp_lcd_st7701: "^1.1.5"        # resolved 1.1.5
  espressif/esp_lcd_touch_gt911: "^1.2.1"   # resolved 1.2.1 (+ esp_lcd_touch 1.2.1)
```

`src/CMakeLists.txt` globs, so both template variants share it:

```cmake
file(GLOB app_sources ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
idf_component_register(SRCS ${app_sources}
                       INCLUDE_DIRS "." "../include")
```

## 4. MIPI-DSI panel — ST7701S, 480 × 800

From `template/src/display.c`. The **init table is Guition's** and is the part that
cannot be skipped: with `init_cmds = NULL` the registry driver sends its own default and the
panel stays **black with no error** (tested).

```c
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7701.h"
#include "esp_ldo_regulator.h"

static const st7701_lcd_init_cmd_t s_init_cmds[] = {
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]){0x08}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x63, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t[]){0x10, 0x08}, 2, 0},
    {0xCC, (uint8_t[]){0x10}, 1, 0},
    {0xB0, (uint8_t[]){0x80, 0x09, 0x53, 0x0C, 0xD0, 0x07, 0x0C, 0x09, 0x09, 0x28, 0x06, 0xD4, 0x13, 0x69, 0x2B, 0x71}, 16, 0},
    {0xB1, (uint8_t[]){0x80, 0x94, 0x5A, 0x10, 0xD3, 0x06, 0x0A, 0x08, 0x08, 0x25, 0x03, 0xD3, 0x12, 0x66, 0x6A, 0x0D}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x5D}, 1, 0},
    {0xB1, (uint8_t[]){0x58}, 1, 0},
    {0xB2, (uint8_t[]){0x87}, 1, 0},
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x4E}, 1, 0},
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x21}, 1, 0},
    {0xB9, (uint8_t[]){0x10, 0x1F}, 2, 0},
    {0xBB, (uint8_t[]){0x03}, 1, 0},
    {0xBC, (uint8_t[]){0x00}, 1, 0},
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xD0, (uint8_t[]){0x88}, 1, 0},
    {0xE0, (uint8_t[]){0x00, 0x3A, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x04, 0xA0, 0x00, 0xA0, 0x05, 0xA0, 0x00, 0xA0, 0x00, 0x40, 0x40}, 11, 0},
    {0xE2, (uint8_t[]){0x30, 0x00, 0x40, 0x40, 0x32, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00}, 13, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t[]){0x09, 0x2E, 0xA0, 0xA0, 0x0B, 0x30, 0xA0, 0xA0, 0x05, 0x2A, 0xA0, 0xA0, 0x07, 0x2C, 0xA0, 0xA0}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t[]){0x08, 0x2D, 0xA0, 0xA0, 0x0A, 0x2F, 0xA0, 0xA0, 0x04, 0x29, 0xA0, 0xA0, 0x06, 0x2B, 0xA0, 0xA0}, 16, 0},
    {0xEB, (uint8_t[]){0x00, 0x00, 0x4E, 0x4E, 0x00, 0x00, 0x00}, 7, 0},
    {0xEC, (uint8_t[]){0x08, 0x01}, 2, 0},
    {0xED, (uint8_t[]){0xB0, 0x2B, 0x98, 0xA4, 0x56, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0x65, 0x4A, 0x89, 0xB2, 0x0B}, 16, 0},
    {0xEF, (uint8_t[]){0x08, 0x08, 0x08, 0x45, 0x3F, 0x54}, 6, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x11, (uint8_t[]){0x00}, 1, 120}, /* sleep out */
    {0x29, (uint8_t[]){0x00}, 1, 20},  /* display on */
};

esp_lcd_panel_handle_t panel;

/* 1. DSI PHY power: on-chip LDO channel 3 at 2.5 V, before anything DSI */
esp_ldo_channel_handle_t ldo;
esp_ldo_acquire_channel(&(esp_ldo_channel_config_t){.chan_id = 3, .voltage_mv = 2500}, &ldo);

/* 2. DSI bus: 2 lanes, 750 Mbps (Guition) */
esp_lcd_dsi_bus_handle_t bus;
esp_lcd_new_dsi_bus(&(esp_lcd_dsi_bus_config_t){
    .bus_id = 0, .num_data_lanes = 2,
    .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT, .lane_bit_rate_mbps = 750}, &bus);

/* 3. command channel */
esp_lcd_panel_io_handle_t io;
esp_lcd_new_panel_io_dbi(bus, &(esp_lcd_dbi_io_config_t){
    .virtual_channel = 0, .lcd_cmd_bits = 8, .lcd_param_bits = 8}, &io);

/* 4. video timing — spelled out; NOT ST7701_480_360_PANEL_60HZ_DPI_CONFIG */
const esp_lcd_dpi_panel_config_t dpi = {
    .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
    .dpi_clock_freq_mhz = 28,
    .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
    .num_fbs = 1,
    .video_timing = {
        .h_size = 480, .v_size = 800,
        .hsync_pulse_width = 12, .hsync_back_porch = 42, .hsync_front_porch = 42,
        .vsync_pulse_width = 2,  .vsync_back_porch = 8,  .vsync_front_porch = 166,
    },
    .flags.use_dma2d = true,
};
st7701_vendor_config_t vendor = {
    .init_cmds = s_init_cmds,
    .init_cmds_size = sizeof(s_init_cmds) / sizeof(s_init_cmds[0]),
    .mipi_config = {.dsi_bus = bus, .dpi_config = &dpi},
    .flags.use_mipi_interface = 1,
};

/* 5. panel, with reset on GPIO5 */
esp_lcd_new_panel_st7701(io, &(esp_lcd_panel_dev_config_t){
    .reset_gpio_num = 5, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
    .bits_per_pixel = 16, .vendor_config = &vendor}, &panel);
esp_lcd_panel_reset(panel);
esp_lcd_panel_init(panel);     /* logs "LCD ID: FF FF FF" — normal on this panel */
```

(The template uses `ESP_RETURN_ON_ERROR` around each call; stripped here for reading.)

An alternative timing set that ESPHome ships for the same board — 34 MHz pixel clock,
500 Mbps lanes, same porches — was not tried here.

## 5. Drawing without LVGL

```c
uint16_t *fb;
esp_lcd_dpi_panel_get_frame_buffer(panel, 1, (void **)&fb);   /* PSRAM, 480*800*2 B */

for (int y = 0; y < 800; y++)
    for (int x = 0; x < 480; x++)
        fb[y * 480 + x] = (y < 400) ? 0xF800 /* red */ : 0x001F /* blue */;

/* Same pointer as the framebuffer => the driver only writes the cache back.
 * Without this the panel keeps showing what was in PSRAM before. */
esp_lcd_panel_draw_bitmap(panel, 0, 0, 480, 800, fb);
```

For LVGL use `esp_lvgl_port` with the same `panel`/`io` handles and
`.flags.sw_rotate = true` if you need landscape — the panel has no hardware swap-XY.

## 6. Backlight

GPIO23 enables the MP3202 boost; PWM on it dims the backlight (verified at 100 % and
20 %). 10K pull-down: the panel is dark until this runs.

```c
#include "driver/ledc.h"

ledc_timer_config(&(ledc_timer_config_t){
    .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = LEDC_TIMER_10_BIT,
    .timer_num = LEDC_TIMER_0, .freq_hz = 5000, .clk_cfg = LEDC_AUTO_CLK});
ledc_channel_config(&(ledc_channel_config_t){
    .gpio_num = 23, .speed_mode = LEDC_LOW_SPEED_MODE, .channel = LEDC_CHANNEL_0,
    .timer_sel = LEDC_TIMER_0, .duty = 0});

void backlight_percent(int p)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 1023 * p / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}
```

Turn it on **after** the first frame is in the framebuffer, or the user sees a flash of
whatever PSRAM held.

## 7. Touch — GT911

From `template/src/touch.c`. `i2c_bus_handle()` is the one shared bus of §8.

```c
#include "esp_lcd_touch_gt911.h"

esp_lcd_touch_handle_t tp;

esp_lcd_panel_io_handle_t tp_io;
esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();  /* 0x5D */
io_cfg.scl_speed_hz = 400000;
esp_lcd_new_panel_io_i2c(i2c_bus_handle(), &io_cfg, &tp_io);

/* driver_data is what makes the driver run the full reset + address-latch
 * sequence (60 ms settle).  With RST/INT set but no driver_data it resets for
 * 10 ms and reads at once: "GT911 read error!", ESP_ERR_INVALID_STATE. */
static esp_lcd_touch_io_gt911_config_t gt911_cfg = {.dev_addr = 0x5D};
const esp_lcd_touch_config_t tp_cfg = {
    .x_max = 480, .y_max = 800,
    .rst_gpio_num = 22, .int_gpio_num = 21,
    .levels = {.reset = 0, .interrupt = 0},
    .driver_data = &gt911_cfg,
};
esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, &tp);

/* poll */
esp_lcd_touch_point_data_t pt[1];
uint8_t n = 0;
if (esp_lcd_touch_read_data(tp) == ESP_OK &&
    esp_lcd_touch_get_data(tp, pt, &n, 1) == ESP_OK && n) {
    /* pt[0].x in 0..479, pt[0].y in 0..799 — same orientation as the panel */
}
```

`esp_lcd_touch_get_coordinates()` still exists in 1.2.x but is deprecated; use
`esp_lcd_touch_get_data()`.

## 8. I2C bus, ES8311 ID, battery, die temperature

```c
/* The one bus.  ~1.5K external pull-ups; the pull-up warning that follows is harmless. */
i2c_master_bus_handle_t bus;
i2c_new_master_bus(&(i2c_master_bus_config_t){
    .i2c_port = 0, .sda_io_num = 7, .scl_io_num = 8,
    .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7}, &bus);
/* scan on a bare board: 0x14 0x18 0x5D */

/* ES8311 presence without a speaker: regs 0xFD/0xFE = 0x83 0x11 */
i2c_master_dev_handle_t es;
i2c_master_bus_add_device(bus, &(i2c_device_config_t){
    .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = 0x18, .scl_speed_hz = 100000}, &es);
uint8_t reg = 0xFD, id1, id2;
i2c_master_transmit_receive(es, &reg, 1, &id1, 1, 50);
reg = 0xFE;
i2c_master_transmit_receive(es, &reg, 1, &id2, 1, 50);
```

Battery (`template/src/battery.c`) — ADC2 channel 4 on GPIO53, 12 dB, curve-fitting
calibration, × 168 / 100 for the 68K/100K divider. Reads ~4200 mV **with no battery**
because the IP5306 drives BAT anyway:

```c
adc_oneshot_unit_handle_t adc;
adc_oneshot_new_unit(&(adc_oneshot_unit_init_cfg_t){.unit_id = ADC_UNIT_2}, &adc);
adc_oneshot_config_channel(adc, ADC_CHANNEL_4,
    &(adc_oneshot_chan_cfg_t){.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT});
adc_cali_handle_t cali;
adc_cali_create_scheme_curve_fitting(&(adc_cali_curve_fitting_config_t){
    .unit_id = ADC_UNIT_2, .chan = ADC_CHANNEL_4,
    .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT}, &cali);
int raw, mv;
adc_oneshot_read(adc, ADC_CHANNEL_4, &raw);
adc_cali_raw_to_voltage(cali, raw, &mv);
int vbat_mv = mv * 168 / 100;
```

Die temperature (`template/src/board_report.c`): the range must fit **one row** of the P4
table, `TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80)`. `(-10, 100)` fits none and
`temperature_sensor_install()` returns `ESP_ERR_INVALID_ARG` ("Out of testing range").

## 9. Wi-Fi through the ESP32-C6

**Run on the board**: found 9-15 networks per scan across five runs; with the C6's stored
credentials it also associated and got a DHCP lease. Separate project, not in the template.

`src/idf_component.yml` — the versions Guition's 5.5.4 demos pin. `esp_hosted` 2.0.17 (with
`esp_wifi_remote` 0.14.5) was tried and crashes with `netif already added`:

```yaml
dependencies:
  idf: ">=5.5"
  espressif/esp_wifi_remote: "*"        # resolved 1.6.5
  espressif/esp_hosted: "2.11.*"        # resolved 2.11.7
```

Add to `sdkconfig.defaults`:

```
CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y
CONFIG_ESP_HOSTED_IDF_SLAVE_TARGET="esp32c6"
CONFIG_ESP_HOSTED_SDIO_SLOT_1=y
CONFIG_ESP_HOSTED_SDIO_4_BIT_BUS=y
CONFIG_ESP_HOSTED_SDIO_CLOCK_FREQ_KHZ=40000
CONFIG_ESP_HOSTED_SDIO_PIN_CLK=18
CONFIG_ESP_HOSTED_SDIO_PIN_CMD=19
CONFIG_ESP_HOSTED_SDIO_PIN_D0=14
CONFIG_ESP_HOSTED_SDIO_PIN_D1=15
CONFIG_ESP_HOSTED_SDIO_PIN_D2=16
CONFIG_ESP_HOSTED_SDIO_PIN_D3=17
CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE=54
CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_HIGH=y
CONFIG_ESP_MAIN_TASK_STACK_SIZE=10240
```

`src/main.c`, exactly as run:

```c
#include <stdio.h>

#include "esp_event.h"
#include "esp_hosted.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define MAX_APS 20
static const char *TAG = "wifi";
static EventGroupHandle_t s_events;
static volatile uint32_t s_scan_status;

static void on_scan_done(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    s_scan_status = ((wifi_event_sta_scan_done_t *)data)->status; /* 0 = completed */
    xEventGroupSetBits(s_events, BIT0);
}

/* The C6 auto-connects with credentials stored in its OWN NVS some time after
 * esp_wifi_start().  While it does, a scan is either refused
 * (ESP_ERR_WIFI_STATE) or started and then aborted (SCAN_DONE with status 1
 * and no records).  Disconnect, let it settle, and retry until one completes. */
static esp_err_t scan_blocking(void)
{
    for (int attempt = 0; attempt < 5; attempt++) {
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(500));
        xEventGroupClearBits(s_events, BIT0);
        esp_err_t err = esp_wifi_scan_start(NULL, false);
        if (err == ESP_ERR_WIFI_STATE) continue; /* C6 busy connecting */
        if (err != ESP_OK) return err;
        if (!xEventGroupWaitBits(s_events, BIT0, pdTRUE, pdTRUE, pdMS_TO_TICKS(10000))) return ESP_ERR_TIMEOUT;
        if (s_scan_status == 0) return ESP_OK;
    }
    return ESP_FAIL;
}

void app_main(void)
{
    /* PlatformIO drops esp_hosted's WHOLE_ARCHIVE, so its self-starting
     * constructor is never linked.  Bring the SDIO link up by hand. */
    ESP_ERROR_CHECK(esp_hosted_init());
    ESP_ERROR_CHECK(esp_hosted_connect_to_slave());

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    s_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, on_scan_done, NULL));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(scan_blocking());
    ESP_LOGI(TAG, "scan complete");

    /* Over esp_wifi_remote, get_ap_records() does not shrink *number to the
     * real count — take the count from get_ap_num(). */
    uint16_t found = 0;
    esp_wifi_scan_get_ap_num(&found);
    uint16_t n = found < MAX_APS ? found : MAX_APS;
    static wifi_ap_record_t ap[MAX_APS];
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&n, ap));

    printf("Wi-Fi scan through the ESP32-C6: %u networks\n", found);
    for (int i = 0; i < n; i++) {
        printf("  %-32.32s  ch %2d  %4d dBm\n", (const char *)ap[i].ssid, ap[i].primary, ap[i].rssi);
    }
}
```

Expected in the log every boot, harmless: `Version mismatch: Host [2.11.0] > Co-proc
[2.3.0]` and a burst of `esp_cli: Registering command: …` lines from esp_hosted.

To connect, use the ordinary `esp_wifi_set_config()` + `esp_wifi_connect()` with an
`IP_EVENT_STA_GOT_IP` handler. If you do *not* want the C6 to reuse credentials it already
holds, set your own config (or an empty one) before connecting — the credentials live on
the C6, not in the P4's NVS.

## 10. A partition table that uses the 16 MB

`partitions_singleapp_large.csv` gives a 1 500 KB app. `partitions.csv` in the project
root, `board_build.partitions = partitions.csv` in `platformio.ini`, and
`CONFIG_PARTITION_TABLE_CUSTOM=y` + `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"`
in `sdkconfig.defaults`. Read back on the board as `factory @0x10000 6144 KB, storage
@0x610000 9216 KB`:

```csv
# name,   type, subtype,  offset,   size,  flags
nvs,      data, nvs,      0x9000,   0x6000,
phy_init, data, phy,      0xf000,   0x1000,
factory,  app,  factory,  0x10000,  0x600000,
storage,  data, spiffs,   ,         0x900000,
```

## 11. microSD — SDMMC slot 0, card powered by LDO 4

**Run with no card inserted**: it reaches `sdmmc_init_ocr: send_op_cond (1) returned
0x107` → `ESP_ERR_TIMEOUT`, the same failure the factory firmware shows without a card.
Mounting a real card was not tested.

```c
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"

static sdmmc_card_t *s_card;

esp_err_t sdcard_mount(void)
{
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, .max_files = 5, .allocation_unit_size = 64 * 1024,
    };
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;             /* IO MUX slot: GPIO39-44 */
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    /* TF_VCC comes from the P4's on-chip LDO channel 4.  Without this the card
     * is unpowered and the mount times out exactly like "no card".  The driver
     * logs "ldo: The voltage value 0 is out of the recommended range" here —
     * the factory firmware does too. */
    sd_pwr_ctrl_handle_t pwr = NULL;
    ESP_RETURN_ON_ERROR(sd_pwr_ctrl_new_on_chip_ldo(
        &(sd_pwr_ctrl_ldo_config_t){.ldo_chan_id = 4}, &pwr), "sd", "LDO4");
    host.pwr_ctrl_handle = pwr;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();  /* slot 0: no pins to set */
    slot.width = 4;
    slot.cd = SDMMC_SLOT_NO_CD;
    slot.wp = SDMMC_SLOT_NO_WP;
    return esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot, &mount_config, &s_card);
}
```

## 12. Audio — ES8311, NS4150 and the on-board microphone

**Run on the board**: `esp_codec_dev` 1.5.11 opens the codec (`Open codec device OK`), a
440 Hz tone is written without error, and the mic returns samples. **No speaker was
attached, so nothing was heard**, and the mic was not tested against a known sound — it
shows a noise floor of RMS ≈ 18 (16-bit) at 30 dB gain, **left channel only** (right is 0).

`src/idf_component.yml`: `espressif/esp_codec_dev: "~1.5"`.

```c
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

/* I2S0, host's point of view: MCLK 13, BCLK 12, LRCK 10, DOUT 9, DIN 48 */
i2s_chan_handle_t tx, rx;
i2s_chan_config_t ch = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
ch.auto_clear = true;
i2s_new_channel(&ch, &tx, &rx);
i2s_std_config_t std = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {.mclk = 13, .bclk = 12, .ws = 10, .dout = 9, .din = 48},
};
std.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
i2s_channel_init_std_mode(tx, &std);
i2s_channel_init_std_mode(rx, &std);
i2s_channel_enable(tx);
i2s_channel_enable(rx);

/* ES8311_CODEC_DEFAULT_ADDR is 0x30 — the 8-bit (shifted) form of 0x18, which
 * is what esp_codec_dev expects.  Do not "fix" it to the 7-bit 0x18 a scan shows. */
audio_codec_i2c_cfg_t i2c_cfg = {.port = 0, .addr = ES8311_CODEC_DEFAULT_ADDR, .bus_handle = bus};
const audio_codec_ctrl_if_t *ctrl = audio_codec_new_i2c_ctrl(&i2c_cfg);
es8311_codec_cfg_t es_cfg = {
    .ctrl_if = ctrl,
    .gpio_if = audio_codec_new_gpio(),
    .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
    .pa_pin = 11,                    /* NS4150 enable, active high — driver toggles it */
    .use_mclk = true,
    .hw_gain = {.pa_voltage = 5.0, .codec_dac_voltage = 3.3},
};
const audio_codec_if_t *es = es8311_codec_new(&es_cfg);
const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(
    &(audio_codec_i2s_cfg_t){.port = I2S_NUM_0, .rx_handle = rx, .tx_handle = tx});
esp_codec_dev_handle_t dev = esp_codec_dev_new(&(esp_codec_dev_cfg_t){
    .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT, .codec_if = es, .data_if = data_if});

esp_codec_dev_open(dev, &(esp_codec_dev_sample_info_t){
    .sample_rate = 16000, .channel = 2, .bits_per_sample = 16});
esp_codec_dev_set_out_vol(dev, 60);
esp_codec_dev_set_in_gain(dev, 30.0);

esp_codec_dev_write(dev, pcm, bytes);   /* interleaved stereo int16 */
esp_codec_dev_read(dev, pcm, bytes);    /* mic lands in the LEFT samples */
```

`bus` is the shared I2C handle from §8. One I2S port carries both directions, so playback
and recording share a clock — the ESPHome community reports that wake-word listening stops
while music plays on this board.

## 13. Arduino

Not run in this bring-up. From Guition's archive and its settings screenshot:

| Setting | Value |
|---|---|
| Board | **ESP32P4 Dev Module** |
| arduino-esp32 | **3.2.1** for rev v1.3 boards; **3.3.11** for rev v3.x (`P4_V3X_DEMO`) |
| CPU | 360 MHz |
| Flash | QIO, 80 MHz, 16 MB |
| Partition scheme | 16M Flash (3MB APP / 9.9MB FATFS) |
| PSRAM | Enabled |
| USB Mode | USB-OTG (TinyUSB) as shipped; upload over "UART0 / Hardware CDC" |

Guition's Arduino display code vendors the same patched `esp_lcd_st7701` into the sketch's
`src/lcd` (reset GPIO5 there, although `pins_config.h` says `LCD_RST -1`). The same rule
applies: the init table travels with the vendored driver; a registry or library-manager
ST7701 driver will not have it. Every pin in `board-hardware.md` §2 applies unchanged.
