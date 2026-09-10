// Pin map of the Waveshare ESP32-S3-RLCD-4.2 (SKU 33298 / 33507).
//
// Sources, in order of trust:
//   [S]  Board schematic, files.waveshare.com/wiki/ESP32-S3-RLCD-4.2/
//        ESP32-S3-RLCD-4.2-schematic.pdf — the authority; net-level
//   [G]  Waveshare's own example code, github.com/waveshareteam/ESP32-S3-RLCD-4.2
//   [W]  Waveshare wiki "Interface Introduction" GPIO table — has known
//        errors, see reference/board-hardware.md §2.2
//   [E]  Waveshare ESPHome tutorial §2.2 "GPIO Pin Assignment"
//   [Z]  Zephyr board port boards/waveshare/esp32s3_rlcd_4_2 (.dts + pinctrl.dtsi)
//
// Where sources disagree it is called out. Nothing here was probed on hardware,
// but every pin below is corroborated by the schematic.

#pragma once

// --- 4.2" reflective LCD, Sitronix ST7305/ST7306, 300 x 400 ---------------
// Write-only 4-wire SPI: the panel never drives data back, so there is no MISO
// and no CS-read cycle. There is no backlight pin — the panel is reflective.
// [S][G][E][Z] all agree on these five. Waveshare's own U8g2 example uses
// SPIClass(HSPI) at 24 MHz with exactly these numbers.
#define PIN_LCD_SCLK 11
#define PIN_LCD_MOSI 12
#define PIN_LCD_CS   40
#define PIN_LCD_DC    5  // "LCD_RS" in the Waveshare table
#define PIN_LCD_RST  41
// LCD_TE is real: the schematic ties net LCD_TE to GPIO6 and to pin 10 of the
// FPC connector [S]. Nothing in software reads it — not Waveshare's own
// driver, not U8g2, not the Zephyr port — but the pin is taken, not free.
#define PIN_LCD_TE    6

// --- I2C bus (one bus, four slaves) ---------------------------------------
// SHTC3 0x70 · ES8311 0x18 · ES7210 0x40 or 0x42 · PCF85063A 0x51
#define PIN_I2C_SDA  13
#define PIN_I2C_SCL  14

#define I2C_ADDR_ES8311   0x18
#define I2C_ADDR_ES7210   0x40  // 0x42 on some batches
#define I2C_ADDR_PCF85063 0x51
#define I2C_ADDR_SHTC3    0x70

// PCF85063ATL interrupt/alarm output, active low. [S][Z]
#define PIN_RTC_INT  15

// --- I2S audio ------------------------------------------------------------
// Directions are from the ESP32-S3's point of view.
// GPIO8  = ESP -> ES8311 DSDIN  (playback / speaker)
// GPIO10 = ES7210 ASDOUT -> ESP (capture / microphones)
// Confirmed at the schematic: GPIO8 -> ES8311 pin 9 DSDIN, and ES7210 pin 11
// SDOUT1 -> R42 (51R) -> GPIO10 [S]. The Zephyr pinctrl swaps them and its
// i2s0 node is never enabled, so it is untested there — do not copy it.
#define PIN_I2S_DOUT  8
#define PIN_I2S_BCLK  9
#define PIN_I2S_DIN  10
#define PIN_I2S_MCLK 16
#define PIN_I2S_LRCK 45  // also the VDD_SPI strapping pin — see SKILL.md rule 6
#define PIN_SPK_EN   46  // "PA_CTRL", speaker amplifier enable, ACTIVE HIGH.
                         // Schematic: GPIO46 -> R48 (0R) -> NS4150 CTRL, with
                         // R49 10K pulling it DOWN [S]. That pull-down both
                         // mutes the amp at boot and holds this strapping pin
                         // low through reset — see SKILL.md rule 6.

// --- microSD / TF slot ----------------------------------------------------
// SDMMC host, 1-bit mode. Not an SPI slot: the wiki table's "MOSI/SCK/MISO"
// labels are CMD/CLK/D0. Card pins D1 and D2 are explicitly unconnected in the
// schematic, so 4-bit mode cannot work [S]. Waveshare's own BSP declares
//   CustomSDPort(name, clk = 38, cmd = 21, d0 = 39, width = 1)   [G]
// The card's CS (CD/D3) has a 10K pull-up and reaches GPIO17 only through R7,
// which is NOT POPULATED — that is why there is no CS to pass [S].
// Card-detect is tied to ground, so there is no card-present signal either.
// [Z] caps the bus at 20 MHz.
#define PIN_SD_CLK   38
#define PIN_SD_CMD   21
#define PIN_SD_D0    39

// --- Buttons --------------------------------------------------------------
// Both active low, each with an external 10K pull-up and a 100nF debounce cap
// on the board [S], so plain INPUT works; INPUT_PULLUP is merely harmless.
// PWR is NOT on this list: it is Key3 on a hardware power-latch IC (U3) and is
// not readable from firmware. There is no EN/RST button at all — CHIP_PU
// carries only an RC network, no switch [S].
#define PIN_BTN_BOOT  0
#define PIN_BTN_KEY  18

// --- Battery --------------------------------------------------------------
// 18650 holder through R21 200K / R23 100K, both 1% — exactly 1/3 [S].
// GPIO4 is ADC1_CH3, so it keeps working with Wi-Fi up. Needs 12 dB
// attenuation to reach ~1.4 V; Waveshare's own BSP uses ADC_UNIT_1,
// ADC_CHANNEL_3, ADC_ATTEN_DB_12 and multiplies by 3 [G].
#define PIN_BAT_ADC   4
#define BAT_DIVIDER   3.0f

// --- 2x8 2.54 mm expansion header, as silkscreened -------------------------
//   top row:  VBUS  GND  GP19  GP20  TXD   RXD   SDA   SCL
//   bottom:   3V3   GND  GP0   GP1   GP2   GP3   GP17  GP18
// TXD/RXD are GPIO43/44 (UART0); SDA/SCL are the shared I2C bus above;
// GP19/GP20 are the native-USB D-/D+; GP0 is BOOT; GP18 is KEY.
// Free on the header: GPIO1, GPIO2, GPIO17 and GPIO3 — but GPIO3 is the
// JTAG-source strapping pin and floats, so give it a defined level at reset.
// GPIO17 also carries the depopulated R7 to the SD card's CS (see above);
// with R7 absent it is genuinely free.
#define PIN_FREE_A    1  // ADC1_CH0, TOUCH1
#define PIN_FREE_B    2  // ADC1_CH1, TOUCH2
#define PIN_FREE_C   17  // ADC2_CH6 — dead while Wi-Fi is up
#define PIN_UART0_TX 43
#define PIN_UART0_RX 44

// --- Not available on this module -----------------------------------------
// GPIO26-32  in-package quad SPI flash bus
// GPIO35-37  octal SPI PSRAM (N16R8) — fatal to drive
// GPIO22-25  do not exist on the ESP32-S3
//
// GPIO7, GPIO42, GPIO47, GPIO48 are connected to NOTHING on this board [S],
// but none of them reaches the header — they are usable only by soldering
// to the module pad. The wiki table's "TP_INT" (GPIO7) and "TP_RESET" (GPIO42)
// are wrong: those touch signals exist as pins 16-19 of the LCD FPC connector
// and are all marked unconnected. See reference/board-hardware.md §2.2.
