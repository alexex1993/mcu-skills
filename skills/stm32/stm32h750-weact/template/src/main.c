/**
 * "Hello World" on the 0.96" ST7735 TFT of the WeAct Studio
 * MiniSTM32H7xx core board (STM32H750VBT6), STM32Cube HAL.
 *
 * Clock tree matches the vendor SDK: HSE 25 MHz -> PLL1 (/5 *96 /2)
 * -> SYSCLK 240 MHz, HCLK 120 MHz.
 *
 * The UI runs as a fixed 120 Hz frame loop, paced off the DWT cycle counter
 * (SysTick's 1 ms granularity cannot express the 8.333 ms period).
 */
#include <stdio.h>

#include "board.h"
#include "lcd.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "temp_sensor.h"

SPI_HandleTypeDef hspi4;
TIM_HandleTypeDef htim1;

static void SystemClock_Config(void);
static void CPU_CACHE_Enable(void);
static void MX_GPIO_Init(void);
static void MX_SPI4_Init(void);
static void MX_TIM1_Init(void);
static void DWT_Init(void);

#define FRAME_HZ        120U
#define UI_TICK_Y        32U   /* screen rows: the strip we repaint every frame */
#define UI_TEMP_Y        48U
#define UI_METER_Y       64U
#define FONT_H           16U

int main(void)
{
    uint32_t frame_cycles;
    uint32_t next_frame;
    uint32_t frames        = 0;      /* frames drawn since the last fps window */
    uint32_t fps           = 0;      /* measured, displayed */
    uint32_t render_us     = 0;      /* how long one frame's drawing took */
    uint32_t last_fps_calc = 0;
    uint32_t last_report;
    uint32_t last_temp = 0;
    uint32_t last_led  = 0;
    uint32_t seq       = 0;
    int32_t  temp10    = TEMP_SENSOR_INVALID;

    CPU_CACHE_Enable();

    HAL_Init();
    SystemClock_Config();
    DWT_Init();

    MX_GPIO_Init();
    MX_SPI4_Init();
    MX_TIM1_Init();
    MX_USB_DEVICE_Init();
    MX_ADC3_Init();

    LCD_Init();
    /* Push the panel's own refresh from the driver's stock ~80 Hz to ~130 Hz,
       so it is no longer the thing that limits what we can show. */
    LCD_SetFrameRate(LCD_FRAMERATE_FAST);
    LCD_Clear(BLACK);

    /* The two static lines: drawn once, never touched again. */
    LCD_StripClear(BLACK);
    LCD_StripText(4, 0, FONT_H, WHITE, "Hello, Threads");
    LCD_StripFlush(0, FONT_H);

    LCD_StripClear(BLACK);
    LCD_StripText(4, 0, FONT_H, CYAN, "STM32H750VBT6");
    LCD_StripFlush(16, FONT_H);

    /* Fade the backlight in once the frame is on screen. */
    LCD_Light(100, 300);

    frame_cycles = SystemCoreClock / FRAME_HZ;
    next_frame   = DWT->CYCCNT;
    last_report  = HAL_GetTick();

    for (;;) {
        uint32_t now, render_start;
        char text[32];

        /* Wait out the rest of the frame. Signed compare, so the 17.9 s
           CYCCNT wrap costs nothing. */
        while ((int32_t)(DWT->CYCCNT - next_frame) < 0) {
        }
        next_frame += frame_cycles;
        if ((int32_t)(DWT->CYCCNT - next_frame) > (int32_t)frame_cycles) {
            next_frame = DWT->CYCCNT + frame_cycles;  /* overran badly - resync */
        }

        now          = HAL_GetTick();
        render_start = DWT->CYCCNT;

        /* --- every frame: the tick counter ---------------------------- */
        LCD_StripClear(BLACK);
        snprintf(text, sizeof(text), "Tick:%8lu ms", (unsigned long)now);
        LCD_StripText(4, 0, FONT_H, GRAY, text);
        LCD_StripFlush(UI_TICK_Y, FONT_H);

        /* --- every frame: fps readout and a sweeping marker ------------ */
        {
            /* 1 s there, 1 s back across the free width */
            uint32_t phase  = now % 2000U;
            uint32_t travel = (phase < 1000U) ? phase : (2000U - phase);
            uint16_t bar_x  = (uint16_t)(travel * (LCD_STRIP_MAX_W - 12U) / 1000U);

            LCD_StripClear(BLACK);
            snprintf(text, sizeof(text), "%3lu fps %2lu.%01lums",
                     (unsigned long)fps,
                     (unsigned long)(render_us / 1000U),
                     (unsigned long)((render_us % 1000U) / 100U));
            LCD_StripText(4, 0, FONT_H, GREEN, text);
            LCD_StripRect(bar_x, 13, 12, 3, MAGENTA);
            LCD_StripFlush(UI_METER_Y, FONT_H);
        }

        render_us = (DWT->CYCCNT - render_start) / (SystemCoreClock / 1000000U);
        frames++;

        if ((now - last_fps_calc) >= 1000U) {
            fps           = frames * 1000U / (now - last_fps_calc);
            frames        = 0;
            last_fps_calc = now;
        }

        if ((now - last_led) >= 200U) {
            last_led = now;
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        }

        /* --- twice a second: the ADC read and the temperature line ----- */
        if ((now - last_temp) >= 500U) {
            last_temp = now;
            temp10    = TempSensor_ReadDeciC();

            LCD_StripClear(BLACK);
            if (temp10 == TEMP_SENSOR_INVALID) {
                snprintf(text, sizeof(text), "Temp: --.- C");
            } else {
                snprintf(text, sizeof(text), "Temp: %ld.%ld C",
                         (long)(temp10 / 10), (long)(temp10 % 10 < 0 ? -(temp10 % 10) : temp10 % 10));
            }
            LCD_StripText(4, 0, FONT_H, YELLOW, text);
            LCD_StripFlush(UI_TEMP_Y, FONT_H);
        }

        /* Report over the USB virtual COM port every 5 seconds. */
        if ((now - last_report) >= 5000U) {
            char line[128];
            char temp_str[16];
            int len;

            last_report += 5000U;
            if ((now - last_report) >= 5000U) {
                last_report = now;   /* fell far behind - resync instead of catching up */
            }
            seq++;

            if (temp10 == TEMP_SENSOR_INVALID) {
                snprintf(temp_str, sizeof(temp_str), "--.-");
            } else {
                snprintf(temp_str, sizeof(temp_str), "%ld.%ld",
                         (long)(temp10 / 10), (long)(temp10 % 10 < 0 ? -(temp10 % 10) : temp10 % 10));
            }

            len = snprintf(line, sizeof(line),
                           "[%3lu.%03lu] #%lu Hello World | uptime %lus | temp %s C | LCD %lux%lu @ %lu fps\r\n",
                           (unsigned long)(now / 1000U), (unsigned long)(now % 1000U),
                           (unsigned long)seq, (unsigned long)(now / 1000U), temp_str,
                           (unsigned long)ST7735Ctx.Width,
                           (unsigned long)ST7735Ctx.Height,
                           (unsigned long)fps);
            if (len > 0) {
                CDC_Transmit_FS((uint8_t *)line, (uint16_t)len);
            }
        }
    }
}

/* Free-running cycle counter at CPU clock (240 MHz) - the frame clock. */
static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->LAR          = 0xC5ACCE55U;   /* Cortex-M7 needs the unlock */
    DWT->CYCCNT       = 0;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;
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
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOE, LCD_CS_Pin | LCD_DC_Pin, GPIO_PIN_SET);

    GPIO_InitStruct.Pin   = LED_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = KEY_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(KEY_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = LCD_CS_Pin | LCD_DC_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}

/* SPI4 drives the panel half-duplex: only SCK (PE12) and MOSI (PE14) are wired. */
static void MX_SPI4_Init(void)
{
    hspi4.Instance                        = SPI4;
    hspi4.Init.Mode                       = SPI_MODE_MASTER;
    hspi4.Init.Direction                  = SPI_DIRECTION_1LINE;
    hspi4.Init.DataSize                   = SPI_DATASIZE_8BIT;
    hspi4.Init.CLKPolarity                = SPI_POLARITY_LOW;
    hspi4.Init.CLKPhase                   = SPI_PHASE_1EDGE;
    hspi4.Init.NSS                        = SPI_NSS_SOFT;
    /* SPI4 kernel clock is D2PCLK1 = 120 MHz. /4 -> 30 MHz: above the ST7735
       datasheet 66 ns write cycle, but the panel sits on short on-board
       traces and takes it. Drop back to _8 if pixels ever come out garbled. */
    hspi4.Init.BaudRatePrescaler          = SPI_BAUDRATEPRESCALER_4;
    hspi4.Init.FirstBit                   = SPI_FIRSTBIT_MSB;
    hspi4.Init.TIMode                     = SPI_TIMODE_DISABLE;
    hspi4.Init.CRCCalculation             = SPI_CRCCALCULATION_DISABLE;
    hspi4.Init.CRCPolynomial              = 0x0;
    hspi4.Init.NSSPMode                   = SPI_NSS_PULSE_ENABLE;
    hspi4.Init.NSSPolarity                = SPI_NSS_POLARITY_LOW;
    hspi4.Init.FifoThreshold              = SPI_FIFO_THRESHOLD_01DATA;
    hspi4.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    hspi4.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    hspi4.Init.MasterSSIdleness           = SPI_MASTER_SS_IDLENESS_00CYCLE;
    hspi4.Init.MasterInterDataIdleness    = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    hspi4.Init.MasterReceiverAutoSusp     = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    hspi4.Init.MasterKeepIOState          = SPI_MASTER_KEEP_IO_STATE_DISABLE;
    hspi4.Init.IOSwap                     = SPI_IO_SWAP_DISABLE;
    if (HAL_SPI_Init(&hspi4) != HAL_OK) {
        Error_Handler();
    }
}

/* TIM1_CH2N on PE10 dims the backlight: 10 MHz / 1000 = 10 kHz, duty 0..100. */
static void MX_TIM1_Init(void)
{
    TIM_ClockConfigTypeDef        sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef       sMasterConfig      = {0};
    TIM_OC_InitTypeDef            sConfigOC          = {0};
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

    htim1.Instance               = TIM1;
    htim1.Init.Prescaler         = 12 - 1;
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim1.Init.Period            = 1000 - 1;
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim1) != HAL_OK) {
        Error_Handler();
    }

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) {
        Error_Handler();
    }

    sMasterConfig.MasterOutputTrigger  = TIM_TRGO_RESET;
    sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    sMasterConfig.MasterSlaveMode      = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK) {
        Error_Handler();
    }

    sConfigOC.OCMode       = TIM_OCMODE_PWM1;
    sConfigOC.Pulse        = 0;
    sConfigOC.OCPolarity   = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode   = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState  = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, LCD_BL_TIM_CHANNEL) != HAL_OK) {
        Error_Handler();
    }

    sBreakDeadTimeConfig.OffStateRunMode  = TIM_OSSR_DISABLE;
    sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
    sBreakDeadTimeConfig.LockLevel        = TIM_LOCKLEVEL_OFF;
    sBreakDeadTimeConfig.DeadTime         = 0;
    sBreakDeadTimeConfig.BreakState       = TIM_BREAK_DISABLE;
    sBreakDeadTimeConfig.BreakPolarity    = TIM_BREAKPOLARITY_HIGH;
    sBreakDeadTimeConfig.BreakFilter      = 0;
    sBreakDeadTimeConfig.Break2State      = TIM_BREAK2_DISABLE;
    sBreakDeadTimeConfig.Break2Polarity   = TIM_BREAK2POLARITY_HIGH;
    sBreakDeadTimeConfig.Break2Filter     = 0;
    sBreakDeadTimeConfig.AutomaticOutput  = TIM_AUTOMATICOUTPUT_DISABLE;
    if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK) {
        Error_Handler();
    }

    HAL_TIM_MspPostInit(&htim1);
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
