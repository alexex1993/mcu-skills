#pragma once

#include <Arduino.h>

// board_report.cpp — chip, flash, PSRAM, reset reason, strapping levels.
void reportBoard();

// display.cpp — ST7305 panel over hardware SPI2, U8g2 full-frame buffer.
void displayBegin();
void displayDrawDashboard(float tempC, float rh, float battV, uint32_t wakes);
void displayEnterLowPower();  // ST7305 LPM, 0x39
void displayLeaveLowPower();  // ST7305 HPM, 0x38

// sensors.cpp — the shared I2C bus and everything on it, plus the battery ADC.
void sensorsBegin();
void i2cScan();                          // prints every address that answers
bool shtc3Read(float *tempC, float *rh); // SHTC3 @ 0x70
bool pcf85063ReadTime(struct tm *out);   // PCF85063A @ 0x51
float batteryVolts();                    // GPIO4 through the 1/3 divider
