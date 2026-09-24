/*
 * JC4880P443 — battery voltage.  BAT+ -> R52 68K -> GPIO53 (ADC2_CH4) ->
 * R57 100K -> GND, so V_bat = V_pin * 168 / 100.  With no cell fitted the
 * IP5306 still drives its BAT pin, so expect a plausible-looking ~4 V even on
 * USB alone — the reading does not prove a battery is present.
 */

#include "app.h"
#include "board_pins.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"

static const char *TAG = "battery";
static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;

esp_err_t battery_init(void)
{
    const adc_oneshot_unit_init_cfg_t unit = {.unit_id = ADC_UNIT_2};
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit, &s_adc), TAG, "unit");
    const adc_oneshot_chan_cfg_t ch = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc, ADC_CHANNEL_4, &ch), TAG, "chan");
    const adc_cali_curve_fitting_config_t cal = {
        .unit_id = ADC_UNIT_2, .chan = ADC_CHANNEL_4,
        .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cal, &s_cali) != ESP_OK) s_cali = NULL;
    return ESP_OK;
}

int battery_mv(void)
{
    int raw = 0, mv = 0;
    if (adc_oneshot_read(s_adc, ADC_CHANNEL_4, &raw) != ESP_OK) return -1;
    if (s_cali) adc_cali_raw_to_voltage(s_cali, raw, &mv);
    else mv = raw * 3300 / 4095;
    return mv * BAT_DIV_NUM / BAT_DIV_DEN;
}
