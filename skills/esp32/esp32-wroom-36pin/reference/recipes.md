# Recipes — ESP32-WROOM-32 devkit (36-pin), ESP-IDF

Code that is known to build. Everything below was compiled against **ESP-IDF 6.0.1 /
platform-espressif32 7.0.1 / PlatformIO 6.1.19** for `board = esp32dev`, with zero
warnings. Nothing here is retyped from memory.

`§1–§5` come straight out of `template/`. `§6` onward were compiled in the same project as
a check harness and then extracted; they are **compile-verified, not hardware-verified**
unless noted.

Pin constants come from `include/board.h`, which is the only file that differs between the
30-, 36- and 38-pin skills. Use the constants rather than literals, and moving a project
between board sizes stays a one-file change.

---

## 1. `platformio.ini`

```ini
[env:esp32dev]
platform  = espressif32@7.0.1
board     = esp32dev
framework = espidf

; 4 MB is what every ESP32-WROOM-32 module carries. Both lines are needed:
; board_upload feeds esptool, the sdkconfig symbol feeds the bootloader image header.
board_upload.flash_size = 4MB
board_build.partitions  = partitions.csv

monitor_speed   = 115200
monitor_filters = esp32_exception_decoder, time
upload_speed    = 921600

build_flags =
    -DBOARD_HAS_PSRAM=0
```

`monitor_filters = esp32_exception_decoder` turns a raw backtrace into file:line. On a
board with no debugger it is the single most useful line in the file.

If uploads fail at 921600 (some CH340G clones), drop `upload_speed` to `460800`.

## 2. `sdkconfig.defaults`

```ini
CONFIG_IDF_TARGET="esp32"
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_ESPTOOLPY_FLASHMODE_DIO=y
CONFIG_ESPTOOLPY_FLASHFREQ_40M=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_ESP_MAIN_TASK_STACK_SIZE=6144
CONFIG_ESP_CONSOLE_UART_DEFAULT=y
CONFIG_ESP_CONSOLE_UART_BAUDRATE=115200
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
CONFIG_ESP32_XTAL_FREQ_40=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_BROWNOUT_DET_LVL_SEL_7=y
```

Reasoning for each line is in `board-hardware.md` §11. The two that are *not* what an
ESP32-C3/C6 project would use are `CONFIG_ESP_CONSOLE_UART_DEFAULT` (this chip has no USB
peripheral) and `CONFIG_ESP32_XTAL_FREQ_40`.

## 3. `partitions.csv`

```csv
# name,   type, subtype,  offset,   size
nvs,      data, nvs,      0x9000,   0x6000
phy_init, data, phy,      0xf000,   0x1000
factory,  app,  factory,  0x10000,  0x1C0000
storage,  data, spiffs,   0x1D0000, 0x230000
```

For OTA instead:

```csv
nvs,      data, nvs,      0x9000,   0x6000
otadata,  data, ota,      0xf000,   0x2000
phy_init, data, phy,      0x11000,  0x1000
ota_0,    app,  ota_0,    0x20000,  0x180000
ota_1,    app,  ota_1,    0x1A0000, 0x180000
nvs_key,  data, nvs_keys, 0x320000, 0x1000
storage,  data, spiffs,   0x321000, 0xDF000
```

## 4. Chip, reset and strapping report

From `template/src/board_report.c`. The strapping decode is the part worth copying — the
bit order is not the one you would guess.

```c
#include "esp_chip_info.h"
#include "esp_system.h"
#include "soc/gpio_reg.h"

/* ESP32 TRM Register 6.13: GPIO_STRAP_REG bit5..bit0 are
 * MTDI, GPIO0, GPIO2, GPIO4, MTDO, GPIO5 — high bit first. */
uint32_t strap = REG_READ(GPIO_STRAP_REG);
int mtdi  = (strap >> 5) & 1;   /* GPIO12 — 1 means VDD_SDIO strapped to 1.8 V: BAD */
int gpio0 = (strap >> 4) & 1;   /* 1 = SPI boot, 0 = download mode */
int gpio2 = (strap >> 3) & 1;
int gpio4 = (strap >> 2) & 1;
int mtdo  = (strap >> 1) & 1;   /* GPIO15 — 1 = ROM log on U0TXD */
int gpio5 = (strap >> 0) & 1;
```

```c
esp_chip_info_t info;
esp_chip_info(&info);
/* revision is packed: v3.0 silicon reports 300 */
printf("ESP32 rev v%d.%d, %d cores\n", info.revision / 100, info.revision % 100, info.cores);

switch (esp_reset_reason()) {
case ESP_RST_BROWNOUT:
    /* On a devkit this is the supply, not the firmware. */
    break;
case ESP_RST_DEEPSLEEP:
    break;
default:
    break;
}
```

## 5. ADC1 oneshot with calibration

From `template/src/analog.c`. **ADC1, never ADC2** — ADC2 returns `ESP_ERR_TIMEOUT` for as
long as the Wi-Fi driver is started.

```c
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"

static adc_oneshot_unit_handle_t s_unit;
static adc_cali_handle_t         s_cali;   /* NULL if the chip has no eFuse Vref */

adc_oneshot_unit_init_cfg_t unit_cfg = {
    .unit_id  = ADC_UNIT_1,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
};
ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_unit));

/* 12 dB = the full ~150-2450 mV window. Above ~2450 mV the transfer function
 * flattens, so a divider from 3.3 V or 4.2 V must aim its top at ~2.4 V. */
adc_oneshot_chan_cfg_t chan_cfg = {
    .atten    = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_DEFAULT,
};
ESP_ERROR_CHECK(adc_oneshot_config_channel(s_unit, ADC_CHANNEL_6 /* GPIO34 */, &chan_cfg));

/* Line fitting is the ONLY scheme the original ESP32 has.
 * adc_cali_create_scheme_curve_fitting is S2/S3/C3 and will not link here. */
adc_cali_line_fitting_config_t cali_cfg = {
    .unit_id  = ADC_UNIT_1,
    .atten    = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_DEFAULT,
};
if (adc_cali_create_scheme_line_fitting(&cali_cfg, &s_cali) != ESP_OK) {
    s_cali = NULL;   /* uncalibrated: ±6 % chip-to-chip */
}

int raw = 0, mv = -1;
ESP_ERROR_CHECK(adc_oneshot_read(s_unit, ADC_CHANNEL_6, &raw));
if (s_cali) adc_cali_raw_to_voltage(s_cali, raw, &mv);
```

Channel ↔ GPIO for ADC1: CH0→36(VP), CH3→39(VN), CH4→32, CH5→33, CH6→34, CH7→35.
CH1 and CH2 are GPIO37/38, which the WROOM-32 package does not bond out.

DNL is ±7 LSB, so average 8–16 samples before believing a reading.

## 6. LEDC — LED fade and hobby servo

The ESP32 has 16 LEDC channels: 8 high-speed and 8 low-speed, 4 timers each. Only
low-speed channels survive light sleep, so `LEDC_LOW_SPEED_MODE` is the safe default.

LED (from `template/src/main.c`):

```c
#include "driver/ledc.h"

ledc_timer_config_t t = {
    .speed_mode      = LEDC_LOW_SPEED_MODE,
    .timer_num       = LEDC_TIMER_0,
    .duty_resolution = LEDC_TIMER_10_BIT,
    .freq_hz         = 5000,
    .clk_cfg         = LEDC_AUTO_CLK,
};
ESP_ERROR_CHECK(ledc_timer_config(&t));

ledc_channel_config_t c = {
    .gpio_num   = BOARD_LED_GPIO,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel    = LEDC_CHANNEL_0,
    .timer_sel  = LEDC_TIMER_0,
    .duty = 0, .hpoint = 0,
};
ESP_ERROR_CHECK(ledc_channel_config(&c));

ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);   /* nothing happens without this */
```

Servo — 50 Hz, and the resolution has to be high enough that a microsecond is
representable. 14 bits over a 20 ms period gives 1.22 µs per step:

```c
ledc_timer_config_t t = {
    .speed_mode      = LEDC_LOW_SPEED_MODE,
    .timer_num       = LEDC_TIMER_1,
    .duty_resolution = LEDC_TIMER_14_BIT,
    .freq_hz         = 50,
    .clk_cfg         = LEDC_AUTO_CLK,
};
ESP_ERROR_CHECK(ledc_timer_config(&t));

ledc_channel_config_t c = {
    .gpio_num = 13, .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_1, .timer_sel = LEDC_TIMER_1, .duty = 0, .hpoint = 0,
};
ESP_ERROR_CHECK(ledc_channel_config(&c));

uint32_t us   = 1500;                                        /* 500–2500 typical */
uint32_t duty = (uint32_t)((uint64_t)us * (1u << 14) / 20000u);
ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);
ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
```

`duty_resolution` and `freq_hz` are constrained by each other: `freq × 2^bits ≤ 80 MHz`.
`ledc_timer_config()` returns `ESP_ERR_INVALID_ARG` if you ask for a combination the
divider cannot make — check the return value, it is a common silent failure.

Do not drive a servo's power from the board's 3V3 pin. The stall current exceeds what the
AMS1117 can give and browns out the ESP32.

## 7. Debounced input

```c
gpio_config_t c = {
    .pin_bit_mask = 1ULL << BOARD_BOOT_BTN_GPIO,
    .mode         = GPIO_MODE_INPUT,
    .pull_up_en   = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type    = GPIO_INTR_DISABLE,
};
ESP_ERROR_CHECK(gpio_config(&c));

/* Call every ~10 ms. Returns true only on a level that has held for 3 polls. */
bool button_stable_pressed(gpio_num_t pin)
{
    static int stable, last;
    int now = gpio_get_level(pin);
    if (now != last) { last = now; stable = 0; return false; }
    if (stable < 3) { stable++; return false; }
    return now == 0;
}
```

`GPIO_PULLUP_ENABLE` does **nothing** on GPIO34/35/36/39 — those pads have no internal
pulls. `gpio_config()` still returns `ESP_OK`, which is why this bites.

## 8. I2C master — the new driver

ESP-IDF 5.2 replaced `driver/i2c.h` with `driver/i2c_master.h`. Old tutorials use
`i2c_param_config()` + `i2c_driver_install()`, which still exist but are deprecated and
will not survive the next major version.

```c
#include "driver/i2c_master.h"

static i2c_master_bus_handle_t s_bus;

i2c_master_bus_config_t bus = {
    .i2c_port   = I2C_NUM_0,
    .sda_io_num = BOARD_I2C_SDA_GPIO,      /* GPIO21 */
    .scl_io_num = BOARD_I2C_SCL_GPIO,      /* GPIO22 */
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,  /* 45 kΩ — enough for a short bus only */
};
ESP_ERROR_CHECK(i2c_new_master_bus(&bus, &s_bus));

/* Bus scan — the first thing to run when a sensor "does not respond". */
for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    if (i2c_master_probe(s_bus, addr, 50) == ESP_OK) {
        printf("i2c device at 0x%02x\n", addr);
    }
}
```

Adding a device and talking to it:

```c
i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address  = 0x76,
    .scl_speed_hz    = 400000,
};
i2c_master_dev_handle_t dev;
ESP_ERROR_CHECK(i2c_master_bus_add_device(s_bus, &dev_cfg, &dev));

uint8_t reg = 0xD0, id = 0;
ESP_ERROR_CHECK(i2c_master_transmit_receive(dev, &reg, 1, &id, 1, 100));
```

The internal 45 kΩ pull-ups are weak. Anything longer than a few centimetres, or faster
than 100 kHz, wants external 4.7 kΩ resistors to 3V3. A bus that scans clean at 100 kHz and
fails at 400 kHz is a pull-up problem, not a driver problem.

## 9. SPI master at 80 MHz

**The pin choice is the whole recipe.** `SPI3_HOST` on GPIO18/19/23/5 uses SPI3's IO_MUX
pads and bypasses the GPIO Matrix; anything else is matrix-routed and the driver clamps
full-duplex transfers to 26.67 MHz. See `esp32-soc.md` §8.

```c
#include "driver/spi_master.h"

spi_bus_config_t bus = {
    .mosi_io_num   = BOARD_SPI_MOSI_GPIO,   /* 23 */
    .miso_io_num   = BOARD_SPI_MISO_GPIO,   /* 19 */
    .sclk_io_num   = BOARD_SPI_SCLK_GPIO,   /* 18 */
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4092,                /* raise for DMA'd framebuffers */
};
ESP_ERROR_CHECK(spi_bus_initialize(BOARD_SPI_HOST_DEFAULT /* SPI3_HOST */,
                                   &bus, SPI_DMA_CH_AUTO));

spi_device_interface_config_t dev = {
    .clock_speed_hz = 80 * 1000 * 1000,
    .mode           = 0,
    .spics_io_num   = BOARD_SPI_CS_GPIO,    /* 5 */
    .queue_size     = 4,
};
static spi_device_handle_t s_dev;
ESP_ERROR_CHECK(spi_bus_add_device(BOARD_SPI_HOST_DEFAULT, &dev, &s_dev));

/* Ask what you actually got. If this prints 26666 you are on the GPIO Matrix. */
int actual_khz = 0;
ESP_ERROR_CHECK(spi_device_get_actual_freq(s_dev, &actual_khz));

uint8_t tx[4] = { 0x9F, 0, 0, 0 }, rx[4] = { 0 };
spi_transaction_t t = { .length = 8 * sizeof(tx), .tx_buffer = tx, .rx_buffer = rx };
ESP_ERROR_CHECK(spi_device_polling_transmit(s_dev, &t));
```

`spi_device_get_actual_freq()` is the diagnostic worth wiring into any project that cares
about throughput — the driver silently gives you the nearest achievable divider, so an
80 MHz request can come back as 26.67 MHz with no error.

A write-only device (most displays) sets `.miso_io_num = -1` and is not bound by the
read-timing limit, so it can be pushed past 26.67 MHz even on matrix pins.

For a second device, add another `spi_bus_add_device()` with a different `spics_io_num` on
the **same** bus. A second `spi_bus_initialize()` on a host that is already up returns
`ESP_ERR_INVALID_STATE`.

## 10. UART2

UART1's ROM defaults are GPIO9 and GPIO10 — flash pins. Use UART2, or remap UART1 before
installing the driver.

```c
#include "driver/uart.h"

const uart_port_t port = UART_NUM_2;
uart_config_t cfg = {
    .baud_rate  = 115200,
    .data_bits  = UART_DATA_8_BITS,
    .parity     = UART_PARITY_DISABLE,
    .stop_bits  = UART_STOP_BITS_1,
    .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};
ESP_ERROR_CHECK(uart_param_config(port, &cfg));
ESP_ERROR_CHECK(uart_set_pin(port, BOARD_UART1_TX_GPIO /* 17 */,
                                   BOARD_UART1_RX_GPIO /* 16 */,
                                   UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
ESP_ERROR_CHECK(uart_driver_install(port, 1024, 0, 0, NULL, 0));

uart_write_bytes(port, "hello\n", 6);
uint8_t buf[64];
int n = uart_read_bytes(port, buf, sizeof(buf), pdMS_TO_TICKS(20));
```

`uart_set_pin()` must come before `uart_driver_install()`. The reverse order compiles, runs,
and quietly leaves the peripheral on its default pins.

Do not reuse UART0 (GPIO1/3) for a peripheral — it is the console and the flashing route.

## 11. Wi-Fi

Scan (from `template/src/wifi_scan.c`):

```c
ESP_ERROR_CHECK(esp_netif_init());
esp_err_t err = esp_event_loop_create_default();
if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
esp_netif_create_default_wifi_sta();

wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
ESP_ERROR_CHECK(esp_wifi_init(&cfg));
ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
ESP_ERROR_CHECK(esp_wifi_start());
ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));   /* blocking */

uint16_t count = 16;
static wifi_ap_record_t records[16];
ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&count, records));
```

Station connect:

```c
#include "freertos/event_groups.h"

static EventGroupHandle_t s_evt;
#define WIFI_OK   BIT0
#define WIFI_FAIL BIT1

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if      (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)        esp_wifi_connect();
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) xEventGroupSetBits(s_evt, WIFI_FAIL);
    else if (base == IP_EVENT   && id == IP_EVENT_STA_GOT_IP)         xEventGroupSetBits(s_evt, WIFI_OK);
}

s_evt = xEventGroupCreate();
ESP_ERROR_CHECK(esp_netif_init());
ESP_ERROR_CHECK(esp_event_loop_create_default());
esp_netif_create_default_wifi_sta();

wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
ESP_ERROR_CHECK(esp_wifi_init(&cfg));
ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,   on_wifi, NULL, NULL));
ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi, NULL, NULL));

wifi_config_t wc = { 0 };
strlcpy((char *)wc.sta.ssid,     ssid, sizeof(wc.sta.ssid));
strlcpy((char *)wc.sta.password, pass, sizeof(wc.sta.password));
ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
ESP_ERROR_CHECK(esp_wifi_start());

EventBits_t b = xEventGroupWaitBits(s_evt, WIFI_OK | WIFI_FAIL, pdFALSE, pdFALSE,
                                    pdMS_TO_TICKS(15000));
```

`nvs_flash_init()` must have succeeded before `esp_wifi_init()` — the Wi-Fi driver stores
calibration data in NVS and aborts otherwise.

Two board-level consequences of turning the radio on:

- **ADC2 stops working** until `esp_wifi_stop()`. Move analog inputs to ADC1.
- **The board draws ~300 mA in bursts.** On a devkit this is the single most common cause
  of `ESP_RST_BROWNOUT`, and it appears at the first transmission, not at boot.

To halve the average current without losing the connection:

```c
esp_wifi_set_ps(WIFI_PS_MIN_MODEM);   /* the default; MAX_MODEM saves more, adds latency */
```

## 12. NVS

```c
#include "nvs.h"
#include "nvs_flash.h"

esp_err_t err = nvs_flash_init();
if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    /* First boot after a partition-table change, or a truncated write. */
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
}
ESP_ERROR_CHECK(err);

nvs_handle_t h;
ESP_ERROR_CHECK(nvs_open("board", NVS_READWRITE, &h));
uint32_t n = 0;
nvs_get_u32(h, "boots", &n);          /* leaves n untouched and returns NOT_FOUND first time */
nvs_set_u32(h, "boots", ++n);
nvs_commit(h);                        /* without this the write is lost on reset */
nvs_close(h);
```

The erase-and-retry branch is not optional. Changing `partitions.csv` moves the NVS
partition and the old contents become unreadable; without the retry the app aborts at boot
and looks bricked.

## 13. Deep sleep

Only **RTC GPIOs** can wake the chip: `0, 2, 4, 12, 13, 14, 15, 25, 26, 27, 32, 33, 34, 35,
36, 39`. GPIO1, 3, 5, 16, 17, 18, 19, 21, 22, 23 cannot, whatever you configure.

```c
#include "driver/rtc_io.h"
#include "esp_sleep.h"

const gpio_num_t WAKE = GPIO_NUM_33;

/* The digital GPIO pull registers are powered down in deep sleep — the pull
 * must be set through the RTC IO driver or the pin floats and wakes randomly. */
ESP_ERROR_CHECK(rtc_gpio_init(WAKE));
ESP_ERROR_CHECK(rtc_gpio_set_direction(WAKE, RTC_GPIO_MODE_INPUT_ONLY));
ESP_ERROR_CHECK(rtc_gpio_pullup_en(WAKE));
ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(WAKE));

ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(WAKE, 0));          /* wake on low */
ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(30ULL * 1000000)); /* or after 30 s */

/* Outputs float during deep sleep unless held. A relay or MOSFET gate needs both. */
gpio_hold_en(GPIO_NUM_25);
gpio_deep_sleep_hold_en();

esp_deep_sleep_start();   /* does not return */
```

After the wake, `esp_reset_reason()` is `ESP_RST_DEEPSLEEP` and
`esp_sleep_get_wakeup_cause()` says which source fired. Variables marked `RTC_DATA_ATTR`
live in the 8 KB RTC FAST memory and survive.

Expect **8–20 mA**, not 10 µA, on any devkit: the LDO's quiescent current, the USB bridge
chip and the power LED all stay powered. The 10 µA datasheet number is the bare chip.

`ext1` wakes on a mask of RTC GPIOs and is the one to use for several buttons:

```c
esp_sleep_enable_ext1_wakeup((1ULL << 33) | (1ULL << 32), ESP_EXT1_WAKEUP_ALL_LOW);
```

## 14. WS2812 / NeoPixel over RMT

Bit-banging WS2812 on a FreeRTOS system produces flicker whenever a task preempts you. RMT
generates the waveform in hardware.

```c
#include "driver/rmt_tx.h"

static rmt_channel_handle_t s_chan;
static rmt_encoder_handle_t s_enc;

rmt_tx_channel_config_t ch = {
    .gpio_num          = 4,
    .clk_src           = RMT_CLK_SRC_DEFAULT,
    .resolution_hz     = 10 * 1000 * 1000,   /* 100 ns per tick */
    .mem_block_symbols = 64,
    .trans_queue_depth = 4,
};
ESP_ERROR_CHECK(rmt_new_tx_channel(&ch, &s_chan));

/* WS2812B: T0H 0.3 µs / T0L 0.9 µs, T1H 0.9 µs / T1L 0.3 µs, at 100 ns per tick. */
rmt_bytes_encoder_config_t bytes = {
    .bit0 = { .level0 = 1, .duration0 = 3, .level1 = 0, .duration1 = 9 },
    .bit1 = { .level0 = 1, .duration0 = 9, .level1 = 0, .duration1 = 3 },
    .flags.msb_first = 1,
};
ESP_ERROR_CHECK(rmt_new_bytes_encoder(&bytes, &s_enc));
ESP_ERROR_CHECK(rmt_enable(s_chan));

uint8_t grb[3] = { 0x10, 0x00, 0x00 };   /* GRB order, not RGB */
rmt_transmit_config_t tx = { .loop_count = 0 };
ESP_ERROR_CHECK(rmt_transmit(s_chan, s_enc, grb, sizeof(grb), &tx));
ESP_ERROR_CHECK(rmt_tx_wait_all_done(s_chan, -1));
```

Two things that are always wrong the first time: the wire order is **GRB**, and a strip of
any length needs its own 5 V supply — the board's 3V3 pin cannot feed even a handful of
pixels at full brightness.

## 15. Moving a project between board sizes

Only `include/board.h` differs between the 30-, 36- and 38-pin skills. If code uses
`BOARD_*` constants rather than pin literals, porting is a single file swap. What actually
changes:

| | 30-pin | 36-pin | 38-pin |
|---|---|---|---|
| `BOARD_HEADER_PINS` | 30 | 36 | 38 |
| `BOARD_EXPOSED_GPIOS` | 25 | 31 | 32 |
| GPIO0 on the header | no | no | **yes** |
| GPIO6–11 on the header | no | yes (unusable) | yes (unusable) |
| `BOARD_FLASH_GPIOS` defined | no | yes | yes |
| User LED | GPIO2 | GPIO2 | GPIO2, **absent on Espressif DevKitC V4** |

The *usable* GPIO set is identical on all three: `4 13 14 16 17 18 19 21 22 23 25 26 27 32
33` free, `34 35 36 39` input-only, `2 5 12 15` strapping, `1 3` UART0. A design that fits
a 30-pin board fits all three.
