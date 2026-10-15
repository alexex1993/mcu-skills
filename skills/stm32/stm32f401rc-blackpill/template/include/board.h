/**
 * Board definitions for the 40-pin "Black Pill"-format board populated with an
 * STM32F401RCT6 (LQFP64, 256 KB flash, 64 KB SRAM) and shipped with the WeAct
 * Studio HID bootloader.
 *
 *   PC13  user LED  -- lit by a LOW level (the bootloader's "off" is SET)
 *   PA0   user KEY  -- shorts to GND: LOW = pressed; internal pull-up required
 *   PH0/PH1    25 MHz HSE crystal (USB enumerating at all proves the 25 MHz)
 *   PC14/PC15  32.768 kHz LSE crystal
 *   PA11/PA12  USB_OTG_FS DM/DP on the USB-C connector
 *   PA13/PA14  SWDIO / SWCLK
 *
 * Flash map with the bootloader in place:
 *   0x08000000  WeAct HID bootloader, sector 0 (16 KB) - do not overwrite;
 *               V1.2 keeps this chip's UID at 0x08003A28 inside it
 *   0x08004000  application vector table (ld/STM32F401RCTX_APP_0x08004000.ld)
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f4xx_hal.h"

#define LED_Pin                 GPIO_PIN_13
#define LED_GPIO_Port           GPIOC
#define LED_ON()                HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET)
#define LED_OFF()               HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET)

#define KEY_Pin                 GPIO_PIN_0
#define KEY_GPIO_Port           GPIOA
#define KEY_PRESSED()           (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_RESET)

void Error_Handler(void);

#endif /* BOARD_H */
