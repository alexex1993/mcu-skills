/*
 * Guition JC-ESP32P4-M3-DEV — every pin on the board, in one place.
 *
 * Source markers:
 *   [S] traced on the board schematic (docs 5-Schematic, sheets 1-8)
 *   [V] taken from Guition's own demo code (docs 1-Demo)
 *   [D] ESP32-P4 Series Datasheet v0.5, Appendix A / Section 2
 *
 * Nothing here is a guess.  Where a source disagrees with another it is said so
 * on the line; see ../reference/board-hardware.md §2 for the full argument.
 */
#pragma once

/* ------------------------------------------------------------------ blink */
/* There is NO user LED on this board.  The only LED that lights is D1, the red
 * power indicator on the 3V3 rail, and it is not on a GPIO. [S]
 * Wire an external LED to a free JP1 pin:
 *     JP1 pin --- [330R] --- (+)LED(-) --- GND
 * Override with -DBLINK_GPIO=NN in platformio.ini.
 *
 * Note what the template's read-back test does and does not prove: in
 * GPIO_MODE_INPUT_OUTPUT the input buffer reads the PAD, and an unloaded pad
 * follows its own driver -- so a passing read-back proves the pin drives, never
 * that it reaches a connector.  Pick from JP1_FREE_GPIOS below. */
#ifndef BLINK_GPIO
#define BLINK_GPIO 20 /* JP1 pin 17, ADC1_CH4, no strap, no alternate use [S] */
#endif

/* --------------------------------------------------- JP1 expansion header */
/* 2x13, 2.54 mm, on the left edge.  Odd pins are the outer row. [S]
 *
 *    1  VCC3V3        2  VCC5V
 *    3  VCC3V3        4  VCC5V
 *    5  GND           6  GND
 *    7  GPIO1         8  (not connected)
 *    9  GPIO2        10  GPIO47
 *   11  GPIO3        12  GPIO46
 *   13  GPIO4        14  GPIO45
 *   15  GPIO5        16  GND
 *   17  GPIO20       18  VCC3V3
 *   19  GPIO32       20  C6_U0RXD
 *   21  GPIO33       22  C6_U0TXD
 *   23  ES_I2C_SDA   24  C6_IO9
 *   25  ES_I2C_SCL   26  C6_CHIP_PU
 *
 * Pins 20/22/24/26 belong to the ESP32-C6, not the P4: they are the C6's UART0,
 * its IO9 (its own boot strap) and its enable line, brought out so the radio
 * co-processor can be reflashed.  Driving them fights esp_hosted. */
#define JP1_FREE_GPIOS \
    { 1, 2, 3, 4, 5, 20, 32, 33, 45, 46, 47 }

/* ------------------------------------------------------- I2C (one bus) [S] */
/* One bus serves everything: the ES8311 codec, the DSI panel's touch
 * controller and its brightness register, the CSI camera's SCCB, plus the
 * CN3 4-pin connector and JP1 pins 23/25.  5.1K pull-ups to 3V3 are on board
 * (R65/R71), so do not enable internal ones for a long cable. */
#define PIN_I2C_SDA 7
#define PIN_I2C_SCL 8

#define I2C_ADDR_ES8311 0x18 /* audio codec                            [S][V] */
#define I2C_ADDR_GT911_A 0x5D /* DSI panel touch, primary address       [V] */
#define I2C_ADDR_GT911_B 0x14 /* DSI panel touch, alternate address     [V] */
#define I2C_ADDR_PANEL_BL 0x45 /* 10.1" DSI panel brightness register   [V] */

/* ------------------------------------------------------------- audio [S] */
/* ES8311 on I2S0.  DOUT/DIN are named from the HOST's point of view here; the
 * schematic net names are the CODEC's, which reads backwards — CODEC_I2S0_DSDIN
 * is data INTO the codec, i.e. out of the P4. */
#define PIN_I2S_MCLK 13
#define PIN_I2S_BCLK 12 /* schematic net CODEC_I2S0_SCLK */
#define PIN_I2S_LRCK 10
#define PIN_I2S_DOUT 9 /* P4 -> codec DSDIN -> speaker */
#define PIN_I2S_DIN 48 /* codec ASDOUT -> P4.  Schematic net is named
                        * ES7210_SDOUT, but there is no ES7210 on this board:
                        * it is the ES8311's own ADC output.             [S] */
#define PIN_PA_CTRL 11 /* NS4150 amplifier /STD, ACTIVE HIGH, 10K pull-DOWN
                        * (R19) — muted until firmware raises it.        [S] */

/* Analog microphone: MSM381A3729H9CP into the ES8311's MIC1 differential
 * input with MICBIAS.  One mic, no digital mic array. [S] */

/* ------------------------------------------------- microSD / TF slot [S][V] */
/* SDMMC slot 0 (the IO MUX slot: SD1_* pads).  4-bit, no card detect, no write
 * protect.  Card VDD comes from the P4's ON-CHIP LDO channel 4 through an
 * always-on AO3401 switch, so the card is dead until that channel is acquired
 * — see reference/recipes.md §5. */
#define PIN_SD_CLK 43
#define PIN_SD_CMD 44
#define PIN_SD_D0 39
#define PIN_SD_D1 40
#define PIN_SD_D2 41
#define PIN_SD_D3 42
#define SD_LDO_CHAN 4 /* esp_ldo / sd_pwr_ctrl channel for TF_VCC [S][V] */

/* GPIO45 reaches the TF power switch gate through R10, which is NOT POPULATED
 * (R13 10K holds the gate low, so the switch is permanently on).  GPIO45 is
 * therefore free and is on JP1 pin 14. [S] */

/* ------------------------------------------------------- DSI display [S][V] */
/* J2, 15-way 0.5 mm FPC: 2 MIPI-DSI lanes + the shared I2C + 3V3.  The DSI
 * lanes are dedicated silicon pins, not GPIOs — they cost no GPIO at all.  The
 * panel has NO reset GPIO and NO backlight GPIO on J2: brightness on the
 * vendor's 10.1" panel is an I2C register (0x45), and a raw LED string goes to
 * CN5 driven by an MP3202 boost whose enable is GPIO23. */
#define PIN_LCD_PWM 23 /* MP3202 backlight-boost enable -> CN5 LEDA/LEDK */
#define PIN_TOUCH_INT 21 /* to J2/FPC1; the touch panel may leave it idle */
#define PIN_TOUCH_RST 22
#define PIN_LCD_RST -1 /* not wired [S] */
#define LCD_DSI_LANES 2
#define LCD_DSI_LDO_CHAN 3 /* LDO_VO3 -> VDD_MIPI_DPHY, 2500 mV [V][D] */

/* ---------------------------------------------------------- CSI camera [S] */
/* J3, 15-way 0.5 mm FPC: 2 MIPI-CSI lanes + the shared I2C for SCCB + 3V3 +
 * two spare lines (CSI_IO0/CSI_IO1) that are pulled up but go nowhere else.
 * Sensor reset and power-down are not wired: pass -1 for both. */

/* ---------------------------------------------------------- Ethernet [S] */
/* Internal EMAC + an IP101 PHY with its own 25 MHz crystal, RMII.  (The part
 * number is from Guition's demo sdkconfig; the schematic labels the symbol
 * only "U4".)  The RMII data and clock signals are IO MUX pads: each may be
 * assigned only from a short per-signal list of pads, not to an arbitrary
 * GPIO.  MDC and MDIO go through the GPIO matrix and can be anywhere.
 * ETH_ESP32_EMAC_DEFAULT_CONFIG() for the P4 in ESP-IDF 5.5.5 already defaults
 * to exactly the numbers below, so this board needs no pin configuration. */
#define PIN_ETH_RMII_RXDV 28
#define PIN_ETH_RMII_RXD0 29
#define PIN_ETH_RMII_RXD1 30
#define PIN_ETH_RMII_TXD0 34
#define PIN_ETH_RMII_TXD1 35 /* also the boot strapping pin and SW1 — see below */
#define PIN_ETH_RMII_TXEN 49
#define PIN_ETH_RMII_CLK 50 /* PHY drives the 50 MHz reference in */
#define PIN_ETH_MDC 31
#define PIN_ETH_MDIO 52
#define PIN_ETH_PHY_RST 51
#define ETH_PHY_ADDR 1 /* set by the 5.1K strap resistors on the PHY [S][V] */

/* ------------------------------------------------------------- RS485 [S] */
/* MAX485 on UART1 through J5/J4.  DE//RE is generated IN HARDWARE from the TX
 * line by a 74LVC1G132 + an 8550 PNP one-shot, so there is NO direction GPIO to
 * assign: pass RTS = UART_PIN_NO_CHANGE.  Guition's own demo still calls
 * uart_set_mode(UART_MODE_RS485_HALF_DUPLEX), which is harmless and keeps the
 * receiver blanked while transmitting. */
#define PIN_RS485_TX 26
#define PIN_RS485_RX 27
#define RS485_UART_NUM 1

/* ------------------------------------------------- console / UART0 [S][D] */
#define PIN_UART0_TX 37 /* CH340C socket, and CN2 pin 2 */
#define PIN_UART0_RX 38 /* diode-OR of the CH340C and CN2 pin 3 */

/* ------------------------------------------------- ESP32-C6 radio link [S] */
/* esp_hosted over SDIO slot 1, 4-bit, 40 MHz.  GPIO14-19 never leave the
 * module; GPIO6 is wired to the C6's IO2; GPIO54 is the C6's enable and is
 * ACTIVE HIGH.  All eight are unavailable to the application. */
#define PIN_C6_SDIO_CLK 18
#define PIN_C6_SDIO_CMD 19
#define PIN_C6_SDIO_D0 14
#define PIN_C6_SDIO_D1 15
#define PIN_C6_SDIO_D2 16
#define PIN_C6_SDIO_D3 17
#define PIN_C6_RESET 54 /* active HIGH */
#define PIN_C6_IO2 6

/* ------------------------------------------------------- battery / power */
/* IP5306 charger from the USB 5 V rail, single Li-ion cell on CN4.  Guition's
 * own ADC demo reads ADC_UNIT_2 / ADC_CHANNEL_4 at 12 dB and maps 2250 mV
 * (empty) .. 2450 mV (full) to 0..100 %.  ADC2_CHANNEL_4 is GPIO53 [D], which
 * the schematic shows reaching the divider through R55 (0R) from BAT+ via
 * 68K/100K.  Untested here — measure before trusting the percentage. */
#define PIN_BAT_SENSE 53 /* ADC2_CHANNEL_4 */
#define BAT_ADC_MV_EMPTY 2250
#define BAT_ADC_MV_FULL 2450

/* -------------------------------------------------------- buttons [S][D] */
/* SW1 pulls GPIO35 (the boot strap) low; SW2 pulls CHIP_PU low (reset).
 * Both are momentary and both have 10K pull-ups.  GPIO35 is ALSO Ethernet
 * RMII_TXD1: reading the button as a user button while the MAC is running is
 * not possible, and holding it at reset enters download mode.
 * Guition's own xiaozhi port defines BOOT_BUTTON_GPIO as 21, which is the
 * touch-panel interrupt, not this button.  Do not copy that. */
#define PIN_BOOT_BUTTON 35 /* active low, 10K pull-up (R34) */
