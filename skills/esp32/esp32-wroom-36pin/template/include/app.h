#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/* board_report.c — everything the board can tell you about itself. */
void board_report_chip(void);       /* SoC model, cores, revision, features */
void board_report_reset(void);      /* reset reason + strapping latch */
void board_report_memory(void);     /* heap, flash, partition table */
void board_report_all(void);

/* analog.c — ADC1 oneshot with eFuse calibration where the chip has it. */
esp_err_t analog_init(void);
esp_err_t analog_read_mv(int *raw_out, int *mv_out);
void      analog_deinit(void);

/* wifi_scan.c — one blocking passive scan, printed as a table. */
esp_err_t wifi_scan_once(void);
