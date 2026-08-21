// Minimal ESP32-C6-LCD-1.47 firmware: no display, no SPI bus.
//
// It proves the whole chain -- toolchain, build, flash over the Type-C port,
// console over USB-Serial-JTAG -- and it does it twice over, because the
// heartbeat log and the RGB LED fail independently. If the log appears but the
// LED stays dark, the toolchain and the flashing route are fine and the problem
// is the LED or its RMT timing.

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "rgb_led.h"

static const char *TAG = "minimal";

// A slow hue sweep at low brightness. Keep the RGB LED dim -- it sits under a
// clear acrylic layer right next to the panel and it is bright out of all
// proportion to the numbers you feed it.
static void hue_to_rgb(int hue, uint8_t *r, uint8_t *g, uint8_t *b)
{
    const int sector = hue / 85;            // 0..2
    const int frac = (hue % 85) * 255 / 85; // 0..255 within the sector
    switch (sector) {
        case 0:  *r = 255 - frac; *g = frac;       *b = 0;          break;
        case 1:  *r = 0;          *g = 255 - frac; *b = frac;       break;
        default: *r = frac;       *g = 0;          *b = 255 - frac; break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-C6-LCD-1.47 minimal firmware up");
    rgb_led_init();

    int hue = 0;
    uint32_t tick = 0;
    while (1) {
        uint8_t r, g, b;
        hue_to_rgb(hue, &r, &g, &b);
        // /12 keeps the peak around 20/255 -- visible in daylight, not painful.
        rgb_led_set(r / 12, g / 12, b / 12);

        hue = (hue + 3) % 255;
        if (++tick % 25 == 0) {
            ESP_LOGI(TAG, "alive, %lu ticks", (unsigned long)tick);
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}
