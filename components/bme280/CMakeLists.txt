idf_component_register(SRCS "driver/bme280.c" "driver/bme280_support.c"
                       INCLUDE_DIRS "driver"
                       REQUIRES esp_driver_gpio esp_driver_i2c esp_idf_lib_helpers)
