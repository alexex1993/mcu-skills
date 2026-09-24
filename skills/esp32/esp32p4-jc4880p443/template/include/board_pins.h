/*
 * Guition JC4880P443C_I_W (ESP32-P4 4.3" 480x800) — every pin on the board.
 *
 * Source markers:
 *   [S] traced on the board schematic JC4880P443_V1.0 (PDF netlist + PNG sheets)
 *   [V] taken from Guition's own demo code (1-Demo, ESP-IDF_5.5.4)
 *   [H] verified on hardware (rev v1.3 board) by this skill's bring-up
 *
 * Where a source disagrees with another it is said so on the line; see
 * ../reference/board-hardware.md §2 for the full argument.
 */
#pragma once

/* ------------------------------------------------------- DSI display */
/* 4.3" IPS, 480x800 portrait, ST7701S over 2-lane MIPI-DSI on FPC1 (30-pin).
 * The DSI lanes are dedicated silicon pins and cost no GPIO. */
#define LCD_H_RES 480
#define LCD_V_RES 800
#define LCD_DSI_LANES 2
#define LCD_DSI_LANE_MBPS 750   /* Guition BSP value                    [V][H] */
#define LCD_DPI_CLK_MHZ 28      /* ~50 Hz with the porches below        [V][H] */
#define LCD_HSYNC_PW 12
#define LCD_HSYNC_BP 42
#define LCD_HSYNC_FP 42
#define LCD_VSYNC_PW 2
#define LCD_VSYNC_BP 8
#define LCD_VSYNC_FP 166        /* yes, 166 — from the vendor timing     [V][H] */
#define LCD_DSI_LDO_CHAN 3      /* LDO_VO3 -> VDD_MIPI_DPHY               [V][H] */
#define LCD_DSI_LDO_MV 2500

#define PIN_LCD_RST 5           /* FPC1 pin 23, active low            [S][V][H] */
#define PIN_LCD_BL 23           /* MP3202 boost EN via R69 0R, 10K pull-DOWN
                                 * (R72): backlight is OFF until firmware
                                 * drives it. PWM on it dims.            [S][H] */
/* FPC1 pin 22 is the panel's TE output; it is not routed to the P4.     [S] */

/* ------------------------------------------------------- touch (GT911) */
#define PIN_TOUCH_RST 22        /* FPC1 pin 25                        [S][H] */
#define PIN_TOUCH_INT 21        /* FPC1 pin 28                        [S][H] */
/* Both ARE wired, although Guition's BSP declares them GPIO_NUM_NC.  Hand them
 * to esp_lcd_touch_gt911 together with driver_data (see src/touch.c), or the
 * driver does a bare 10 ms reset and the first config read fails.
 * An I2C scan on this board shows the GT911 at BOTH 0x5D and 0x14, before and
 * after that reset sequence; talk to 0x5D.                          [S][V][H] */
#define I2C_ADDR_GT911 0x5D
#define I2C_ADDR_GT911_ALT 0x14

/* ------------------------------------------------------- I2C (one bus) [S] */
/* One bus serves the ES8311, the GT911 (FPC1 26/27), the camera SCCB (FPC2
 * 13/15), CN3 and JP1 pins 23/25.  Pull-ups on board: 5K1 (R65/R71) in
 * parallel with 2K2 (R29/R31) = ~1.5K to 3V3.  The i2c_master "check pull-up"
 * warning is informational — it fires only because the internal pull-ups are
 * disabled. */
#define PIN_I2C_SDA 7
#define PIN_I2C_SCL 8
#define I2C_ADDR_ES8311 0x18    /*                                      [S][H] */
#define I2C_ADDR_OV02C10 0x36   /* only with the optional camera fitted  [V] */
#define ES8311_CHIP_ID1 0x83    /* registers 0xFD / 0xFE                 [H] */
#define ES8311_CHIP_ID2 0x11

/* ------------------------------------------------------------- audio [S] */
/* ES8311 on I2S.  DOUT/DIN named from the HOST's side; the schematic names
 * the CODEC's side, and GPIO48's net is called ES7210_SDOUT although there is
 * no ES7210 on this board — it is the ES8311's own ADC output. */
#define PIN_I2S_MCLK 13
#define PIN_I2S_BCLK 12
#define PIN_I2S_LRCK 10
#define PIN_I2S_DOUT 9          /* P4 -> codec DSDIN -> NS4150 -> CN1 speaker */
#define PIN_I2S_DIN 48          /* codec ASDOUT (on-board analog mic) -> P4 */
#define PIN_PA_CTRL 11          /* NS4150 enable, ACTIVE HIGH, 10K pull-down R19 */

/* ------------------------------------------------- microSD / TF slot [S][V] */
/* SDMMC slot 0 (IO MUX pads), 4-bit, no card detect.  Card VDD comes from the
 * P4's ON-CHIP LDO channel 4 through an always-on AO3401 (gate held by R13;
 * R10 to GPIO45 is NOT populated). */
#define PIN_SD_CLK 43
#define PIN_SD_CMD 44
#define PIN_SD_D0 39
#define PIN_SD_D1 40
#define PIN_SD_D2 41
#define PIN_SD_D3 42
#define SD_LDO_CHAN 4

/* ------------------------------------------------------------- RS485 [S] */
/* MAX485 on UART1.  DE//RE generated in hardware from TX (74LVC1G132 + 8550
 * one-shot) — there is no direction GPIO.  J5 also carries TX1/RX1 at TTL
 * level, in parallel with the transceiver; J4 carries A/B. */
#define PIN_RS485_TX 26
#define PIN_RS485_RX 27

/* ------------------------------------------------- UART0 / CN2 [S] */
/* CN2 (MX1.25 4P): VIN, TX0, RX0, GND.  There is NO USB-UART bridge on this
 * board — the only USB console is the native USB-Serial/JTAG. */
#define PIN_UART0_TX 37
#define PIN_UART0_RX 38

/* ------------------------------------------------- ESP32-C6 radio link [S][V] */
#define PIN_C6_SDIO_CLK 18
#define PIN_C6_SDIO_CMD 19
#define PIN_C6_SDIO_D0 14
#define PIN_C6_SDIO_D1 15
#define PIN_C6_SDIO_D2 16
#define PIN_C6_SDIO_D3 17
#define PIN_C6_RESET 54         /* C6 CHIP_PU, active high */
#define PIN_C6_IO2 6

/* ------------------------------------------------------- battery [S][V] */
/* IP5306 charger, Li-ion on CN4.  BAT+ -> 68K (R52) / 100K (R57) -> GPIO53
 * (ADC2_CH4) through R55 0R.  V_bat = V_adc * 168 / 100. */
#define PIN_BAT_ADC 53
#define BAT_DIV_NUM 168
#define BAT_DIV_DEN 100

/* ------------------------------------------------------- buttons [S] */
/* SW1 -> GPIO35 (BOOT strap), active low, 10K pull-up R34.  Also JP1 pin 15.
 * SW2 -> CHIP_PU (reset).  SW3 is the IP5306 power key, not on a GPIO. */
#define PIN_BOOT_BUTTON 35      /*                                    [S][H] */

/* ------------------------------------------------------- JP1 header [S] */
/*  2x13, 2.54 mm.
 *    1  3V3          2  5V
 *    3  3V3          4  5V
 *    5  GND          6  GND
 *    7  GPIO52       8  GPIO33
 *    9  GPIO51      10  GPIO31
 *   11  GPIO50      12  GPIO30
 *   13  GPIO49      14  GPIO29
 *   15  GPIO35*     16  GND          * BOOT strap + SW1
 *   17  GPIO34      18  3V3
 *   19  GPIO32      20  C6_U0RXD     } these four belong to the ESP32-C6,
 *   21  GPIO28      22  C6_U0TXD     } brought out for reflashing it.
 *   23  I2C SDA(7)  24  C6_IO9       } Driving them takes Wi-Fi down.
 *   25  I2C SCL(8)  26  C6_CHIP_PU   }
 *
 * The silkscreen RMII names (MDC, RMII_TXD0, ...) are inherited from the
 * module's reference design: there is NO Ethernet PHY on this board, so all
 * eleven GPIOs are plain free pins. */
#define JP1_FREE_GPIOS \
    { 28, 29, 30, 31, 32, 33, 34, 49, 50, 51, 52 }

/* Not routed anywhere on this board: GPIO1-4, 20, 46, 47 (module pads only).
 * GPIO36 has a 10K pull-up (R44) and reaches no connector.             [S] */
