# src/CMakeLists.txt for the --minimal variant: no panel, no touch controller,
# so neither vendored component is required.
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "." "../include"
                       REQUIRES esp_driver_i2c esp_driver_ledc esp_driver_gpio)
