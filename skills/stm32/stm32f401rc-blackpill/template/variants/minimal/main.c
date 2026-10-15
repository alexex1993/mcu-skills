/*
 * Blink for the STM32F401RCT6 Black Pill behind the WeAct HID bootloader.
 *
 *   - PC13 LED blinks at 2 Hz (non-blocking, timestamp compare);
 *   - while KEY (PA0) is held, the LED blinks fast instead (10 Hz), so the
 *     button is proven too - and holding KEY through NRST still lands in the
 *     bootloader, which blinks PC13 even faster (~8 Hz) and enumerates as HID.
 *
 * Clock: HSE 25 MHz -> PLL -> 84 MHz SYSCLK, 48 MHz USB clock (unused here,
 * kept so that adding USB later changes nothing about the tree).
 */

#include "board.h"

#define BLINK_SLOW_MS   250U
#define BLINK_FAST_MS   50U

static void SystemClock_Config(void);
static void LED_GPIO_Init(void);
static void KEY_GPIO_Init(void);

int main(void)
{
    HAL_Init();             /* SysTick at 1 ms - on OUR vector table, see boot_handoff.c */
    SystemClock_Config();
    LED_GPIO_Init();
    KEY_GPIO_Init();

    uint32_t last = HAL_GetTick();

    while (1)
    {
        uint32_t period = KEY_PRESSED() ? BLINK_FAST_MS : BLINK_SLOW_MS;

        if ((HAL_GetTick() - last) >= period)
        {
            last = HAL_GetTick();
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        }
    }
}

/*
 * 84 MHz is the F401's maximum. PLLQ must give exactly 48 MHz for USB and
 * shares the VCO with PLLP: VCO 336 -> /4 = 84 SYSCLK, /7 = 48 USB.
 *
 * HAL_RCC_DeInit() first: behind a V1.1 bootloader the PLL is already running
 * as SYSCLK, and HAL_RCC_OscConfig() refuses (HAL_ERROR -> Error_Handler) to
 * touch a PLL that drives SYSCLK unless the requested values match exactly.
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    if (HAL_RCC_DeInit() != HAL_OK)     /* back to HSI 16 MHz, PLL off */
    {
        Error_Handler();
    }

    /* F401: Scale 2 is its top range (84 MHz) and the reset value */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 25;            /* 25 MHz / 25 = 1 MHz          */
    osc.PLL.PLLN       = 336;           /* -> 336 MHz VCO (192..432)    */
    osc.PLL.PLLP       = RCC_PLLP_DIV4; /* -> 84 MHz SYSCLK             */
    osc.PLL.PLLQ       = 7;             /* -> 48 MHz for USB            */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        Error_Handler();                /* HSE did not start            */
    }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  84 MHz              */
    clk.APB1CLKDivider = RCC_HCLK_DIV2;     /* PCLK1 42 MHz (42 max)     */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;     /* PCLK2 84 MHz              */
    /* 2 wait states: 30 MHz per state at 2.7-3.6 V, 84 / 3 = 28 <= 30 */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

static void LED_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    LED_OFF();                          /* set the level before driving the pin */

    gpio.Pin   = LED_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;   /* PC13: 2 MHz / 3 mA sink max */
    HAL_GPIO_Init(LED_GPIO_Port, &gpio);
}

static void KEY_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin  = KEY_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;            /* KEY shorts to GND */
    HAL_GPIO_Init(KEY_GPIO_Port, &gpio);
}

/* Hang with the LED solid on, so a clock failure looks different from a blink.
   Register-level on purpose: it must work even if HAL state is broken. */
void Error_Handler(void)
{
    __disable_irq();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIOC->MODER = (GPIOC->MODER & ~(3U << 26)) | (1U << 26);
    GPIOC->BSRR  = GPIO_PIN_13 << 16;   /* PC13 low = lit */
    while (1)
    {
    }
}

/* framework-stm32cube ships no stm32f4xx_it.c and the startup file's weak
   SysTick_Handler is an infinite loop: without this, HAL_Delay() and every
   HAL timeout hang. */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
