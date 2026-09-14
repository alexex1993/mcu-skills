/*
 * Board passport and live telemetry for the Guition JC-ESP32P4-M3-DEV.
 *
 * Everything here answers a question you would otherwise ask the board by
 * guessing: which silicon revision is it, did the 32 MB PSRAM actually map,
 * how big is the flash, what is the core really running at, and how fast is
 * external memory compared with internal.
 */

#include <inttypes.h>
#include <string.h>

#include "app.h"
#include "driver/temperature_sensor.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_cpu.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_partition.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PSRAM_BENCH_BYTES (4 * 1024 * 1024)
#define SRAM_BENCH_BYTES (64 * 1024)
#define SRAM_BENCH_ROUNDS 64
#define CPU_BENCH_ITERATIONS (10 * 1000 * 1000)

static const char *TAG = "p4";

static temperature_sensor_handle_t s_tsens;

/* One volatile sink for the whole benchmark: the loop is not optimised away,
 * but it does not pay for a volatile access per iteration either — otherwise
 * we would be measuring the loop rather than the memory. */
static volatile uint32_t s_sink;

/* ------------------------------------------------------------------ helpers */

static const char *reset_reason_str(void)
{
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "power-on";
    case ESP_RST_EXT:       return "external reset";
    case ESP_RST_SW:        return "software reset";
    case ESP_RST_PANIC:     return "panic / exception";
    case ESP_RST_INT_WDT:   return "interrupt watchdog";
    case ESP_RST_TASK_WDT:  return "task watchdog";
    case ESP_RST_WDT:       return "other watchdog";
    case ESP_RST_DEEPSLEEP: return "woke from deep sleep";
    case ESP_RST_BROWNOUT:  return "brownout";
    case ESP_RST_SDIO:      return "SDIO reset";
    case ESP_RST_USB:       return "USB reset (reflash / monitor)";
    case ESP_RST_JTAG:      return "JTAG reset";
    default:                return "unknown";
    }
}

/* The core frequency as measured, not as configured: count CPU cycles over a
 * known interval. */
static double measure_cpu_mhz(void)
{
    const int64_t t0 = esp_timer_get_time();
    const uint32_t c0 = esp_cpu_get_cycle_count();
    vTaskDelay(pdMS_TO_TICKS(100));
    const uint32_t c1 = esp_cpu_get_cycle_count();
    const int64_t t1 = esp_timer_get_time();

    return (double)(c1 - c0) / (double)(t1 - t0);
}

static double mb_per_sec(size_t bytes, int64_t usec)
{
    if (usec <= 0) {
        return 0.0;
    }
    return ((double)bytes / (1024.0 * 1024.0)) / ((double)usec / 1e6);
}

static void read_sweep(const uint32_t *buf, size_t bytes)
{
    uint32_t sum = 0;
    const size_t words = bytes / sizeof(uint32_t);

    for (size_t i = 0; i < words; i++) {
        sum += buf[i];
    }
    s_sink = sum;
}

/* ---------------------------------------------------------------- sections */

static void print_chip(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_BASE);

    /* revision is in hundredths: 103 is "v1.3".  Anything below 300 is the
     * pre-rev.3 silicon that CONFIG_ESP32P4_SELECTS_REV_LESS_V3 builds for. */
    printf("CHIP     ESP32-P4 rev v%d.%d, %d cores\n",
           chip.revision / 100, chip.revision % 100, chip.cores);
    printf("         base MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    /* CHIP_FEATURE_EMB_PSRAM is not set on the ESP32-P4 even though the PSRAM
     * is in the package, so ask the driver instead of the feature bitmask. */
    printf("         PSRAM: %u MB detected, reset: %s\n",
           (unsigned)(esp_psram_get_size() >> 20), reset_reason_str());
}

static void print_clock(void)
{
    printf("CLOCK    configured %d MHz, measured %.1f MHz, xtal %d MHz\n",
           CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, measure_cpu_mhz(), CONFIG_XTAL_FREQ);
}

static void print_flash_and_app(void)
{
    uint32_t size = 0, jedec = 0;
    esp_flash_get_size(NULL, &size);
    esp_flash_read_id(NULL, &jedec);

    printf("FLASH    %" PRIu32 " MB, JEDEC id 0x%06" PRIx32 " (manufacturer 0x%02" PRIx32 ")\n",
           size >> 20, jedec, (jedec >> 16) & 0xff);

    /* The build date below comes from the ESP-IDF application descriptor and is
     * NOT refreshed on every rebuild — do not use it to tell whether the
     * firmware on the board is the one you just built. */
    const esp_app_desc_t *app = esp_app_get_description();
    if (app) {
        printf("APP      %s, ESP-IDF %s, built %s %s\n",
               app->project_name, app->idf_ver, app->date, app->time);
    }

    const esp_partition_t *part =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (part) {
        printf("         partition '%s' @ 0x%06" PRIx32 ", size %" PRIu32 " KB\n",
               part->label, (uint32_t)part->address, (uint32_t)part->size >> 10);
    }
}

static void print_memory(void)
{
    printf("MEMORY   internal: %u KB free of %u KB, largest block %u KB\n",
           (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >> 10),
           (unsigned)(heap_caps_get_total_size(MALLOC_CAP_INTERNAL) >> 10),
           (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) >> 10));
    printf("         PSRAM: %u KB total, %u KB free, largest block %u KB\n",
           (unsigned)(esp_psram_get_size() >> 10),
           (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >> 10),
           (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) >> 10));
}

static void bench_ram(const char *label, uint32_t caps, size_t bytes, int rounds)
{
    uint32_t *buf = heap_caps_malloc(bytes, caps);
    if (!buf) {
        printf("BENCH    %s: could not allocate %u KB\n", label, (unsigned)(bytes >> 10));
        return;
    }

    int64_t t0 = esp_timer_get_time();
    for (int i = 0; i < rounds; i++) {
        memset(buf, i, bytes);
    }
    const int64_t write_us = esp_timer_get_time() - t0;

    t0 = esp_timer_get_time();
    for (int i = 0; i < rounds; i++) {
        read_sweep(buf, bytes);
    }
    const int64_t read_us = esp_timer_get_time() - t0;

    /* Write is memset (the compiler uses wide stores); read is a scalar 4-byte
     * sweep with an accumulator dependency.  This is not peak bus bandwidth, it
     * is the honest speed of plain code — the useful comparison is PSRAM against
     * internal SRAM, not either number against a datasheet. */
    printf("BENCH    %-6s write %6.1f MB/s (memset), read %6.1f MB/s (32-bit scalar), buffer %u KB x%d\n",
           label, mb_per_sec(bytes * rounds, write_us), mb_per_sec(bytes * rounds, read_us),
           (unsigned)(bytes >> 10), rounds);

    heap_caps_free(buf);
}

static void bench_cpu(void)
{
    uint32_t acc = 1;

    const int64_t t0 = esp_timer_get_time();
    for (uint32_t i = 0; i < CPU_BENCH_ITERATIONS; i++) {
        acc = acc * 1664525u + 1013904223u;
    }
    s_sink = acc; /* the result has to escape or the loop is deleted */
    const int64_t us = esp_timer_get_time() - t0;

    printf("BENCH    CPU    %.1f Mops/s (%" PRId64 " ms for %d iterations, acc=0x%08" PRIx32 ")\n",
           (double)CPU_BENCH_ITERATIONS / (double)us, us / 1000, CPU_BENCH_ITERATIONS, acc);
}

/* -------------------------------------------------------------------- API */

void board_report_init(void)
{
    /* The range MUST fit inside ONE row of the ESP32-P4 range table in
     * soc/esp32p4/temperature_sensor_periph.c:
     *   {-2, 5, 50, 125, 3} {-1, 7, 20, 100, 2} {0, 15, -10, 80, 1}
     *   {1, 11, -30, 50, 2} {2, 10, -40, 20, 3}
     * -10..80 is the most accurate row (+-1 C).  A "reasonable" range such as
     * -10..100 is contained in none of them, so temperature_sensor_install()
     * returns ESP_ERR_INVALID_ARG and logs "Out of testing range" — which
     * reads like a broken peripheral rather than a bad argument. */
    temperature_sensor_config_t tsens_cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    esp_err_t err = temperature_sensor_install(&tsens_cfg, &s_tsens);
    if (err == ESP_OK) {
        err = temperature_sensor_enable(s_tsens);
    }
    if (err != ESP_OK) {
        s_tsens = NULL;
        ESP_LOGW(TAG, "die temperature sensor unavailable: %s", esp_err_to_name(err));
    }
}

float board_report_temperature(void)
{
    float celsius = 0.0f;

    if (s_tsens && temperature_sensor_get_celsius(s_tsens, &celsius) == ESP_OK) {
        return celsius;
    }
    return -273.15f;
}

void board_report_print_all(void)
{
    print_chip();
    print_clock();
    print_flash_and_app();
    print_memory();
    bench_ram("PSRAM", MALLOC_CAP_SPIRAM, PSRAM_BENCH_BYTES, 1);
    bench_ram("SRAM", MALLOC_CAP_INTERNAL, SRAM_BENCH_BYTES, SRAM_BENCH_ROUNDS);
    bench_cpu();
    printf("TEMP     %.1f C\n", board_report_temperature());
}
