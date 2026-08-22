idf_component_register(
    SRCS
        "main.c"
        "board_report.c"
        "analog.c"
        "wifi_scan.c"
    INCLUDE_DIRS "." "../include"
    REQUIRES
        driver
        esp_adc
        esp_wifi
        nvs_flash
        esp_timer
        spi_flash
        esp_partition
)
