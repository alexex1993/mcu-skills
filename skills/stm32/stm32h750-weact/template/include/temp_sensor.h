/**
 * Internal temperature sensor, read through ADC3 (channel ADC3_INP18).
 */
#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include "board.h"

#define TEMP_SENSOR_INVALID  INT32_MIN

extern ADC_HandleTypeDef hadc3;

void MX_ADC3_Init(void);

/* Die temperature in tenths of a degree Celsius, or TEMP_SENSOR_INVALID
   if the conversion failed or the chip carries no factory calibration. */
int32_t TempSensor_ReadDeciC(void);

#endif /* TEMP_SENSOR_H */
