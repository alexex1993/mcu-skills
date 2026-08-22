/*
 * wifi_scan.c — one blocking scan, as a smoke test for the radio.
 *
 * A devkit that blinks but cannot scan is almost always a power problem: the
 * TX burst pulls ~300 mA for a few hundred microseconds and a weak USB port
 * or a charge-only cable browns the rail out. The scan either prints APs or
 * the board resets — both answers are useful.
 */
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "app.h"

static const char *TAG = "wifi";
#define SCAN_MAX_AP 16

static const char *authmode_str(wifi_auth_mode_t m)
{
    switch (m) {
    case WIFI_AUTH_OPEN:            return "open";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/2";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/3";
    default:                        return "?";
    }
}

esp_err_t wifi_scan_once(void)
{
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif_init");

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_RETURN_ON_ERROR(err, TAG, "event_loop");
    }
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi_init");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set_mode");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start");

    ESP_LOGI(TAG, "scanning 2.4 GHz ...");
    ESP_RETURN_ON_ERROR(esp_wifi_scan_start(NULL, true), TAG, "scan");

    uint16_t count = SCAN_MAX_AP;
    static wifi_ap_record_t records[SCAN_MAX_AP];
    memset(records, 0, sizeof(records));
    ESP_RETURN_ON_ERROR(esp_wifi_scan_get_ap_records(&count, records), TAG, "get_records");

    uint16_t total = 0;
    esp_wifi_scan_get_ap_num(&total);
    ESP_LOGI(TAG, "%u AP(s) visible, showing %u", total, count);
    for (int i = 0; i < count; i++) {
        ESP_LOGI(TAG, "  ch%-3d %4d dBm  %-7s  %s",
                 records[i].primary, records[i].rssi,
                 authmode_str(records[i].authmode), (char *)records[i].ssid);
    }

    /* Stopping the driver hands ADC2 and the RF-shared pins back. */
    esp_wifi_stop();
    esp_wifi_deinit();
    return ESP_OK;
}
