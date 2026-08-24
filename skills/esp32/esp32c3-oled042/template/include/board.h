/* ESP32-C3 0.42" OLED board (ABRobot / 01Space "ESP32-C3-0.42LCD" and clones).
 *
 * Every board-specific number lives here. If something on your board is wired
 * differently, this is the only file that should need editing.
 */
#pragma once

/* --- On-board I2C: the OLED, and the only I2C bus you get -----------------
 *
 * The ESP32-C3 has exactly one I2C peripheral. The panel sits on GPIO5/GPIO6
 * with the board's own 4.7k pull-ups. The arduino-esp32 variant header claims
 * SDA/SCL are GPIO8/GPIO9 -- that is the SoC default, not this board, and
 * GPIO8/GPIO9 here are the LED and the BOOT button.
 *
 * External I2C devices share this bus: same two pins, different address.
 */
#define BOARD_I2C_SDA_GPIO 5
#define BOARD_I2C_SCL_GPIO 6
#define BOARD_I2C_FREQ_HZ  400000

/* --- The panel ------------------------------------------------------------
 *
 * SSD1306 controller, but only a 72x40 window of its 128x64 GDDRAM is bonded
 * to glass, and that window does NOT start at column 0. Driving it as a plain
 * 128x64 gives snow.
 */
#define BOARD_OLED_I2C_ADDR   0x3C
#define BOARD_OLED_W          72
#define BOARD_OLED_H          40
#define BOARD_OLED_PAGES      (BOARD_OLED_H / 8) /* 5 pages of 8 rows */
#define BOARD_OLED_COL_OFFSET 28                 /* panel column 0 = GDDRAM column 28 */
#define BOARD_OLED_MUX_RATIO  40                 /* 0xA8 argument is this minus 1 */

/* --- LED ------------------------------------------------------------------
 *
 * Blue LED on GPIO8, wired to 3V3 through a resistor: the pin SINKS the
 * current, so LOW = lit. GPIO8 is also a boot-strapping pin; see SKILL.md.
 */
#define BOARD_LED_GPIO   8
#define BOARD_LED_ON     0
#define BOARD_LED_OFF    1

/* --- Button ---------------------------------------------------------------
 *
 * The BOOT/"BOO" button pulls GPIO9 to ground. It has an external pull-up and
 * doubles as a user button once the board has booted. RESET acts on CHIP_EN
 * and is not readable from software.
 */
#define BOARD_BOOT_GPIO    9
#define BOARD_BOOT_PRESSED 0

/* --- Free header pins -----------------------------------------------------
 *
 * GPIO0..GPIO4, GPIO7, GPIO10, GPIO20, GPIO21 come out on the headers and are
 * otherwise unused. GPIO18/GPIO19 are the USB D-/D+ lines -- do not touch.
 * ADC1 channels 0..4 are GPIO0..GPIO4; ADC2 (GPIO5) is on the OLED bus and is
 * unusable on several C3 revisions anyway.
 */
#define BOARD_UART0_TX_GPIO 21
#define BOARD_UART0_RX_GPIO 20
