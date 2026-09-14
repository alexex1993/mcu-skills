# JC-ESP32P4-M3-DEV recipes

Copy-paste ready. §§1-4 and §12 are extracted from `template/` and are **verified to
build** with PlatformIO Core 6.2.0 + pioarduino 55.03.311 (ESP-IDF 5.5.5). §§5-11 are
assembled from Guition's own demos and the schematic and are marked where they were not
compiled in this session — say so when you hand them to a user.

Pin constants are the ones in `template/include/board_pins.h`.

---

## 1. `platformio.ini`

```ini
[env:jc-esp32p4-m3-dev]
platform  = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip
board     = esp32-p4-evboard      ; rev < 3.0.  esp32-p4_r3-evboard for rev >= 3.0
framework = espidf

board_build.flash_mode  = qio
board_upload.flash_size = 16MB
board_build.partitions  = partitions_singleapp_large.csv

upload_speed    = 921600
monitor_speed   = 115200
monitor_filters = direct, esp32_exception_decoder, time

build_flags = -DBLINK_GPIO=20
```

`platform = espressif32` does not work: the official platform has no ESP32-P4 board and no
P4 support at all. The URL is not decoration.

## 2. `sdkconfig.defaults`

```ini
# Silicon revision — without these two the bootloader dies with
# "Guru Meditation Error: Illegal instruction" right after "entry 0x...".
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_100=y

CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y

# Console on the native USB-Serial/JTAG socket (303A:1001)
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y

# 32 MB in-package PSRAM: HEX (16-line), 200 MHz
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_HEX=y
CONFIG_SPIRAM_SPEED_200M=y
```

For a **rev ≥ 3.0** board, drop the first two lines and use `CONFIG_ESP32P4_REV_MIN_301=y`.

To put the console on the CH340C socket instead, replace the console line with
`CONFIG_ESP_CONSOLE_UART_DEFAULT=y` (UART0 is GPIO37/38 and is already wired to it).

## 3. `src/CMakeLists.txt` that works for both variants

```cmake
file(GLOB app_sources ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
idf_component_register(SRCS ${app_sources}
                       INCLUDE_DIRS "." "../include")
```

Globbing keeps `--full` and `--minimal` on one file: the minimal variant is the same tree
with the extra `.c` files deleted.

## 4. Which silicon am I on, from firmware

```c
#include "esp_chip_info.h"

esp_chip_info_t chip;
esp_chip_info(&chip);
/* revision is in hundredths: 103 == "v1.3".  < 300 is the pre-rev.3 part. */
printf("ESP32-P4 rev v%d.%d, %d cores\n",
       chip.revision / 100, chip.revision % 100, chip.cores);
```

And PSRAM, which is **not** reported through `chip.features` on the P4 — the
`CHIP_FEATURE_EMB_PSRAM` bit stays clear even though the die is in the package:

```c
#include "esp_psram.h"
size_t psram = esp_psram_get_size();      /* 33554432 on this board */
```

If `esp_psram_get_size` fails to *link*, `CONFIG_SPIRAM` is not set — the whole PSRAM
component is compiled out, so the error is a linker error, not a runtime zero.

## 5. microSD — SDMMC slot 0, with the on-chip LDO

**Not compiled in this session.** Assembled from the BSP Guition ships unmodified
(`espressif__esp32_p4_function_ev_board`) plus the schematic.

```c
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"

static sdmmc_card_t *s_card;

esp_err_t sdcard_mount(void)
{
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 64 * 1024,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;              /* IO MUX slot: GPIO39-44 */
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    /* TF_VCC comes from the P4's OWN LDO channel 4.  Skip this and the card is
     * simply unpowered: the mount times out and looks like a bad card. */
    sd_pwr_ctrl_ldo_config_t ldo_config = { .ldo_chan_id = 4 };
    sd_pwr_ctrl_handle_t pwr = NULL;
    ESP_RETURN_ON_ERROR(sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr),
                        "sd", "on-chip LDO for the SD card failed");
    host.pwr_ctrl_handle = pwr;

    /* Slot 0 is an IO MUX slot, so the pins are NOT passed here — that is not an
     * omission.  There is no card-detect and no write-protect line on this board. */
    const sdmmc_slot_config_t slot_config = {
        .cd = SDMMC_SLOT_NO_CD,
        .wp = SDMMC_SLOT_NO_WP,
        .width = 4,
        .flags = 0,
    };

    return esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &s_card);
}
```

In Arduino (`framework = arduino`), Guition's own MP3 demo does the same slot with
`SD_MMC`, **calling `setPins()` before `begin()`** — after `begin()` it returns true and
changes nothing:

```cpp
SD_MMC.setPins(43 /*CLK*/, 44 /*CMD*/, 39, 40, 41, 42);
if (!SD_MMC.begin()) { /* card mount failed */ }
```

## 6. I2C — the one shared bus

Extracted from `template/src/i2c_scan.c`, which builds.

```c
#include "driver/i2c_master.h"

i2c_master_bus_config_t bus_cfg = {
    .i2c_port = I2C_NUM_0,
    .sda_io_num = 7,
    .scl_io_num = 8,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags = { .enable_internal_pullup = false },   /* 5.1K on board */
};
i2c_master_bus_handle_t bus;
ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

for (uint8_t a = 0x08; a < 0x78; a++) {
    if (i2c_master_probe(bus, a, 50) == ESP_OK) {
        printf("0x%02x\n", a);
    }
}
```

Expected on a bare board: **`0x18`** only (the ES8311). With Guition's 10.1" DSI panel
attached you also get **`0x45`** (brightness) and **`0x5D`** or **`0x14`** (GT911 touch).

## 7. Audio — ES8311 and the NS4150

**Not compiled in this session.** Pins are schematic-confirmed; the codec register
sequence is Espressif's `esp_codec_dev`, not retyped here.

```c
/* ALWAYS FIRST: the amplifier is muted by a 10K pull-down until this line. */
gpio_set_direction(GPIO_NUM_11, GPIO_MODE_OUTPUT);
gpio_set_level(GPIO_NUM_11, 1);          /* PA_CTRL, active high */

/* I2S0, standard Philips, from the HOST's point of view:
 *   MCLK 13   BCLK 12   WS/LRCK 10   DOUT 9 (-> speaker)   DIN 48 (<- microphone)
 * The schematic names these from the CODEC's side, where DOUT reads as DSDIN.
 * The net on GPIO48 is called ES7210_SDOUT and there is no ES7210 on this board;
 * it is the ES8311's own ADC output. */
```

For ESP-IDF, add `espressif/esp_codec_dev` and use its ES8311 driver on the I2C bus of §6
at address `0x18`. For Arduino, Guition's MP3 demo uses `pschatzmann/arduino-audio-driver`
with `AudioDriverES8311` plus `schreibfaul1/ESP32-audioI2S`:

```cpp
my_pins.addI2C(PinFunction::CODEC, 8 /*SCL*/, 7 /*SDA*/, 0x18);
my_pins.addPin(PinFunction::PA, 11, PinLogic::Output);
audio.setPinout(12 /*BCK*/, 10 /*WS*/, 9 /*DOUT*/, 13 /*MCLK*/);
```

## 8. MIPI-DSI panel

**Not compiled in this session.** From Guition's xiaozhi board file, whose numbers match
the schematic and Espressif's BSP.

```c
#include "esp_ldo_regulator.h"
#include "esp_lcd_mipi_dsi.h"

/* The DSI D-PHY is powered by the P4's on-chip LDO channel 3.  Every DSI call
 * fails, or the panel stays dark, if this is skipped. */
static esp_ldo_channel_handle_t phy_pwr;
esp_ldo_channel_config_t ldo_cfg = { .chan_id = 3, .voltage_mv = 2500 };
ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_cfg, &phy_pwr));

esp_lcd_dsi_bus_config_t bus_config = JD9365_PANEL_BUS_DSI_2CH_CONFIG();
esp_lcd_dsi_bus_handle_t dsi_bus;
ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &dsi_bus));
```

The panel Guition tests with is a Waveshare **10.1-DSI-TOUCH-A**, 800 × 1280, **JD9365**
controller, 2 lanes, DPI clock 80 MHz, RGB565:

```c
esp_lcd_dpi_panel_config_t dpi_config = {
    .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
    .dpi_clock_freq_mhz = 80,
    .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
    .num_fbs = 1,
    .video_timing = {
        .h_size = 800, .v_size = 1280,
        .hsync_pulse_width = 20, .hsync_back_porch = 20, .hsync_front_porch = 40,
        .vsync_pulse_width = 10, .vsync_back_porch = 4,  .vsync_front_porch = 30,
    },
    .flags = { .use_dma2d = true },
};
```

Espressif's own BSP for the EV board uses different porches for the same resolution
(`HSYNC 40 / HBP 140 / HFP 40`, `VSYNC 4 / VBP 16 / VFP 16`) and a 1500 Mbps lane rate.
**Both are in the archive Guition ships, and they disagree** — the numbers belong to the
panel, not the board, so take them from your panel's datasheet and treat either set as a
starting point.

There is **no reset GPIO and no backlight GPIO on J2**: pass `GPIO_NUM_NC` for the panel
reset. On Guition's panel, brightness is an I2C write, not PWM:

```c
/* I2C 0x45, register 0x96 on the "10.1 inch A" panel (0x86 on the non-A one),
 * one data byte 0..255.  Panel-specific — a different panel will not answer. */
uint8_t data[2] = { 0x96, brightness };
i2c_master_transmit(dev, data, sizeof(data), -1);
```

A panel with a plain LED string goes to **CN5** instead and is dimmed with LEDC on
**GPIO23**, which enables the MP3202 boost.

## 9. Touch — GT911 on the same I2C bus

**Not compiled in this session.**

```c
esp_lcd_touch_config_t tp_cfg = {
    .x_max = 800, .y_max = 1280,
    .rst_gpio_num = GPIO_NUM_NC,   /* GPIO22 exists but Guition's own port leaves it NC */
    .int_gpio_num = GPIO_NUM_NC,   /* GPIO21, likewise — polled, not interrupt-driven */
    .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
};
esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
tp_io_config.scl_speed_hz = 100 * 1000;
esp_lcd_new_panel_io_i2c(shared_i2c_bus, &tp_io_config, &tp_io);
esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, &tp);
```

The touch controller shares the codec's bus — pass the **same** `i2c_master_bus_handle_t`,
do not open a second bus on the same pins.

## 10. Ethernet — IP101 over RMII

**Not compiled in this session.** From Guition's `ethernet/basic` demo, whose sdkconfig is
in the archive.

```c
eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
esp32_emac_config.smi_gpio.mdc_num  = 31;
esp32_emac_config.smi_gpio.mdio_num = 52;
esp32_emac_config.dma_burst_len = ETH_DMA_BURST_LEN_4;
esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);

eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
phy_config.phy_addr = 1;
phy_config.reset_gpio_num = 51;
esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
```

sdkconfig side: `CONFIG_ETH_USE_ESP32_EMAC=y`, `CONFIG_ETH_PHY_INTERFACE_RMII=y`.

**No RMII pin configuration is needed on this board.** `ETH_ESP32_EMAC_DEFAULT_CONFIG()`
for the ESP32-P4 in ESP-IDF 5.5.5 already reads `mdc 31`, `mdio 52`, clock
`EMAC_CLK_EXT_IN` on GPIO50, and `emac_dataif_gpio.rmii = { tx_en 49, txd0 34, txd1 35,
crs_dv 28, rxd0 29, rxd1 30 }` — the board's exact wiring. The two `smi_gpio` lines above
are Guition's demo re-stating the defaults; you can drop them.

If you ever do need to move a pin, note that the RMII signals are **IO MUX pads with a
short list of alternatives each**, not matrix-routable to anywhere:
`CRS_DV` 28/45/51 · `RXD0` 29/46/52 · `RXD1` 30/47/53 · `TXD0` 34/41 · `TXD1` 35/42 ·
`TX_EN` 33/40/49 · `RMII_CLK` in 32/44/50. Set them through `emac_dataif_gpio.rmii`.
MDC and MDIO go through the GPIO matrix and can be any GPIO.

## 11. RS485 — UART1, and there is no direction GPIO

**Not compiled in this session.** From Guition's `uart_echo_rs485` demo.

```c
uart_config_t cfg = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};
ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 256, 0, 0, NULL, 0));
ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &cfg));
/* TX 26, RX 27.  RTS is UART_PIN_NO_CHANGE because DE//RE is generated in
 * hardware from the TX line by a 74LVC1G132 + an 8550 PNP one-shot — there is no
 * direction pin to assign, and hunting for one is the usual time sink here. */
ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, 26, 27, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
ESP_ERROR_CHECK(uart_set_mode(UART_NUM_1, UART_MODE_RS485_HALF_DUPLEX));
ESP_ERROR_CHECK(uart_set_rx_timeout(UART_NUM_1, 3));
```

## 12. A partition table that uses the 16 MB

`partitions_singleapp_large.csv` gives a **1,536 KB** app slot and strands 14 MB. Verified
by building with this CSV in the project root and `board_build.partitions = partitions.csv`
— the resulting `partitions.bin` reads `factory` 6,144 K at `0x010000` and `storage`
9,216 K at `0x610000`:

```csv
# name,   type, subtype,  offset,   size,  flags
nvs,      data, nvs,      0x9000,   0x6000,
phy_init, data, phy,      0xf000,   0x1000,
factory,  app,  factory,  0x10000,  0x600000,
storage,  data, spiffs,   ,         0x900000,
```

Keep `CONFIG_PARTITION_TABLE_*` in `sdkconfig.defaults` consistent with it — PlatformIO
builds the image from the `.ini` line, so a mismatch is silent.

## 13. Battery voltage

**Not compiled in this session, and the scaling is Guition's, not measured here.**
`BAT+` reaches **GPIO53** through a 68 K / 100 K divider (ratio 0.595), a 10 nF cap and a
0 R link (`R55`). GPIO53 is `ADC2_CHANNEL4` in the datasheet and in ESP-IDF
(`ADC2_CHANNEL_4_GPIO_NUM 53` in `soc/esp32p4/include/soc/adc_channel.h`), which is why the
vendor's unit/channel pair is the right one. Guition's `adc_test` demo reads ADC unit 2, channel 4, at
`ADC_ATTEN_DB_12`, averages 500 samples, and maps **2250 mV → 0 %** and **2450 mV → 100 %**
at the ADC pin — i.e. roughly 3.78 V to 4.12 V at the cell. Those end points are the
vendor's; check them against a meter before showing a user a percentage.

ADC2 shares the analog front end with nothing on this chip (there is no radio), so unlike
every Wi-Fi-carrying ESP32 there is no ADC2-versus-Wi-Fi restriction here.

## 14. Die temperature — the range trap

Verified in `template/src/board_report.c`.

```c
/* The requested range MUST be CONTAINED IN one row of the P4 range table in
 * soc/esp32p4/temperature_sensor_periph.c:
 *   {50..125, +-3} {20..100, +-2} {-10..80, +-1} {-30..50, +-2} {-40..20, +-3}
 * The driver takes the most accurate row that contains the request, so (-10, 80)
 * lands on the +-1 C row.  A plausible-looking (-10, 100) is contained in none of
 * them: temperature_sensor_install() then returns ESP_ERR_INVALID_ARG and logs
 * "Out of testing range" — which reads like a missing peripheral rather than a
 * bad argument. */
temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
ESP_ERROR_CHECK(temperature_sensor_install(&cfg, &handle));
ESP_ERROR_CHECK(temperature_sensor_enable(handle));

float c;
temperature_sensor_get_celsius(handle, &c);
```

## 15. Wi-Fi through the ESP32-C6

**Not compiled in this session.** `main/idf_component.yml`:

```yaml
dependencies:
  idf:
    version: '>=5.3'
  espressif/esp_wifi_remote: ^0.14.2
```

which pulls `espressif/esp_hosted` 2.0.13. Then the ordinary `esp_wifi_*` API works
unchanged — `esp_wifi_init`, `esp_wifi_scan_start`, `esp_wifi_connect` — because
`esp_wifi_remote` re-exports it over the SDIO transport. The sdkconfig block for the link
is in `board-hardware.md` §14; all of it is schematic-confirmed, and
`CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_HIGH=y` is the one that is easy to get backwards.

The C6 must already be running slave firmware. It is not a Wi-Fi error when it is not: the
failure appears as a transport timeout inside `esp_hosted` init.
