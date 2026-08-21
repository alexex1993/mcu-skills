/**
 * Minimal starting point for the WeAct MiniSTM32H7xx core board (STM32H750VBT6).
 *
 * Blinks the on-board LED and mirrors the K1 button - no display, no USB, no
 * ADC - so it is the smallest thing that proves the toolchain, the clock tree
 * and the flashing route all work. Add peripherals from
 * `reference/recipes.md` in the stm32h750-weact skill.
 *
 * Clock tree: HSE 25 MHz -> PLL1 (/5 *96 /2) -> SYSCLK 240 MHz, HCLK 120 MHz.
 */
#include "board.h"

/* Declared in board.h so the shared MSP file links; unused here. */
SPI_HandleTypeDef hspi4;
TIM_HandleTypeDef htim1;

static void SystemClock_Config(void);
static void CPU_CACHE_Enable(void);
static void MX_GPIO_Init(void);

int main(void)
{
    uint32_t last_blink = 0;
    uint32_t period_ms  = 500;

    CPU_CACHE_Enable();      /* before HAL_Init, so the caches start coherent */
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();

    for (;;) {
        uint32_t now = HAL_GetTick();

        /* K1 shorts to VDD through R8, so pressed reads HIGH. */
        period_ms = (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET) ? 100 : 500;

        if ((now - last_blink) >= period_ms) {
            last_blink = now;
            /* PE3 drives a PDTC114ET - the LED is ACTIVE HIGH. */
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        }
    }
}

static void CPU_CACHE_Enable(void)
{
    SCB_EnableICache();
    SCB_EnableDCache();
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    /* HSI48 feeds the USB FS PHY (same source the vendor SDK uses). */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI48;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.HSI48State     = RCC_HSI48_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 5;
    RCC_OscInitStruct.PLL.PLLN       = 96;
    RCC_OscInitStruct.PLL.PLLP       = 2;
    RCC_OscInitStruct.PLL.PLLQ       = 2;
    RCC_OscInitStruct.PLL.PLLR       = 2;
    RCC_OscInitStruct.PLL.PLLRGE     = RCC_PLL1VCIRANGE_2;
    RCC_OscInitStruct.PLL.PLLVCOSEL  = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN   = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                                       RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin   = LED_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = KEY_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;   /* K1 pulls UP to VDD when pressed */
    HAL_GPIO_Init(KEY_GPIO_Port, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    __disable_irq();
    for (;;) {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
