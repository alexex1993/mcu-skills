# WeAct STM32F411CEU6 "Black Pill" — Working Code Recipes

Every block below is lifted verbatim from `../template/`, which builds clean with `-Wall` and runs on
the board. The correspondence is mechanical: if a recipe and the template ever disagree, the template
is right.

Contents:

1. `platformio.ini`
2. `board.h` — the file to copy into the next project
3. Clock tree: 96 MHz SYSCLK + 48 MHz for USB
4. GPIO: LED and button, with the polarity this board actually has
5. RTC on the 32.768 kHz LSE crystal
6. Internal temperature sensor and VREFINT, factory-calibrated
7. USB CDC: the PCD ↔ USBD glue (`usbd_conf.c`)
8. USB CDC: descriptors (`usbd_desc.c`)
9. USB CDC: the application side, and a `printf` that cannot stall the loop
10. Minimal `main()` — a blink-only project, complete

---

## 1. `platformio.ini`

```ini
; PlatformIO Project Configuration File
;
; WeAct Studio MiniSTM32F4x1 "Black Pill" V3.x (STM32F411CEU6), built against
; the STM32Cube HAL. Flashed over the board's own USB-C through the ROM DFU
; bootloader, and talks back over USB CDC on the same cable.
;
; https://docs.platformio.org/page/projectconf.html

[env:blackpill_f411ce]
platform = ststm32
board = blackpill_f411ce
framework = stm32cube

; The board definition already reports the right sizes (512 KB flash,
; 128 KB SRAM) and the right linker script (STM32F411CEUX_FLASH.ld),
; so nothing needs correcting here.

build_flags =
    ; The F4 hal_conf template happens to default HSE_VALUE to 25 MHz, which is
    ; exactly this board's crystal - but that is luck, not a guarantee. State it,
    ; and every HAL_RCC_Get*Freq() stays right no matter which conf gets picked up.
    -DHSE_VALUE=25000000
    -DLSE_VALUE=32768
    -Wall

; Hold BOOT0, tap NRST, release -> the board enumerates as 0483:DF11 and
; `pio run -t upload` writes to 0x08000000 with `:leave`, so it restarts into
; the new firmware by itself. No probe, no second cable.
upload_protocol = dfu

; The stock board definition defaults debug_tool to blackmagic; almost everyone
; has an ST-Link clone on the 4-pin SWD header instead.
debug_tool = stlink

; The firmware enumerates as a USB CDC virtual COM port on that same USB-C
; cable. The baud rate is ignored - the link is virtual - but the monitor
; requires the setting. monitor_dtr raises DTR, which is what the firmware
; watches to know the port is open.
monitor_speed = 115200
monitor_dtr = 1
```

---

## 2. `board.h` — the file to copy into the next project

Pin names *with polarity baked into the macro* — `LED_ON()` rather than a bare pin number — because
this board's LED is active-low and its button is active-low, and both are easy to get backwards.

```c
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
```

---

## 3. Clock tree: 96 MHz SYSCLK + 48 MHz for USB

96 MHz is the ceiling on this board whenever USB is in play; `board-hardware.md` §4 has the arithmetic
for why 100 MHz and a 48 MHz PLLQ cannot coexist.

```c
/*
 * 96 MHz, not the chip's 100 MHz maximum. USB_OTG_FS needs exactly 48 MHz off
 * PLLQ, and PLLQ divides the same VCO as PLLP: VCO 192 -> /2 = 96 SYSCLK and
 * /4 = 48 USB. There is no integer pair that yields 100 MHz and 48 MHz at once,
 * so on any Black Pill firmware that uses USB, 96 MHz is the ceiling.
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* Voltage scale 1 is required above 84 MHz (datasheet Table 14) */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* No crystal fitted? Swap this block for RCC_OSCILLATORTYPE_HSI:
       osc.HSIState = RCC_HSI_ON; osc.PLL.PLLSource = RCC_PLLSOURCE_HSI; PLLM = 16;
       - same 96/48 MHz, but the HSI's +/-1% drift is outside USB's +/-0.25%. */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 25;   /* 25 MHz / 25 = 1 MHz  (spec: 0.95 .. 2.10)   */
    osc.PLL.PLLN       = 192;  /* 1 MHz * 192 = 192 MHz VCO (spec: 100 .. 432) */
    osc.PLL.PLLP       = RCC_PLLP_DIV2; /* 192 / 2 = 96 MHz SYSCLK            */
    osc.PLL.PLLQ       = 4;    /* 192 / 4 = 48 MHz - mandatory for USB        */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        Error_Handler();
    }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  = 96 MHz                 */
    clk.APB1CLKDivider = RCC_HCLK_DIV2;     /* PCLK1 = 48 MHz (50 MHz max)    */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;     /* PCLK2 = 96 MHz                 */
    /* 3 wait states: at VDD 2.7-3.6 V flash runs 30 MHz per state, and
       96 / (3+1) = 24 <= 30. Too few states hard-faults on the first fetch. */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_3) != HAL_OK)
    {
        Error_Handler();
    }
}
```

Startup order in `main()`:

```c
    HAL_Init();             /* reset peripherals, SysTick at 1 ms            */
    SystemClock_Config();   /* HSE 25 MHz -> PLL -> 96 MHz, PLLQ -> 48 MHz   */
    LED_GPIO_Init();
    KEY_GPIO_Init();
    RTC_ClockSource_Init(); /* LSE 32.768 kHz -> RTC clock                   */
    RTC_Init();
    TempSensor_Init();
    MX_USB_DEVICE_Init();
```

And the handler PlatformIO does not give you — without it the first `HAL_Delay()` hangs forever:

```c
/* framework-stm32cube ships no stm32f4xx_it.c. Without this, the weak default
   in the startup file spins forever and HAL_Delay()/HAL_GetTick() never advance
   - the classic "board boots, then hangs in the first HAL_Delay". */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
```

---

## 4. GPIO: LED and button

```c
static void LED_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    LED_OFF();  /* set the level before switching the pin to output */

    gpio.Pin   = LED_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    /* PC13 is fed through the backup-domain power switch: <= 2 MHz, <= 3 mA */
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &gpio);
}

static void KEY_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin  = KEY_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;    /* K1 shorts to GND; nothing else holds it high */
    HAL_GPIO_Init(KEY_GPIO_Port, &gpio);
}
```

---

## 5. RTC on the 32.768 kHz LSE crystal

Clock source first — the backup domain is write-protected out of reset, and the writes that fail
because of it fail silently:

```c
/*
 * The RTC lives in the backup domain, which is write-protected out of reset.
 * PWR's clock and HAL_PWR_EnableBkUpAccess() must both come first, or the
 * writes to RCC->BDCR are silently dropped and the RTC never ticks.
 */
static void RTC_ClockSource_Init(void)
{
    RCC_OscInitTypeDef       osc    = {0};
    RCC_PeriphCLKInitTypeDef periph = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    osc.OscillatorType = RCC_OSCILLATORTYPE_LSE;
    osc.LSEState       = RCC_LSE_ON;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        Error_Handler();    /* LSE can take seconds to start; timeout is 5 s */
    }

    periph.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    periph.RTCClockSelection    = RCC_RTCCLKSOURCE_LSE;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_RCC_RTC_ENABLE();
}
```

Then the calendar, seeded only when the backup domain actually lost power:

```c
static void RTC_Init(void)
{
    hrtc.Instance            = RTC;
    hrtc.Init.HourFormat     = RTC_HOURFORMAT_24;
    /* 32768 Hz = (127+1) * (255+1), i.e. exactly 1 Hz out of the two prescalers.
       SynchPrediv also sets the subsecond resolution: 1/256 s ~ 3.9 ms. */
    hrtc.Init.AsynchPrediv   = 127;
    hrtc.Init.SynchPrediv    = 255;
    hrtc.Init.OutPut         = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;
    if (HAL_RTC_Init(&hrtc) != HAL_OK)
    {
        Error_Handler();
    }

    /* Clock already set and the backup domain never lost power - leave it
       alone. The RTC keeps counting straight through a software reset. */
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == RTC_BKP_MAGIC)
    {
        return;
    }

    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};

    time.Hours          = SEED_HOUR;
    time.Minutes        = SEED_MINUTE;
    time.Seconds        = SEED_SECOND;
    time.TimeFormat     = RTC_HOURFORMAT12_AM;
    time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    time.StoreOperation = RTC_STOREOPERATION_RESET;
    if (HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK)
    {
        Error_Handler();
    }

    date.WeekDay = SEED_WEEKDAY;
    date.Month   = SEED_MONTH;
    date.Date    = SEED_DAY;
    date.Year    = SEED_YEAR - 2000U;
    if (HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, RTC_BKP_MAGIC);
}
```

Reading it back, with the ordering rule that catches everyone:

```c
/* Time with milliseconds. GetTime *before* GetDate is mandatory: on the F4 the
   calendar sits behind shadow registers that stay locked until both have been
   read (RM0383, "Reading the calendar"). Read them the other way round and the
   date freezes at whatever it was when the shadow was last unlocked. */
static void print_time(void)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};

    HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

    /* SubSeconds counts DOWN from SecondFraction to 0 within the current second */
    uint32_t ms = ((time.SecondFraction - time.SubSeconds) * 1000U) /
                  (time.SecondFraction + 1U);

    cdc_printf("%04u-%02u-%02u %02u:%02u:%02u.%03lu (" SEED_TZ_LABEL ")\r\n",
               2000U + date.Year, date.Month, date.Date,
               time.Hours, time.Minutes, time.Seconds, (unsigned long)ms);
}
```

---

## 6. Internal temperature sensor and VREFINT

Header:

```c
/**
 * Internal temperature sensor + VREFINT on ADC1, scaled with the chip's
 * factory calibration values instead of the datasheet's typical constants.
 */
#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include <stdint.h>

void     TempSensor_Init(void);

/** Die temperature in tenths of a degree Celsius (253 -> 25.3 C). */
int32_t  TempSensor_ReadDeciC(void);

/** Actual analog supply in millivolts, back-computed from VREFINT. */
uint32_t TempSensor_ReadVddaMv(void);

#endif /* TEMP_SENSOR_H */
```

Implementation. Three things here are places where the obvious code is wrong on this specific part:
the channel number, the sampling time, and using the factory calibration instead of the datasheet's
typical constants.

```c
/*
 * Internal temperature sensor and VREFINT on ADC1.
 *
 * Two things make this different from the F401 examples the Black Pill is
 * usually taught with:
 *
 * 1. On the STM32F411 the temperature sensor is ADC_IN18, not ADC_IN16, and it
 *    SHARES that channel with the VBAT divider. Only one measurement path may
 *    be enabled at a time - if VBATE is set, channel 18 returns VBAT/4 and the
 *    "temperature" reads roughly 3 x too high with no error anywhere. The HAL
 *    handles the mutual exclusion for you as long as you never call
 *    HAL_ADCEx_EnableVBAT(); ADC_CHANNEL_TEMPSENSOR carries a marker bit that
 *    tells HAL_ADC_ConfigChannel() to clear VBATE and set TSVREFE.
 *
 * 2. The datasheet's "V25 = 0.76 V, 2.5 mV/degC" is a typical, not a per-chip
 *    figure; section 3.30 says outright that the raw sensor is only good for
 *    detecting *changes*. The two-point factory calibration in system memory
 *    (TS_CAL1 at 30 C, TS_CAL2 at 110 C, both taken at VDDA = 3.3 V) turns it
 *    into an absolute reading, so use it. Same for VREFINT: VREFIN_CAL lets you
 *    recover the true VDDA, which on this board is whatever the little LDO
 *    happens to produce rather than an exact 3.3 V.
 *
 * Calibration addresses, datasheet Table 72 / Table 75:
 *   0x1FFF7A2A  VREFIN_CAL   raw VREFINT at 30 C, VDDA = 3.3 V
 *   0x1FFF7A2C  TS_CAL1      raw sensor at  30 C, VDDA = 3.3 V
 *   0x1FFF7A2E  TS_CAL2      raw sensor at 110 C, VDDA = 3.3 V
 */

#include "temp_sensor.h"
#include "board.h"

#define VREFIN_CAL  (*(volatile uint16_t *)0x1FFF7A2AU)
#define TS_CAL1     (*(volatile uint16_t *)0x1FFF7A2CU)
#define TS_CAL2     (*(volatile uint16_t *)0x1FFF7A2EU)

#define TS_CAL1_TEMP_C   30
#define TS_CAL2_TEMP_C   110
#define CAL_VDDA_MV      3300U

static ADC_HandleTypeDef hadc1;

void TempSensor_Init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance                   = ADC1;
    /* PCLK2 is 96 MHz; /4 gives a 24 MHz ADC clock, inside the 36 MHz maximum */
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }
}

/* One conversion of one channel. 480 cycles at 24 MHz is 20 us of sampling,
   comfortably past the 10 us the datasheet requires for both the temperature
   sensor (Table 71) and VREFINT (Table 74). The 3-cycle default is roughly
   0.1 us and reads nonsense from these two high-impedance sources. */
static uint32_t adc_read(uint32_t channel)
{
    ADC_ChannelConfTypeDef cfg = {0};

    cfg.Channel      = channel;
    cfg.Rank         = 1;
    cfg.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &cfg) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10U) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        return 0U;
    }
    uint32_t v = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return v;
}

uint32_t TempSensor_ReadVddaMv(void)
{
    uint32_t raw = adc_read(ADC_CHANNEL_VREFINT);

    if (raw == 0U || VREFIN_CAL == 0xFFFFU)
    {
        return CAL_VDDA_MV;     /* uncalibrated part - assume the nominal rail */
    }
    /* VREFINT is a fixed ~1.21 V, so a smaller raw count means a larger VDDA */
    return (CAL_VDDA_MV * (uint32_t)VREFIN_CAL) / raw;
}

int32_t TempSensor_ReadDeciC(void)
{
    uint32_t vdda = TempSensor_ReadVddaMv();
    uint32_t raw  = adc_read(ADC_CHANNEL_TEMPSENSOR);

    if (TS_CAL1 == 0xFFFFU || TS_CAL2 == TS_CAL1)
    {
        return 0;
    }

    /* The cal points were taken at 3.3 V. Rescale this reading to that rail
       before interpolating, otherwise a 3.25 V board reads about 2 C low. */
    int32_t scaled = (int32_t)((raw * vdda) / CAL_VDDA_MV);

    int32_t span_c   = (TS_CAL2_TEMP_C - TS_CAL1_TEMP_C) * 10;      /* deci-C */
    int32_t span_lsb = (int32_t)TS_CAL2 - (int32_t)TS_CAL1;

    return ((scaled - (int32_t)TS_CAL1) * span_c) / span_lsb + TS_CAL1_TEMP_C * 10;
}
```

---

## 7. USB CDC: the PCD ↔ USBD glue

The file CubeMX would generate. It carries the MSP (PA11/PA12 as AF10, the peripheral clock, the NVIC
line), every `HAL_PCD_*Callback`, the whole `USBD_LL_*` layer, the FIFO split, and `OTG_FS_IRQHandler`.

```c
/*
 * Glue between the ST USB Device Library and the HAL PCD driver for the
 * STM32F411's USB_OTG_FS core (PA11 = DM, PA12 = DP, AF10).
 *
 * This is the CubeMX-generated file, hand-written: framework-stm32cubef4 ships
 * the HAL but NOT the USB device middleware, so both this and lib/USBDevice/
 * have to come from the project.
 */

#include "usbd_conf.h"
#include "usbd_core.h"
#include "usbd_cdc.h"

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* Static backing store for the CDC class handle. ST's default USBD_malloc is
   plain malloc(), and the stock linker script reserves only 0x200 of heap - the
   allocation fails and the device never enumerates. Going static sidesteps the
   whole question; see usbd_conf.h. */
static uint32_t mem[(sizeof(USBD_CDC_HandleTypeDef) / 4) + 1];
static uint8_t mem_used = 0U;

/* ---------------------------------------------------------- MSP ---------- */

void HAL_PCD_MspInit(PCD_HandleTypeDef *pcdHandle)
{
    GPIO_InitTypeDef gpio = {0};

    if (pcdHandle->Instance != USB_OTG_FS)
    {
        return;
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA11 = USB_DM, PA12 = USB_DP, alternate function AF10 */
    gpio.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF10_OTG_FS;
    HAL_GPIO_Init(GPIOA, &gpio);

    __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

    HAL_NVIC_SetPriority(OTG_FS_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef *pcdHandle)
{
    if (pcdHandle->Instance != USB_OTG_FS)
    {
        return;
    }

    __HAL_RCC_USB_OTG_FS_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12);
    HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
}

/* The USB interrupt. Without this handler the stack never runs: enumeration
   simply never happens and the host reports an unknown device. */
void OTG_FS_IRQHandler(void)
{
    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}

/* --------------------------------------------- PCD -> USBD callbacks ----- */

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_SetupStage((USBD_HandleTypeDef *)hpcd->pData, (uint8_t *)hpcd->Setup);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_DataOutStage((USBD_HandleTypeDef *)hpcd->pData, epnum,
                         hpcd->OUT_ep[epnum].xfer_buff);
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_DataInStage((USBD_HandleTypeDef *)hpcd->pData, epnum,
                        hpcd->IN_ep[epnum].xfer_buff);
}

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_SOF((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_SetSpeed((USBD_HandleTypeDef *)hpcd->pData, USBD_SPEED_FULL);
    USBD_LL_Reset((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_Suspend((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_Resume((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_IsoOUTIncomplete((USBD_HandleTypeDef *)hpcd->pData, epnum);
}

void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_IsoINIncomplete((USBD_HandleTypeDef *)hpcd->pData, epnum);
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_DevConnected((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_DevDisconnected((USBD_HandleTypeDef *)hpcd->pData);
}

/* ------------------------------------------- USBD low-level layer -------- */

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
    hpcd_USB_OTG_FS.Instance                     = USB_OTG_FS;
    hpcd_USB_OTG_FS.Init.dev_endpoints           = 4;
    hpcd_USB_OTG_FS.Init.speed                   = PCD_SPEED_FULL;
    hpcd_USB_OTG_FS.Init.dma_enable              = DISABLE;
    hpcd_USB_OTG_FS.Init.phy_itface              = PCD_PHY_EMBEDDED;
    hpcd_USB_OTG_FS.Init.Sof_enable              = DISABLE;
    hpcd_USB_OTG_FS.Init.low_power_enable        = DISABLE;
    hpcd_USB_OTG_FS.Init.lpm_enable              = DISABLE;
    hpcd_USB_OTG_FS.Init.vbus_sensing_enable     = DISABLE;
    hpcd_USB_OTG_FS.Init.use_dedicated_ep1       = DISABLE;

    /* Cross-link the two handles: each one's pData points at the other */
    hpcd_USB_OTG_FS.pData = pdev;
    pdev->pData           = &hpcd_USB_OTG_FS;

    if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK)
    {
        return USBD_FAIL;
    }

    /* Carve up the core's 1.25 KB of FIFO (320 x 32-bit words, datasheet 3.27).
       These are word counts, not bytes, and they must sum to <= 320. Too small an
       RX FIFO shows up as data loss under load rather than as an error. */
    HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, 0x80);
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0, 0x40);
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 1, 0x80);

    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{
    return (HAL_PCD_DeInit(pdev->pData) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{
    return (HAL_PCD_Start(pdev->pData) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{
    return (HAL_PCD_Stop(pdev->pData) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                  uint8_t ep_type, uint16_t ep_mps)
{
    return (HAL_PCD_EP_Open(pdev->pData, ep_addr, ep_mps, ep_type) == HAL_OK)
               ? USBD_OK
               : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    return (HAL_PCD_EP_Close(pdev->pData, ep_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    return (HAL_PCD_EP_Flush(pdev->pData, ep_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    return (HAL_PCD_EP_SetStall(pdev->pData, ep_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    return (HAL_PCD_EP_ClrStall(pdev->pData, ep_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    PCD_HandleTypeDef *hpcd = (PCD_HandleTypeDef *)pdev->pData;

    if ((ep_addr & 0x80U) == 0x80U)
    {
        return hpcd->IN_ep[ep_addr & 0x7FU].is_stall;
    }

    return hpcd->OUT_ep[ep_addr & 0x7FU].is_stall;
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr)
{
    return (HAL_PCD_SetAddress(pdev->pData, dev_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                    uint8_t *pbuf, uint32_t size)
{
    return (HAL_PCD_EP_Transmit(pdev->pData, ep_addr, pbuf, size) == HAL_OK)
               ? USBD_OK
               : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                          uint8_t *pbuf, uint32_t size)
{
    return (HAL_PCD_EP_Receive(pdev->pData, ep_addr, pbuf, size) == HAL_OK)
               ? USBD_OK
               : USBD_FAIL;
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    return HAL_PCD_EP_GetRxCount(pdev->pData, ep_addr);
}

void USBD_LL_Delay(uint32_t Delay)
{
    HAL_Delay(Delay);
}

void *USBD_static_malloc(uint32_t size)
{
    (void)size;
    if (mem_used != 0U)
    {
        return NULL;
    }
    mem_used = 1U;
    return mem;
}

void USBD_static_free(void *p)
{
    (void)p;
    mem_used = 0U;
}
```

Its configuration header — the static allocator that avoids the 0x200-byte heap lives here:

```c
/* USB Device Library configuration - the file CubeMX would generate */

#ifndef __USBD_CONF_H
#define __USBD_CONF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"

/* CDC needs two interfaces: Control (ACM) + Data */
#define USBD_MAX_NUM_INTERFACES     2U
#define USBD_MAX_NUM_CONFIGURATION  1U
#define USBD_MAX_STR_DESC_SIZ       512U
#define USBD_DEBUG_LEVEL            0U
#define USBD_SELF_POWERED           1U
#define USBD_LPM_ENABLED            0U

/* Allocate the class handle statically. The stock linker script gives the heap
   0x200 bytes; ST's default malloc-based USBD_malloc quietly fails there and the
   device never enumerates. */
#define USBD_malloc                 (void *)USBD_static_malloc
#define USBD_free                   USBD_static_free
#define USBD_memset                 memset
#define USBD_memcpy                 memcpy
#define USBD_Delay                  HAL_Delay

void *USBD_static_malloc(uint32_t size);
void USBD_static_free(void *p);

#endif /* __USBD_CONF_H */
```

And the three-call assembly of the device:

```c
/* Assemble the USB device: core + CDC class + descriptors */

#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"

USBD_HandleTypeDef hUsbDeviceFS;

void MX_USB_DEVICE_Init(void)
{
    if (USBD_Init(&hUsbDeviceFS, &CDC_Desc, DEVICE_FS) != USBD_OK)
    {
        return;
    }
    if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK)
    {
        return;
    }
    if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS) != USBD_OK)
    {
        return;
    }
    USBD_Start(&hUsbDeviceFS);
}
```

---

## 8. USB CDC: descriptors

```c
/* USB device descriptors for a CDC ACM virtual COM port */

#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_conf.h"

#define USBD_VID           0x0483      /* STMicroelectronics */
#define USBD_PID_FS        0x5740      /* STM32 Virtual COM Port */
#define USBD_LANGID_STRING 0x409       /* English (US) */
#define USBD_MANUFACTURER  "STMicroelectronics"
#define USBD_PRODUCT_FS    "Black Pill F411 VCP"
#define USBD_CONFIG_FS     "CDC Config"
#define USBD_INTERFACE_FS  "CDC Interface"

/* The die's 96-bit unique ID, used to build a per-board serial number */
#define DEVICE_ID1            (UID_BASE)
#define DEVICE_ID2            (UID_BASE + 0x4U)
#define DEVICE_ID3            (UID_BASE + 0x8U)
#define USB_SIZ_STRING_SERIAL 26

static uint8_t *USBD_FS_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_FS_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_FS_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_FS_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_FS_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_FS_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);

USBD_DescriptorsTypeDef CDC_Desc = {
    USBD_FS_DeviceDescriptor,
    USBD_FS_LangIDStrDescriptor,
    USBD_FS_ManufacturerStrDescriptor,
    USBD_FS_ProductStrDescriptor,
    USBD_FS_SerialStrDescriptor,
    USBD_FS_ConfigStrDescriptor,
    USBD_FS_InterfaceStrDescriptor,
};

__ALIGN_BEGIN static uint8_t USBD_FS_DeviceDesc[USB_LEN_DEV_DESC] __ALIGN_END = {
    0x12,                       /* bLength */
    USB_DESC_TYPE_DEVICE,       /* bDescriptorType */
    0x00, 0x02,                 /* bcdUSB = 2.00 */
    0x02,                       /* bDeviceClass: CDC */
    0x02,                       /* bDeviceSubClass */
    0x00,                       /* bDeviceProtocol */
    USB_MAX_EP0_SIZE,           /* bMaxPacketSize0 */
    LOBYTE(USBD_VID), HIBYTE(USBD_VID),
    LOBYTE(USBD_PID_FS), HIBYTE(USBD_PID_FS),
    0x00, 0x02,                 /* bcdDevice = 2.00 */
    USBD_IDX_MFC_STR,           /* iManufacturer */
    USBD_IDX_PRODUCT_STR,       /* iProduct */
    USBD_IDX_SERIAL_STR,        /* iSerialNumber */
    USBD_MAX_NUM_CONFIGURATION  /* bNumConfigurations */
};

__ALIGN_BEGIN static uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END = {
    USB_LEN_LANGID_STR_DESC,
    USB_DESC_TYPE_STRING,
    LOBYTE(USBD_LANGID_STRING),
    HIBYTE(USBD_LANGID_STRING)
};

__ALIGN_BEGIN static uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;
__ALIGN_BEGIN static uint8_t USBD_StringSerial[26] __ALIGN_END = {
    26, USB_DESC_TYPE_STRING
};

/* A stable serial number keeps the host from re-enumerating the board as a
   new COM port every time it is reflashed. */
static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len)
{
    for (uint8_t idx = 0U; idx < len; idx++)
    {
        uint8_t nibble = (value >> 28) & 0x0FU;
        pbuf[2U * idx]      = (nibble < 0xAU) ? (nibble + '0') : (nibble + 'A' - 10U);
        pbuf[2U * idx + 1U] = 0U;
        value <<= 4;
    }
}

static void Get_SerialNum(void)
{
    uint32_t deviceserial0 = *(uint32_t *)DEVICE_ID1;
    uint32_t deviceserial1 = *(uint32_t *)DEVICE_ID2;
    uint32_t deviceserial2 = *(uint32_t *)DEVICE_ID3;

    deviceserial0 += deviceserial2;

    if (deviceserial0 != 0U)
    {
        IntToUnicode(deviceserial0, &USBD_StringSerial[2], 8U);
        IntToUnicode(deviceserial1, &USBD_StringSerial[18], 4U);
    }
}

static uint8_t *USBD_FS_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = sizeof(USBD_FS_DeviceDesc);
    return USBD_FS_DeviceDesc;
}

static uint8_t *USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = sizeof(USBD_LangIDDesc);
    return USBD_LangIDDesc;
}

static uint8_t *USBD_FS_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_PRODUCT_FS, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_FS_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_MANUFACTURER, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_FS_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = USB_SIZ_STRING_SERIAL;
    Get_SerialNum();
    return (uint8_t *)USBD_StringSerial;
}

static uint8_t *USBD_FS_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_CONFIG_FS, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_FS_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_INTERFACE_FS, USBD_StrDesc, length);
    return USBD_StrDesc;
}
```

---

## 9. USB CDC: the application side

```c
/* CDC application layer: the read/write side of the virtual COM port */

#include "usbd_cdc_if.h"
#include "usb_device.h"

static uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
static uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

/* The host sets baud/parity, but over USB CDC they are fiction: store what it
   asks for and hand it back, or some drivers refuse to open the port. */
static USBD_CDC_LineCodingTypeDef LineCoding = {
    115200, /* bitrate  */
    0x00,   /* stopbits: 1 */
    0x00,   /* parity:   none */
    0x08    /* databits: 8 */
};

volatile uint8_t CDC_PortOpen = 0U;

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t *pbuf, uint32_t *Len);

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS,
    NULL
};

static int8_t CDC_Init_FS(void)
{
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    return USBD_OK;
}

static int8_t CDC_DeInit_FS(void)
{
    return USBD_OK;
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    (void)length;

    switch (cmd)
    {
    case CDC_SET_LINE_CODING:
        LineCoding.bitrate    = (uint32_t)(pbuf[0] | (pbuf[1] << 8) |
                                           (pbuf[2] << 16) | (pbuf[3] << 24));
        LineCoding.format     = pbuf[4];
        LineCoding.paritytype = pbuf[5];
        LineCoding.datatype   = pbuf[6];
        break;

    case CDC_GET_LINE_CODING:
        pbuf[0] = (uint8_t)(LineCoding.bitrate);
        pbuf[1] = (uint8_t)(LineCoding.bitrate >> 8);
        pbuf[2] = (uint8_t)(LineCoding.bitrate >> 16);
        pbuf[3] = (uint8_t)(LineCoding.bitrate >> 24);
        pbuf[4] = LineCoding.format;
        pbuf[5] = LineCoding.paritytype;
        pbuf[6] = LineCoding.datatype;
        break;

    /* A no-data request: DTR/RTS live in the setup packet's wValue. DTR goes
       high when the host opens the port, which is how the firmware knows a
       monitor is attached and it is worth printing a banner. */
    case CDC_SET_CONTROL_LINE_STATE:
        CDC_PortOpen = (((USBD_SetupReqTypedef *)pbuf)->wValue & 0x0001U) ? 1U : 0U;
        break;

    default:
        break;
    }

    return USBD_OK;
}

/* Receive path: everything typed into the monitor lands here. Echo it back.
   Keep this fast - it runs in USB interrupt context. */
static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len)
{
    CDC_Transmit_FS(Buf, (uint16_t)*Len);

    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
}

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len)
{
    /* pClassDataCmsit[] is the field name in current ST USB Device Library
       releases; older ones (and most tutorials) call it pClassData. Copying an
       old snippet in here fails to compile - it is the same pointer. */
    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassDataCmsit[0];

    if (hcdc == NULL)
    {
        return USBD_FAIL;
    }
    if (hcdc->TxState != 0)
    {
        return USBD_BUSY;   /* the previous packet has not gone out yet */
    }

    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
    return USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}
```

A `printf` for it that gives up rather than stalling the main loop when nothing is reading:

```c
/* Print to USB CDC. If the host is not draining the pipe the endpoint stays
   busy; give up after 20 ms so the blink does not drift. */
static void cdc_printf(const char *fmt, ...)
{
    static char buf[256];
    va_list args;

    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len <= 0)
    {
        return;
    }
    if (len > (int)sizeof(buf))
    {
        len = (int)sizeof(buf);
    }

    uint32_t deadline = HAL_GetTick() + 20U;
    while (CDC_Transmit_FS((uint8_t *)buf, (uint16_t)len) == USBD_BUSY)
    {
        if (HAL_GetTick() > deadline)
        {
            return;     /* host is not reading - drop the line silently */
        }
    }
}
```

---

## 10. Minimal `main()` — a blink-only project, complete

Nothing else needed: this file plus `board.h` plus `platformio.ini` is a whole project, 3476 bytes of
flash and 44 bytes of RAM.

```c
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
```
