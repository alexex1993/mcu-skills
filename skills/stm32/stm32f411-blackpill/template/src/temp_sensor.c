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
