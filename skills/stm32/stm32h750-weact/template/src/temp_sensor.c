#include "temp_sensor.h"

ADC_HandleTypeDef hadc3;

/**
 * ADC3 clocking follows the vendor SDK (SDK/HAL/STM32H750/07-ADC_Test):
 * PLL3 off the 25 MHz HSE, 25/10 = 2.5 MHz -> VCO 150 MHz -> R = 75 MHz.
 * The extra /4 prescaler brings the ADC to 18.75 MHz, inside the 50 MHz
 * the datasheet allows; the vendor example leaves it undivided.
 */
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
