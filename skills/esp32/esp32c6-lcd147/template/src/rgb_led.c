#include "driver/rmt_tx.h"
#include "esp_err.h"

#include "board.h"
#include "rgb_led.h"

// 10 MHz => one tick is 0.1 us, which is fine enough for WS2812 bit timing and
// far below the RMT's 80 MHz source clock, so the divider is exact.
#define RGB_RMT_RESOLUTION_HZ 10000000

static rmt_channel_handle_t s_chan;
static rmt_encoder_handle_t s_encoder;

void rgb_led_init(void)
{
    rmt_tx_channel_config_t chan_cfg = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .gpio_num          = BOARD_PIN_RGB_LED,
        .mem_block_symbols = 64,
        .resolution_hz     = RGB_RMT_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&chan_cfg, &s_chan));

    // WS2812 bit cells, in 0.1 us ticks: a 0 is a short high then a long low,
    // a 1 is the other way round. Both cells are ~1.25 us total.
    rmt_bytes_encoder_config_t enc_cfg = {
        .bit0 = { .level0 = 1, .duration0 = 3,   // 0.3 us high
                  .level1 = 0, .duration1 = 9 }, // 0.9 us low
        .bit1 = { .level0 = 1, .duration0 = 9,   // 0.9 us high
                  .level1 = 0, .duration1 = 3 }, // 0.3 us low
        .flags.msb_first = 1,
    };
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&enc_cfg, &s_encoder));
    ESP_ERROR_CHECK(rmt_enable(s_chan));
}

void rgb_led_set(uint8_t r, uint8_t g, uint8_t b)
{
    const uint8_t grb[3] = { g, r, b };   // WS2812 wire order is GRB
    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,
    };
    ESP_ERROR_CHECK(rmt_transmit(s_chan, s_encoder, grb, sizeof(grb), &tx_cfg));
    // Waiting here also covers the >50 us reset gap before the next update.
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(s_chan, -1));
}
