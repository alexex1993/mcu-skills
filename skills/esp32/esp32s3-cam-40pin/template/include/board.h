/* board.h — ESP32-S3-WROOM-1 CAM board, 40-pin (Freenove FNK0085 and clones)
 *
 * The only board-specific file in this project. Every other file uses these
 * names, so retargeting to another ESP32-S3 camera board is a one-file edit.
 *
 * Header layout, silkscreen order, USB connectors at the bottom:
 *
 *   left  (20)  3V3 RST  4  5  6  7 15 16 17 18  8  3 46  9 10 11 12 13 14 5V
 *   right (20)   TX  RX  1  2 42 41 40 39 38 37 36 35  0 45 48 47 21 20 19 GND
 *
 * TX = GPIO43, RX = GPIO44, RST = chip EN (not a GPIO).
 */
#pragma once

/* ---- camera, 8-bit DVP (OV2640 / OV3660 / GC2145 depending on batch) ------
 * Identical to CAMERA_MODEL_ESP32S3_EYE in the Arduino core's camera_pins.h.
 * PWDN and RESET are not wired on this board — both must stay -1.        */
#define BOARD_CAM_PWDN   -1
#define BOARD_CAM_RESET  -1
#define BOARD_CAM_XCLK   15
#define BOARD_CAM_SIOD    4   /* SCCB SDA — not the Wire bus */
#define BOARD_CAM_SIOC    5   /* SCCB SCL */
#define BOARD_CAM_D0     11   /* Y2 */
#define BOARD_CAM_D1      9   /* Y3 */
#define BOARD_CAM_D2      8   /* Y4 */
#define BOARD_CAM_D3     10   /* Y5 */
#define BOARD_CAM_D4     12   /* Y6 */
#define BOARD_CAM_D5     18   /* Y7 */
#define BOARD_CAM_D6     17   /* Y8 */
#define BOARD_CAM_D7     16   /* Y9 */
#define BOARD_CAM_VSYNC   6
#define BOARD_CAM_HREF    7
#define BOARD_CAM_PCLK   13

/* ---- microSD slot: SDMMC host, 1-bit bus only ----------------------------
 * D1/D2/D3 are not routed on this board, so 4-bit mode cannot work.
 * All three pins are JTAG pads: 39 = MTCK, 38 = FSPIWP, 40 = MTDO.        */
#define BOARD_SD_CLK     39
#define BOARD_SD_CMD     38
#define BOARD_SD_D0      40

/* ---- indicators ---------------------------------------------------------- */
#define BOARD_LED         2   /* plain LED, active HIGH, silkscreen "IO2" */
#define BOARD_RGB_LED    48   /* WS2812, one pixel, GRB */

/* ---- button -------------------------------------------------------------- */
#define BOARD_BOOT_BTN    0   /* BOOT, pressed = LOW, has an internal pull-up */

/* ---- console ------------------------------------------------------------- */
#define BOARD_UART0_TX   43
#define BOARD_UART0_RX   44

/* ---- native USB (the second USB-C, silkscreen "USB") --------------------- */
#define BOARD_USB_DM     19
#define BOARD_USB_DP     20

/* ---- pins that are NOT free ---------------------------------------------
 * 26-32  in-package SPI flash bus — not brought out, never touch
 * 33, 34 not brought out on this header
 * 35, 36, 37 octal PSRAM (R8/R16V modules) — on the header, but unusable
 * 45     VDD_SPI voltage strap, latched at reset (weak pull-down)
 * 46     boot-mode + ROM-log strap, latched at reset (weak pull-down)
 * 3      JTAG source strap, floating, no internal pull
 * 0      BOOT strap + BOOT button
 * 22-25  do not exist on the ESP32-S3 at all
 */
#define BOARD_PSRAM_GPIOS   { 35, 36, 37 }
#define BOARD_STRAP_GPIOS   { 0, 3, 45, 46 }

/* ---- what is actually left with camera + SD + WS2812 + UART0 running ----
 * 1, 2, 14, 21, 41, 42, 47  (2 also drives the LED, 41/42 are MTDI/MTMS)
 * plus 19/20 if you do not use the native-USB port,
 * plus 3/45/46 if you respect their reset-time levels.                    */
#define BOARD_FREE_GPIOS    { 1, 2, 14, 21, 41, 42, 47 }

/* Analog: ADC1 = GPIO1..10, ADC2 = GPIO11..20. With the camera wired, the only
 * ADC1 channels still free are GPIO1, GPIO2 and GPIO3, and GPIO14 on ADC2. */
#define BOARD_FREE_ADC1     { 1, 2, 3 }
