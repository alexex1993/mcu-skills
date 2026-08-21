// Waveshare ESP32-C6-LCD-1.47 (SKU 28563 / 30381) board definition.
//
// Every pin here comes from the Waveshare wiki pinout tables. GPIO10, GPIO11
// and GPIO24-GPIO30 do not exist on the ESP32-C6FH4's QFN32 package, so the
// board has 22 GPIOs to work with and consumes 10 of them.
#pragma once

// ---- LCD (ST7789, 172x320, write-only: no MISO to the panel) ---------------
#define BOARD_PIN_LCD_MOSI 6    // shared with the TF card slot
#define BOARD_PIN_LCD_SCLK 7    // shared with the TF card slot
#define BOARD_PIN_LCD_CS   14
#define BOARD_PIN_LCD_DC   15
#define BOARD_PIN_LCD_RST  21
#define BOARD_PIN_LCD_BL   22   // drive with LEDC PWM, never a plain high level

// ---- TF card slot (SPI mode only: SD_D1/SD_D2 are not connected) -----------
#define BOARD_PIN_SD_CS    4
#define BOARD_PIN_SD_MISO  5
#define BOARD_PIN_SD_MOSI  BOARD_PIN_LCD_MOSI
#define BOARD_PIN_SD_SCLK  BOARD_PIN_LCD_SCLK

// ---- Addressable RGB LED (WS2812 family, one data wire) --------------------
// GPIO8 is a strapping pin (boot mode + ROM message printing). Safe to drive at
// factory eFuse settings; see reference/board-hardware.md.
#define BOARD_PIN_RGB_LED  8

// ---- Buttons ---------------------------------------------------------------
#define BOARD_PIN_BOOT     9    // BOOT button, weak pull-up, pressed = low
                                // RESET acts on CHIP_PU, not on any GPIO

// ---- USB Serial/JTAG (console + flashing + debug, all over the Type-C) -----
#define BOARD_PIN_USB_DM   12
#define BOARD_PIN_USB_DP   13

// Free and unencumbered: GPIO0, 1, 2, 3, 18, 19, 20, 23.
// GPIO0-GPIO3 are also the only remaining ADC1 channels (CH0-CH3) and the only
// remaining LP GPIOs, i.e. deep-sleep wake sources. Spend them deliberately.

// ---- Panel geometry --------------------------------------------------------
#define BOARD_LCD_H_RES    172
#define BOARD_LCD_V_RES    320
// The 172-pixel glass is centred in the ST7789's 240-column RAM: (240-172)/2.
#define BOARD_LCD_X_GAP    34
#define BOARD_LCD_Y_GAP    0
// GPIO-Matrix-routed SPI (the board's MOSI/SCLK are swapped relative to the
// IO MUX fast path), so ~40 MHz is the practical ceiling.
#define BOARD_LCD_PIXEL_CLK_HZ (40 * 1000 * 1000)
