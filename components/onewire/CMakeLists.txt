idf_component_register(SRCS "onewire.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_driver_gpio esp_idf_lib_helpers)
