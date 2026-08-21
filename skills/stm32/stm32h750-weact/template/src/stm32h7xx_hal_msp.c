/**
 * Peripheral MSP (clock + pin) initialisation.
 */
#include "board.h"

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *spiHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    if (spiHandle->Instance == SPI4) {
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SPI4;
        PeriphClkInitStruct.Spi45ClockSelection  = RCC_SPI45CLKSOURCE_D2PCLK1;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
            Error_Handler();
        }

        __HAL_RCC_SPI4_CLK_ENABLE();
        __HAL_RCC_GPIOE_CLK_ENABLE();

        /* PE12 -> SPI4_SCK, PE14 -> SPI4_MOSI */
        GPIO_InitStruct.Pin       = GPIO_PIN_12 | GPIO_PIN_14;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI4;
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    }
}

void HAL_SPI_MspDeInit(SPI_HandleTypeDef *spiHandle)
{
    if (spiHandle->Instance == SPI4) {
        __HAL_RCC_SPI4_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOE, GPIO_PIN_12 | GPIO_PIN_14);
    }
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *tim_baseHandle)
{
    if (tim_baseHandle->Instance == TIM1) {
        __HAL_RCC_TIM1_CLK_ENABLE();
    }
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *tim_baseHandle)
{
    if (tim_baseHandle->Instance == TIM1) {
        __HAL_RCC_TIM1_CLK_DISABLE();
    }
}

/* ADC3 lives in the D3 domain and is clocked from PLL3R, as in the vendor SDK. */
void HAL_ADC_MspInit(ADC_HandleTypeDef *adcHandle)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    if (adcHandle->Instance == ADC3) {
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ADC;
        PeriphClkInitStruct.PLL3.PLL3M          = 10;
        PeriphClkInitStruct.PLL3.PLL3N          = 60;
        PeriphClkInitStruct.PLL3.PLL3P          = 2;
        PeriphClkInitStruct.PLL3.PLL3Q          = 2;
        PeriphClkInitStruct.PLL3.PLL3R          = 2;
        PeriphClkInitStruct.PLL3.PLL3RGE        = RCC_PLL3VCIRANGE_1;
        PeriphClkInitStruct.PLL3.PLL3VCOSEL     = RCC_PLL3VCOMEDIUM;
        PeriphClkInitStruct.PLL3.PLL3FRACN      = 0;
        PeriphClkInitStruct.AdcClockSelection   = RCC_ADCCLKSOURCE_PLL3;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
            Error_Handler();
        }

        __HAL_RCC_ADC3_CLK_ENABLE();
    }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef *adcHandle)
{
    if (adcHandle->Instance == ADC3) {
        __HAL_RCC_ADC3_CLK_DISABLE();
    }
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *timHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (timHandle->Instance == TIM1) {
        __HAL_RCC_GPIOE_CLK_ENABLE();

        /* PE10 -> TIM1_CH2N (LCD backlight) */
        GPIO_InitStruct.Pin       = LCD_BL_Pin;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
        HAL_GPIO_Init(LCD_BL_GPIO_Port, &GPIO_InitStruct);
    }
}
