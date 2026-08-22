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
