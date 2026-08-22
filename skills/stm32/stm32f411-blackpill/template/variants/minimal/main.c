/*
 * Minimal Black Pill firmware: blink PC13, and light it solid while K1 is held.
 *
 * Same clock tree and startup order as the full variant, nothing else. This is
 * the smallest thing that proves toolchain, crystal, clocks and the DFU route
 * all work - flash this first when a board is new or a build has gone strange.
 */

#include "board.h"

#define BLINK_PERIOD_MS 100U

static void SystemClock_Config(void);
static void LED_GPIO_Init(void);
static void KEY_GPIO_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    LED_GPIO_Init();
    KEY_GPIO_Init();

    uint32_t last = HAL_GetTick();

    while (1)
    {
        if (KEY_PRESSED())
        {
            LED_ON();
            last = HAL_GetTick();
            continue;
        }

        if ((HAL_GetTick() - last) >= BLINK_PERIOD_MS)
        {
            last += BLINK_PERIOD_MS;
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        }
    }
}

/*
 * 96 MHz rather than the chip's 100 MHz maximum, so that PLLQ can also produce
 * the exact 48 MHz USB_OTG_FS needs. Keeping it here even in the minimal build
 * means adding USB later changes nothing about the clock tree.
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);  /* > 84 MHz */

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 25;            /* 25 MHz / 25 = 1 MHz   */
    osc.PLL.PLLN       = 192;           /* -> 192 MHz VCO        */
    osc.PLL.PLLP       = RCC_PLLP_DIV2; /* -> 96 MHz SYSCLK      */
    osc.PLL.PLLQ       = 4;             /* -> 48 MHz for USB     */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        Error_Handler();    /* HSE did not start: crystal or its caps */
    }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;     /* PCLK1 max is 50 MHz */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_3) != HAL_OK)
    {
        Error_Handler();
    }
}

static void LED_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    LED_OFF();

    gpio.Pin   = LED_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;   /* PC13 is limited to 2 MHz / 3 mA */
    HAL_GPIO_Init(LED_GPIO_Port, &gpio);
}

static void KEY_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin  = KEY_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;    /* K1 pulls to GND; nothing else holds it high */
    HAL_GPIO_Init(KEY_GPIO_Port, &gpio);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

/* framework-stm32cube ships no stm32f4xx_it.c, and the startup file's weak
   SysTick_Handler is an infinite loop. Omit this and the first HAL_Delay()
   or HAL_GetTick() comparison hangs the board. */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
