/*
 * board.h — ESP32-WROOM-32 devkit, 38-pin header.
 *
 * Everything here is a property of THIS header layout, not of the ESP32 chip.
 * The 38-pin header breaks out every pad of the module: all 32 signal pins,
 * including GPIO0 and the six in-package flash pins (GPIO6-11), which are
 * wired to the flash die and are not usable — see SKILL.md rules 1 and 2.
 */
#pragma once

#include "driver/gpio.h"
#include "hal/adc_types.h"
#include "hal/spi_types.h"

/* ---------------------------------------------------------------- identity */
#define BOARD_NAME              "ESP32-WROOM-32 devkit (38-pin)"
#define BOARD_HEADER_PINS       38
#define BOARD_EXPOSED_GPIOS     32   /* 26 usable + 6 flash pins that are not */

/* ------------------------------------------------------------- onboard I/O */
/*
 * LED: this is the one thing 38-pin boards disagree about.
 *   Espressif ESP32-DevKitC V4  — no user LED at all. Set this to 0.
 *   NodeMCU-32S, DOIT 38-pin, most CH340 clones — blue LED, active HIGH, GPIO2.
 * If the minimal variant prints ticks but nothing lights up, that is the
 * answer, not a bug.
 */
#define BOARD_HAS_USER_LED      1
#define BOARD_LED_GPIO          GPIO_NUM_2
#define BOARD_LED_ACTIVE_LEVEL  1

/* BOOT button on GPIO0, pressed = LOW. On this header GPIO0 is ALSO a header
 * pin, so anything you wire there fights the button and the boot strap. */
#define BOARD_HAS_BOOT_BUTTON   1
#define BOARD_BOOT_BTN_GPIO     GPIO_NUM_0
#define BOARD_BOOT_BTN_PRESSED  0

/* EN/RST button acts on CHIP_PU. It is not a GPIO and cannot be read. */

/* --------------------------------------------------------------- USB/UART0 */
#define BOARD_UART0_TX_GPIO     GPIO_NUM_1
#define BOARD_UART0_RX_GPIO     GPIO_NUM_3

/* ------------------------------------------------------------ analog inputs */
/* ADC1 only. ADC2 shares hardware with the Wi-Fi PHY and returns
 * ESP_ERR_TIMEOUT for as long as the Wi-Fi driver is started. */
#define BOARD_ADC_UNIT          ADC_UNIT_1
#define BOARD_ADC_CH_GPIO34     ADC_CHANNEL_6   /* GPIO34, input-only */
#define BOARD_ADC_CH_GPIO35     ADC_CHANNEL_7   /* GPIO35, input-only */
#define BOARD_ADC_CH_GPIO32     ADC_CHANNEL_4   /* GPIO32 */
#define BOARD_ADC_CH_GPIO33     ADC_CHANNEL_5   /* GPIO33 */
#define BOARD_ADC_CH_GPIO36     ADC_CHANNEL_0   /* SENSOR_VP, input-only */
#define BOARD_ADC_CH_GPIO39     ADC_CHANNEL_3   /* SENSOR_VN, input-only */

#define BOARD_ADC_DEMO_CHANNEL  BOARD_ADC_CH_GPIO34
#define BOARD_ADC_DEMO_GPIO     34

/* ------------------------------------------------------- suggested bus pins */
#define BOARD_I2C_SDA_GPIO      GPIO_NUM_21
#define BOARD_I2C_SCL_GPIO      GPIO_NUM_22

/*
 * SPI3 (the silkscreen calls it VSPI) on exactly these four pins is the only
 * combination on this chip that reaches 80 MHz: they are SPI3's IO_MUX pads,
 * so the signals bypass the GPIO Matrix. Any other pin choice, or the same
 * pins on SPI2_HOST, is routed through the matrix and the driver silently
 * clamps full-duplex transfers to 26.67 MHz (80 MHz APB / 3, from the matrix's
 * 25 ns round-trip delay). SPI2's own IO_MUX pads are 12/13/14/15, which are
 * the JTAG pins and include two strapping pins — rarely worth it.
 */
#define BOARD_SPI_HOST_DEFAULT  SPI3_HOST       /* "VSPI" on the silkscreen */
#define BOARD_SPI_MOSI_GPIO     GPIO_NUM_23     /* VSPID  — IO_MUX */
#define BOARD_SPI_MISO_GPIO     GPIO_NUM_19     /* VSPIQ  — IO_MUX */
#define BOARD_SPI_SCLK_GPIO     GPIO_NUM_18     /* VSPICLK — IO_MUX */
#define BOARD_SPI_CS_GPIO       GPIO_NUM_5      /* VSPICS0 — IO_MUX, and a strapping pin */

/* GPIO16/17 are free on ESP32-WROOM-32 and ESP32-SOLO-1 modules only.
 * On an ESP32-WROVER the same header pins are the PSRAM interface. */
#define BOARD_UART1_TX_GPIO     GPIO_NUM_17     /* silkscreened TX2 / IO17 */
#define BOARD_UART1_RX_GPIO     GPIO_NUM_16     /* silkscreened RX2 / IO16 */

/* --------------------------------------------------------------- pin policy */
/*
 * Safe for anything:
 *   4, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33
 * Input-only (no output driver, no pull-up/pull-down):
 *   34, 35, 36 (VP), 39 (VN)
 * On the header but constrained:
 *   0       strapping + BOOT button + auto-reset circuit
 *   1, 3    UART0 to the USB bridge
 *   2       strapping (+ user LED on most clones)
 *   5       strapping (must read high at reset)
 *   12      strapping MTDI (must read LOW at reset — see SKILL.md)
 *   15      strapping MTDO
 * On the header and NOT usable — in-package SPI flash bus:
 *   6 (CLK), 7 (SD0), 8 (SD1), 9 (SD2), 10 (SD3), 11 (CMD)
 * Do not exist on any ESP32-WROOM-32 module:
 *   20, 24, 28, 29, 30, 31, 37, 38
 */
#define BOARD_SAFE_GPIOS { 4, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33 }
#define BOARD_INPUT_ONLY_GPIOS { 34, 35, 36, 39 }
#define BOARD_STRAPPING_GPIOS { 0, 2, 5, 12, 15 }
#define BOARD_FLASH_GPIOS { 6, 7, 8, 9, 10, 11 }   /* exposed on this header — do not wire */
