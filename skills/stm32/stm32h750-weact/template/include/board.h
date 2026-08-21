/**
 * Board definitions for the WeAct Studio MiniSTM32H7xx core board
 * populated with an STM32H750VBT6 and the 0.96" 160x80 ST7735 TFT.
 *
 * Pin mapping taken from the vendor SDK (SDK/HAL/STM32H750/03-LCD_Test):
 *   PE12  SPI4_SCK    -> LCD SCL
 *   PE14  SPI4_MOSI   -> LCD SDA   (SPI is used half-duplex, transmit only)
 *   PE11  LCD_CS
 *   PE13  LCD_DC      (register select, "WR/RS" in the schematic)
 *   PE10  TIM1_CH2N   -> LCD backlight (PWM)
 *   PE3   user LED
 *   PC13  user key K1
 * The panel reset line is tied to the MCU reset, there is no GPIO for it.
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32h7xx_hal.h"

#define LED_Pin                 GPIO_PIN_3
#define LED_GPIO_Port           GPIOE

#define KEY_Pin                 GPIO_PIN_13
#define KEY_GPIO_Port           GPIOC

#define LCD_CS_Pin              GPIO_PIN_11
#define LCD_CS_GPIO_Port        GPIOE

#define LCD_DC_Pin              GPIO_PIN_13
#define LCD_DC_GPIO_Port        GPIOE

#define LCD_BL_Pin              GPIO_PIN_10
#define LCD_BL_GPIO_Port        GPIOE
#define LCD_BL_TIM_CHANNEL      TIM_CHANNEL_2   /* TIM1_CH2N */

extern SPI_HandleTypeDef hspi4;
extern TIM_HandleTypeDef htim1;

void Error_Handler(void);
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *timHandle);

#endif /* BOARD_H */
