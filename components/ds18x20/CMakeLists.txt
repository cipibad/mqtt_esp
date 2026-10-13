idf_component_register(SRCS "ds18x20.c"
                       INCLUDE_DIRS "."
                       REQUIRES onewire esp_driver_gpio esp_idf_lib_helpers)
