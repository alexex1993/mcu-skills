/*
 * Waveshare ESP32-C6-Touch-LCD-1.47 board map.
 *
 * Sources: the vendor pinout table
 * (https://www.waveshare.com/wiki/ESP32-C6-Touch-LCD-1.47) cross-checked
 * against ESP32-C6-Touch-LCD-1.47-Schematic.pdf.  Where the two disagree the
 * schematic wins — see BSP_BOOT_BTN below.
 *
 * NOTE: not one of these numbers matches the non-touch ESP32-C6-LCD-1.47.
 * The two boards share a name, a chip family and a panel size, and nothing
 * else.  Do not copy pins between them.
 */

#pragma once

/* --- 1.47" IPS panel, JD9853, 4-wire SPI ------------------------------ */
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

/* --- AXS5106L capacitive touch, on the shared I2C bus ----------------- */
#define BSP_I2C_PORT        I2C_NUM_0
#define BSP_I2C_SDA         GPIO_NUM_18   /* shared with the QMI8658A IMU, 10k pull-up on board */
#define BSP_I2C_SCL         GPIO_NUM_19   /* 10k pull-up on board */
#define BSP_TP_RST          GPIO_NUM_20
#define BSP_TP_INT          GPIO_NUM_21

/* Touch orientation is not hard-coded: the app derives it from a three-point
 * calibration and keeps the result in NVS. Hold BOOT during reset to redo it. */

/* --- QMI8658A IMU, same I2C bus, SA0 grounded ------------------------- */
#define BSP_IMU_I2C_ADDR    0x6B          /* SA0 = GND; 0x6A would be SA0 high/open */
#define BSP_IMU_INT1        GPIO_NUM_5
#define BSP_IMU_INT2        GPIO_NUM_6

/* --- Buttons ---------------------------------------------------------- */
/* The wiki's pinout table says the BOOT button is on GPIO8. The schematic
 * says otherwise: Key2 -> net BOOT -> ESP32-C6 pin 15 = GPIO9, with a 10k
 * pull-up and a 100n cap. GPIO8 goes only to header pin 20 and nothing else,
 * so reading GPIO8 for the button always returns "not pressed". */
#define BSP_BOOT_BTN        GPIO_NUM_9    /* pressed = low */

/* --- TF card (not used here, but its CS must stay high on the shared bus) */
#define BSP_SD_CS           GPIO_NUM_4
#define BSP_SD_MISO         GPIO_NUM_3
#define BSP_SD_MOSI         BSP_LCD_MOSI
#define BSP_SD_SCLK         BSP_LCD_SCLK

/* --- Battery sense: VBAT through a 200k/100k divider ------------------ */
#define BSP_BAT_ADC_GPIO    GPIO_NUM_0    /* ADC1_CH0; VBAT = 3 x V(GPIO0) */

/* --- The only pins the board leaves alone ----------------------------- */
#define BSP_FREE_IO7        GPIO_NUM_7    /* header pin 22 */
#define BSP_FREE_IO8        GPIO_NUM_8    /* header pin 20 -- boot strap, never pull it low */
