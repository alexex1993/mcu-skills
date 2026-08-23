# WeAct STM32H750VBT6 — Working Code Recipes

Every block below is taken verbatim from firmware that builds and runs on this board
(PlatformIO + STM32Cube HAL: USB CDC console + ST7735 UI at 120 Hz + internal temperature sensor).
Pin names come from `board.h` in §2.

Contents: 1 project setup · 2 board.h · 3 startup and clocks · 4 GPIO · 5 SPI4 · 6 backlight PWM ·
7 MSP · 8 LCD glue · 9 fast LCD rendering · 10 ADC temperature · 11 USB CDC · 12 frame pacing ·
13 DAC1 (both channels) clocked from TIM6, no DMA.

---

## 1. `platformio.ini`

```ini
; PlatformIO Project Configuration File
;
; WeAct Studio MiniSTM32H7xx core board (STM32H750VBT6) with the
; 0.96" 160x80 ST7735 TFT, built against the STM32Cube HAL.
;
; https://docs.platformio.org/page/projectconf.html

[env:weact_mini_h750vbtx]
platform = ststm32
board = weact_mini_h750vbtx
framework = stm32cube

; The STM32H750VBT6 has 128 KB of internal flash and 512 KB of contiguous
; AXI SRAM (RAM_D1, where the linker script places .data/.bss/stack).
; The stock board definition reports these swapped, which makes PlatformIO
; print a bogus usage percentage, so correct it here.
board_upload.maximum_size = 131072
board_upload.maximum_ram_size = 524288

; Skip the BSP/Utilities/USB libraries shipped with framework-stm32cubeh7:
; they contain their own ST7735 component, which would clash with lib/ST7735.
board_build.stm32cube.disable_embedded_libs = yes

; HSE_VALUE: the core board is fitted with a 25 MHz crystal.
build_flags =
    -DHSE_VALUE=25000000
    -Wall

; Flashed over USB-C through the STM32 ROM DFU bootloader (VID 0483, PID DF11),
; so no external probe is needed. The stock board definition doesn't list dfu
; among its protocols, hence the override.
board_upload.protocols =
    dfu
    stlink
    jlink
    cmsis-dap
    blackmagic
upload_protocol = dfu

debug_tool = stlink

; The firmware enumerates as a USB CDC virtual COM port on that same USB-C
; cable, so `pio device monitor` needs no extra hardware. The baud rate is
; ignored - the link is virtual.
monitor_speed = 115200
monitor_dtr = 1
```

---

## 2. `board.h` — the file to copy into the next project

```c
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
```

---

## 3. Startup order, cache, clocks

```c
int main(void)
{
    CPU_CACHE_Enable();      /* BEFORE HAL_Init */
    HAL_Init();
    SystemClock_Config();
    DWT_Init();              /* §12, if you need sub-millisecond timing */

    MX_GPIO_Init();
    MX_SPI4_Init();
    MX_TIM1_Init();
    MX_USB_DEVICE_Init();
    MX_ADC3_Init();

    LCD_Init();
    /* ...application... */
}

static void CPU_CACHE_Enable(void)
{
    SCB_EnableICache();
    SCB_EnableDCache();
}
```

HSE 25 MHz → PLL1 (M5, N96, P2) → SYSCLK 240 MHz, HCLK 120 MHz, all APBs 120 MHz.

```c
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
```

---

## 4. GPIO: LED, button, LCD control lines

Note the polarities — both are the opposite of the usual convention on this board.

```c
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
```

---

## 5. SPI4 (LCD bus, half-duplex transmit-only)

```c
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
```

---

## 6. Backlight PWM on TIM1_CH2N

`CH2N` is a **complementary** output: it needs `HAL_TIMEx_PWMN_Start()`, a break/dead-time config,
and `HAL_TIM_MspPostInit()` for the pin. The CCR range is `0..Period`, i.e. 0..1000 here.

```c
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
```

Start it and set brightness:

```c
HAL_TIMEx_PWMN_Start(&htim1, LCD_BL_TIM_CHANNEL);          /* NOT HAL_TIM_PWM_Start */
__HAL_TIM_SET_COMPARE(&htim1, LCD_BL_TIM_CHANNEL, level);  /* level: 0..1000 */
```

---

## 7. `stm32h7xx_hal_msp.c` — clock sources and pin AF

Includes the SPI4 kernel clock selection (`D2PCLK1` = 120 MHz) and the ADC3 PLL3 tree.

```c
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
```

---

## 8. LCD glue: ST7735 bus IO over SPI4

```c
/* --- ST7735 bus IO ------------------------------------------------------- */

static int32_t lcd_io_init(void)
{
    /* Backlight PWM. CH2N is a complementary output, hence PWMN_Start. */
    HAL_TIMEx_PWMN_Start(&htim1, LCD_BL_TIM_CHANNEL);
    LCD_SetBrightness(0);
    return ST7735_OK;
}

static int32_t lcd_io_gettick(void)
{
    return (int32_t)HAL_GetTick();
}

static int32_t lcd_io_writereg(uint8_t reg, uint8_t *pdata, uint32_t length)
{
    int32_t result;

    LCD_CS_ACTIVE();
    LCD_DC_CMD();
    result = HAL_SPI_Transmit(LCD_SPI, &reg, 1, 100);
    LCD_DC_DATA();
    if (length > 0) {
        result += HAL_SPI_Transmit(LCD_SPI, pdata, (uint16_t)length, 500);
    }
    LCD_CS_IDLE();

    return (result != 0) ? -1 : 0;
}

static int32_t lcd_io_readreg(uint8_t reg, uint8_t *pdata)
{
    int32_t result;

    LCD_CS_ACTIVE();
    LCD_DC_CMD();
    result = HAL_SPI_Transmit(LCD_SPI, &reg, 1, 100);
    LCD_DC_DATA();
    result += HAL_SPI_Receive(LCD_SPI, pdata, 1, 500);
    LCD_CS_IDLE();

    return (result != 0) ? -1 : 0;
}

static int32_t lcd_io_senddata(uint8_t *pdata, uint32_t length)
{
    int32_t result;

    LCD_CS_ACTIVE();
    result = HAL_SPI_Transmit(LCD_SPI, pdata, (uint16_t)length, 100);
    LCD_CS_IDLE();

    return (result != 0) ? -1 : 0;
}

static int32_t lcd_io_recvdata(uint8_t *pdata, uint32_t length)
{
    int32_t result;

    LCD_CS_ACTIVE();
    result = HAL_SPI_Receive(LCD_SPI, pdata, (uint16_t)length, 500);
    LCD_CS_IDLE();

    return (result != 0) ? -1 : 0;
}
```

---

## 9. Fast LCD rendering — the strip technique

The problem: `ST7735_FillRGBRect()` calls `SetCursor()` once per pixel row, and `SetCursor()` sends
each address byte as a separate CS-framed SPI transaction (7 transactions per call). One 16-pixel
glyph ≈ 128 transactions; a 13-character line ≈ 1660. Bus clock is irrelevant at that point.

The fix: the panel's address window is left at full screen by `Init()`/`FillRect()`, so one
`SetCursor(0, y)` plus one continuous burst lets the controller's address counter walk the rows.
A 160×16 band = 5120 bytes = **8 transactions**, ~1.4 ms at 30 MHz.

Pixels are stored **byte-swapped** at composition time: the panel wants big-endian RGB565.

Header:

```c
#define LCD_STRIP_MAX_W  160
#define LCD_STRIP_MAX_H  16

void LCD_StripClear(uint16_t color);
void LCD_StripRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void LCD_StripText(uint16_t x, uint16_t y, uint8_t size, uint16_t color, const char *p);
void LCD_StripFlush(uint16_t y, uint16_t height);

/* Panel refresh: f = f_osc / ((rtna*2 + 40) * (LINE + fpa + bpa)),
   f_osc ~= 850 kHz, LINE = 160 gate lines. */
#define LCD_FRAMERATE_DEFAULT   0x01U, 0x2CU, 0x2DU   /* ~80 Hz  (driver stock) */
#define LCD_FRAMERATE_FAST      0x00U, 0x02U, 0x02U   /* ~130 Hz */
void LCD_SetFrameRate(uint8_t rtna, uint8_t fpa, uint8_t bpa);
```

Implementation:

```c
/* --- strip renderer ------------------------------------------------------ */

/* Pixels are stored byte-swapped so the buffer can go straight out on the
   wire: the panel wants RGB565 big-endian, the M7 is little-endian. */
#define LCD_SWAP16(c)  ((uint16_t)(((uint16_t)(c) << 8) | ((uint16_t)(c) >> 8)))

static uint16_t lcd_strip[LCD_STRIP_MAX_W * LCD_STRIP_MAX_H];

void LCD_StripClear(uint16_t color)
{
    uint16_t raw = LCD_SWAP16(color);
    uint32_t i;

    for (i = 0; i < (uint32_t)(LCD_STRIP_MAX_W * LCD_STRIP_MAX_H); i++) {
        lcd_strip[i] = raw;
    }
}

void LCD_StripRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint16_t raw = LCD_SWAP16(color);
    uint16_t row, col;

    for (row = y; row < y + h && row < LCD_STRIP_MAX_H; row++) {
        for (col = x; col < x + w && col < LCD_STRIP_MAX_W; col++) {
            lcd_strip[row * LCD_STRIP_MAX_W + col] = raw;
        }
    }
}

/**
 * Draw a string into the strip at (x,y), foreground pixels only - whatever
 * LCD_StripClear() painted shows through as the background.
 * size: 12 or 16 (glyph height; width is size/2)
 */
void LCD_StripText(uint16_t x, uint16_t y, uint8_t size, uint16_t color, const char *p)
{
    uint16_t raw = LCD_SWAP16(color);
    uint8_t  cw  = (uint8_t)(size / 2);

    for (; *p >= ' ' && *p <= '~'; p++, x = (uint16_t)(x + cw)) {
        uint8_t num = (uint8_t)(*p - ' ');
        uint8_t col, row;

        if (x + cw > LCD_STRIP_MAX_W) {
            break;
        }

        for (col = 0; col < cw; col++) {
            for (row = 0; row < size; row++) {
                /* font byte layout: column-major, 8 rows per byte */
                uint8_t idx  = (uint8_t)(col * 2U + (row >> 3));
                uint8_t bits = (size == 12) ? asc2_1206[num][idx] : asc2_1608[num][idx];

                if ((bits & (0x80U >> (row & 7U))) != 0U &&
                    (y + row) < LCD_STRIP_MAX_H) {
                    lcd_strip[(y + row) * LCD_STRIP_MAX_W + (x + col)] = raw;
                }
            }
        }
    }
}

/**
 * Push the top `height` rows of the strip to panel rows y .. y+height-1.
 *
 * The panel's address window is left at full screen by Init/FillRect, so a
 * single SetCursor plus one continuous burst lets the controller's address
 * counter walk the rows for us - no per-row command overhead.
 */
void LCD_StripFlush(uint16_t y, uint16_t height)
{
    if (height > LCD_STRIP_MAX_H || (y + height) > ST7735Ctx.Height) {
        return;
    }

    ST7735_SetCursor(&st7735_pObj, 0, y);
    lcd_io_senddata((uint8_t *)lcd_strip, (uint32_t)ST7735Ctx.Width * height * 2U);
}

/* Panel refresh rate, FRMCTR1 (normal mode). See lcd.h for the formula. */
void LCD_SetFrameRate(uint8_t rtna, uint8_t fpa, uint8_t bpa)
{
    uint8_t args[3] = { rtna, fpa, bpa };

    lcd_io_writereg(ST7735_FRAME_RATE_CTRL1, args, sizeof(args));
}
```

Usage — one composed band, one blit:

```c
LCD_StripClear(BLACK);
LCD_StripText(4, 0, 16, GRAY, "Tick:   12345 ms");
LCD_StripRect(bar_x, 13, 12, 3, MAGENTA);
LCD_StripFlush(32, 16);            /* screen rows 32..47 */
```

The bundled font is column-major, 8 rows per byte: pixel `(col,row)` of a glyph is bit
`0x80 >> (row & 7)` of byte `col*2 + (row >> 3)`.

---

## 10. Internal temperature sensor on ADC3

ADC3 lives in the D3 domain and is clocked from PLL3R (see the MSP in §7): 75 MHz, `/4` → 18.75 MHz.

```c
void MX_ADC3_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    hadc3.Instance                      = ADC3;
    hadc3.Init.ClockPrescaler           = ADC_CLOCK_ASYNC_DIV4;
    hadc3.Init.Resolution               = ADC_RESOLUTION_16B;
    hadc3.Init.ScanConvMode             = ADC_SCAN_DISABLE;
    hadc3.Init.EOCSelection             = ADC_EOC_SINGLE_CONV;
    hadc3.Init.LowPowerAutoWait         = DISABLE;
    hadc3.Init.ContinuousConvMode       = DISABLE;
    hadc3.Init.NbrOfConversion          = 1;
    hadc3.Init.DiscontinuousConvMode    = DISABLE;
    hadc3.Init.ExternalTrigConv         = ADC_SOFTWARE_START;
    hadc3.Init.ExternalTrigConvEdge     = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc3.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
    hadc3.Init.Overrun                  = ADC_OVR_DATA_PRESERVED;
    hadc3.Init.LeftBitShift             = ADC_LEFTBITSHIFT_NONE;
    hadc3.Init.OversamplingMode         = DISABLE;
    if (HAL_ADC_Init(&hadc3) != HAL_OK) {
        Error_Handler();
    }

    sConfig.Channel      = ADC_CHANNEL_TEMPSENSOR;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_387CYCLES_5;
    sConfig.SingleDiff   = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset       = 0;
    if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_ADCEx_Calibration_Start(&hadc3, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK) {
        Error_Handler();
    }
}
```

Reading it, scaled by VREFINT so the result does not depend on the actual rail voltage, and
interpolated against the factory calibration. **`cal2 == cal1` must be checked** — a blank
calibration area would divide by zero.

```c
/* One single conversion of `channel`, returned raw. 0 on failure. */
static uint32_t adc3_read(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    uint32_t value = 0;

    sConfig.Channel      = channel;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_387CYCLES_5;
    sConfig.SingleDiff   = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset       = 0;
    if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK) {
        return 0;
    }

    if (HAL_ADC_Start(&hadc3) != HAL_OK) {
        return 0;
    }
    if (HAL_ADC_PollForConversion(&hadc3, 100) == HAL_OK) {
        value = HAL_ADC_GetValue(&hadc3);
    }
    HAL_ADC_Stop(&hadc3);

    return value;
}

int32_t TempSensor_ReadDeciC(void)
{
    uint32_t vrefint_data;
    uint32_t ts_data;
    uint32_t vdda_mv;
    int32_t  cal1, cal2, ts_scaled;

    /* VREFINT first: it turns the raw temperature reading into something
       independent of how far the actual 3V3 rail sits from nominal. */
    vrefint_data = adc3_read(ADC_CHANNEL_VREFINT);
    ts_data      = adc3_read(ADC_CHANNEL_TEMPSENSOR);

    if (vrefint_data == 0 || ts_data == 0) {
        return TEMP_SENSOR_INVALID;
    }

    vdda_mv = __HAL_ADC_CALC_VREFANALOG_VOLTAGE(vrefint_data, ADC_RESOLUTION_16B);

    cal1 = (int32_t)(*TEMPSENSOR_CAL1_ADDR);
    cal2 = (int32_t)(*TEMPSENSOR_CAL2_ADDR);
    if (cal2 == cal1) {
        /* Blank or unprogrammed calibration area - no sensible result. */
        return TEMP_SENSOR_INVALID;
    }

    /* Same formula as __LL_ADC_CALC_TEMPERATURE, but kept in tenths of a
       degree so the display gets one decimal instead of whole degrees. */
    ts_scaled = (int32_t)((ts_data * vdda_mv) / TEMPSENSOR_CAL_VREFANALOG);

    return ((ts_scaled - cal1) * (TEMPSENSOR_CAL2_TEMP - TEMPSENSOR_CAL1_TEMP) * 10)
               / (cal2 - cal1)
           + TEMPSENSOR_CAL1_TEMP * 10;
}
```

---

## 11. USB CDC over the Type-C cable

Crystal-less FS PHY off HSI48. Requires `HSI48State = RCC_HSI48_ON` in the clock config (§3).

```c
void HAL_PCD_MspInit(PCD_HandleTypeDef *pcdHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    if (pcdHandle->Instance == USB_OTG_FS) {
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
        PeriphClkInitStruct.UsbClockSelection    = RCC_USBCLKSOURCE_HSI48;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
            Error_Handler();
        }

        /* Without this the host sees nothing at all -- no error, just silence. */
        HAL_PWREx_EnableUSBVoltageDetector();

        __HAL_RCC_GPIOA_CLK_ENABLE();
        /* PA11 -> USB_OTG_FS_DM, PA12 -> USB_OTG_FS_DP */
        GPIO_InitStruct.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG1_FS;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        __HAL_RCC_USB_OTG_FS_CLK_ENABLE();
        HAL_NVIC_SetPriority(OTG_FS_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
    }
}
```

And in `stm32h7xx_it.c`:

```c
void OTG_FS_IRQHandler(void)
{
    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}
```

Transmit: `CDC_Transmit_FS((uint8_t *)line, (uint16_t)len);` — returns `USBD_BUSY` if the previous
packet is still in flight, so do not assume it succeeded. On the host: `pio device monitor` with
`monitor_dtr = 1`.

---

## 12. Frame pacing finer than 1 ms (DWT cycle counter)

`HAL_GetTick()` is 1 ms, which cannot express a 120 Hz period (8.333 ms). The DWT counter runs at the
**CPU clock, 240 MHz** (`SystemCoreClock` on the H7 is the CPU clock, not HCLK).

```c
static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->LAR          = 0xC5ACCE55U;   /* Cortex-M7 needs the unlock; M3/M4 do not */
    DWT->CYCCNT       = 0;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;
}
```

Fixed-rate loop, wrap-safe (CYCCNT wraps every ~17.9 s at 240 MHz):

```c
uint32_t frame_cycles = SystemCoreClock / 120U;
uint32_t next_frame   = DWT->CYCCNT;

for (;;) {
    while ((int32_t)(DWT->CYCCNT - next_frame) < 0) {
    }
    next_frame += frame_cycles;
    if ((int32_t)(DWT->CYCCNT - next_frame) > (int32_t)frame_cycles) {
        next_frame = DWT->CYCCNT + frame_cycles;   /* overran badly - resync */
    }

    uint32_t render_start = DWT->CYCCNT;
    render();
    uint32_t render_us = (DWT->CYCCNT - render_start) / (SystemCoreClock / 1000000U);
}
```

Slower periodic jobs run inside the same loop off `HAL_GetTick()` deadlines. Let the period
accumulate rather than resetting to `now`, or the job drifts slow:

```c
if ((now - last_report) >= 5000U) {
    last_report += 5000U;
    if ((now - last_report) >= 5000U) {
        last_report = now;      /* fell far behind - resync instead of catching up */
    }
    /* ...job... */
}
```

Same reason `HAL_Delay(200)` in a loop is wrong for a 200 ms period: it delays *between* jobs instead
of pacing them.

---

## 13. DAC1 (both channels), software tick from TIM6, no DMA

Signal generator: TIM6 fires a period-elapsed interrupt at the sample rate, and the ISR writes both
DAC channels directly. No DMA, no hardware trigger — good up to a few hundred kHz on an M7 at 240 MHz.
Pins: `DAC1_OUT1` = PA4 (P2 pin 21), `DAC1_OUT2` = PA5 (P2 pin 22) — both analog, no AF register
involved. This is not in the vendor SDK examples; the classic "TIMx_TRGO → DAC → DMA circular"
recipe from ST's own examples is unnecessary at these rates.

`board.h` additions:

```c
extern DAC_HandleTypeDef hdac1;
extern TIM_HandleTypeDef htim6;
```

GPIO — plain analog, `GPIO_MODE_ANALOG` needs no `Alternate` field:

```c
GPIO_InitStruct.Pin  = GPIO_PIN_4 | GPIO_PIN_5;   /* PA4 = DAC1_OUT1, PA5 = DAC1_OUT2 */
GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
GPIO_InitStruct.Pull = GPIO_NOPULL;
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
```

DAC init — `DAC_ChannelConfTypeDef` on H7 has four more mandatory fields than the "classic" HAL DAC
(F4-style): leaving any of them at `{0}`-default trips the parameter check inside
`HAL_DAC_ConfigChannel()`. Source: `stm32h7xx_hal_dac.h`.

```c
static void MX_DAC1_Init(void)
{
    DAC_ChannelConfTypeDef sConfig = {0};

    hdac1.Instance = DAC1;
    if (HAL_DAC_Init(&hdac1) != HAL_OK) {
        Error_Handler();
    }

    sConfig.DAC_SampleAndHold           = DAC_SAMPLEANDHOLD_DISABLE;
    sConfig.DAC_Trigger                 = DAC_TRIGGER_NONE;      /* software tick from TIM6 ISR */
    sConfig.DAC_OutputBuffer            = DAC_OUTPUTBUFFER_ENABLE;
    sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_EXTERNAL;
    sConfig.DAC_UserTrimming            = DAC_TRIMMING_FACTORY;
    if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_2) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_DAC_Start(&hdac1, DAC_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_DAC_Start(&hdac1, DAC_CHANNEL_2) != HAL_OK) {
        Error_Handler();
    }
}
```

TIM6 — base timer, period-elapsed interrupt at `sample_hz`. TIM6/TIM7 sit on the same APB1 domain as
TIM3 (§14.3 backlight note applies): at `APB1CLKDivider = RCC_APB1_DIV1` the kernel clock is
`PCLK1 = 120 MHz` directly, no ×2.

```c
#define TIM_CLK_HZ   120000000UL

static void MX_TIM6_Init(uint32_t sample_hz)
{
    htim6.Instance         = TIM6;
    htim6.Init.Prescaler   = 0;
    htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim6.Init.Period      = (TIM_CLK_HZ / sample_hz) - 1;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim6) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_TIM_Base_Start_IT(&htim6) != HAL_OK) {
        Error_Handler();
    }
}
```

MSP — the clock-enable macro is **`__HAL_RCC_DAC12_CLK_ENABLE()`**, not `__HAL_RCC_DAC1_CLK_ENABLE`:
DAC1 and DAC2 share one RCC bit (`RCC_APB1LENR_DAC12EN`).

```c
void HAL_DAC_MspInit(DAC_HandleTypeDef *hdac)
{
    if (hdac->Instance == DAC1) {
        __HAL_RCC_DAC12_CLK_ENABLE();     /* NOT __HAL_RCC_DAC1_CLK_ENABLE -- does not exist */
    }
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        __HAL_RCC_TIM6_CLK_ENABLE();
        HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 1, 0);   /* shared vector, see below */
        HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
    }
}
```

**Vector name trap:** TIM6 shares its interrupt vector with DAC underrun errors —
`TIM6_DAC_IRQn` / `TIM6_DAC_IRQHandler` — while TIM7 has an ordinary standalone vector,
`TIM7_IRQn` / `TIM7_IRQHandler`. Source: `stm32h750xx.h` (`IRQn_Type`) and
`startup_stm32h750xx.s`. Get the handler name wrong and the interrupt silently never fires
(no fault, no warning — the linker's weak default handler just sits there doing nothing).

ISR, in `stm32h7xx_it.c`:

```c
void TIM6_DAC_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim6);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, next_sample_ch1());
        HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, next_sample_ch2());
    }
}
```

With `DAC_Trigger = DAC_TRIGGER_NONE`, `HAL_DAC_SetValue()` moves the value from the DHR register to
the output automatically, one APB1 clock after the write — no hardware trigger and no DMA needed for
sample rates up to a few hundred kHz.

**Unrelated HAL gotcha worth knowing before wiring up TIM6/TIM7 or TIM3 alongside this:** a
`TIM_HandleTypeDef` has two independent weak MSP callbacks — `HAL_TIM_Base_Init()` calls
`HAL_TIM_Base_MspInit()`, `HAL_TIM_PWM_Init()` calls a *different* one, `HAL_TIM_PWM_MspInit()`. If a
PWM channel's clock-enable only lives in `HAL_TIM_Base_MspInit` and you call `HAL_TIM_PWM_Init()`
without first calling `HAL_TIM_Base_Init()`, the clock is never enabled — and this does not fail, the
timer just stays silent. Always call `HAL_TIM_Base_Init()` before `HAL_TIM_PWM_Init()` on the same
handle, as §6/§7 already does for TIM1.
