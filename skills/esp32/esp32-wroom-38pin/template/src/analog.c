/*
 * analog.c — ADC1 oneshot with eFuse calibration.
 *
 * ADC1 and not ADC2: ADC2 is shared with the Wi-Fi PHY, and every
 * adc_oneshot_read() on it returns ESP_ERR_TIMEOUT while the Wi-Fi driver is
 * started. That is a silicon-level arbitration, not a driver bug.
 */
#include "esp_check.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

#include "board.h"
#include "app.h"

static const char *TAG = "analog";

static adc_oneshot_unit_handle_t s_unit;
static adc_cali_handle_t         s_cali;      /* NULL when the chip has no eFuse Vref */

esp_err_t analog_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id  = BOARD_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_cfg, &s_unit), TAG, "new_unit");

    /* 12 dB gives the full ~150-2450 mV window. Above ~2450 mV the transfer
     * function flattens out, so a 3.3 V divider still needs headroom. */
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(
        adc_oneshot_config_channel(s_unit, BOARD_ADC_DEMO_CHANNEL, &chan_cfg),
        TAG, "config_channel");

    /* Line-fitting is the only scheme the original ESP32 supports. Chips
     * without the Vref eFuse burnt fall back to a 1100 mV default and are
     * roughly ±6 % out; calibration brings that to about ±60 mV at 12 dB. */
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id  = BOARD_ADC_UNIT,
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t err = adc_cali_create_scheme_line_fitting(&cali_cfg, &s_cali);
    if (err != ESP_OK) {
        s_cali = NULL;
        ESP_LOGW(TAG, "no ADC calibration available (%s) — raw counts only",
                 esp_err_to_name(err));
    }
    return ESP_OK;
}

esp_err_t analog_read_mv(int *raw_out, int *mv_out)
{
    int raw = 0;
    ESP_RETURN_ON_ERROR(adc_oneshot_read(s_unit, BOARD_ADC_DEMO_CHANNEL, &raw),
                        TAG, "read");
    if (raw_out) *raw_out = raw;

    if (mv_out) {
        int mv = -1;
        if (s_cali) {
            adc_cali_raw_to_voltage(s_cali, raw, &mv);
        }
        *mv_out = mv;
    }
    return ESP_OK;
}

void analog_deinit(void)
{
    if (s_cali) {
        adc_cali_delete_scheme_line_fitting(s_cali);
        s_cali = NULL;
    }
    if (s_unit) {
        adc_oneshot_del_unit(s_unit);
        s_unit = NULL;
    }
}
