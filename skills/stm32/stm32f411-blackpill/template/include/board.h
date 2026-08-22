/**
 * Board definitions for the WeAct Studio MiniSTM32F4x1 "Black Pill" V3.x
 * populated with an STM32F411CEU6 (UFQFPN48).
 *
 *   PC13  user LED    -- ANODE to 3V3, cathode to the pin: LOW = lit
 *   PA0   user key K1 -- shorts to GND when pressed: LOW = pressed
 *   PH0/PH1    25 MHz HSE crystal      (OSC_IN / OSC_OUT)
 *   PC14/PC15  32.768 kHz LSE crystal  (OSC32_IN / OSC32_OUT)
 *   PA11/PA12  USB_OTG_FS DM/DP on the USB-C connector (AF10)
 *   PA13/PA14  SWDIO / SWCLK on the 4-pin debug header
 *   PA4-PA7    SPI1, routed to the unpopulated SOIC-8 flash footprint (U4)
 *
 * PC13/PC14/PC15 sit behind the backup-domain power switch and can sink at
 * most 3 mA at <= 2 MHz (datasheet Table 8, note 2). Sinking the LED is fine;
 * sourcing current from them is not.
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f4xx_hal.h"

#define LED_Pin                 GPIO_PIN_13
#define LED_GPIO_Port           GPIOC
#define LED_ON()                HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET)
#define LED_OFF()               HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET)

/* K1 has no external pull-up fitted on every board revision - always enable
   the internal one, or the input floats and reads random. */
#define KEY_Pin                 GPIO_PIN_0
#define KEY_GPIO_Port           GPIOA
#define KEY_PRESSED()           (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_RESET)

void Error_Handler(void);

#endif /* BOARD_H */
