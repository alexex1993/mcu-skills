/* Module interfaces for the JC-ESP32P4-M3-DEV self-test. */
#pragma once

#include <stdbool.h>

/* board_report.c — one-shot board passport plus the live telemetry line. */
void board_report_init(void);          /* installs the die temperature sensor */
void board_report_print_all(void);     /* chip, clock, flash, memory, benches  */
float board_report_temperature(void);  /* -273.15 if the sensor is unavailable */

/* i2c_scan.c — probes the one shared bus on GPIO7/GPIO8. */
void i2c_scan_print(void);
