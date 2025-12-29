#ifndef APP_LOGGING_H
#define APP_LOGGING_H

#include <string.h>
#include "esp_log.h"
#include "app_sensors.h"

// Standardized module names for logging
#define LOG_MODULE_MQTT "mqtt"
#define LOG_MODULE_SENSOR "sensor"
#define LOG_MODULE_SYSTEM "system"
#define LOG_MODULE_BME280 "bme280"
#define LOG_MODULE_DHT22 "dht22"
#define LOG_MODULE_DS18X20 "ds18x20"
#define LOG_MODULE_BH1750 "bh1750"
#define LOG_MODULE_SOIL_MOISTURE "soil_moisture"
#define LOG_MODULE_ACTUATOR "actuator"
#define LOG_MODULE_RELAY "relay"
#define LOG_MODULE_THERMOSTAT "thermostat"
#define LOG_MODULE_VALVE "valve"
#define LOG_MODULE_WATERPUMP "waterpump"
#define LOG_MODULE_COAP "coap"
#define LOG_MODULE_OTA "ota"
#define LOG_MODULE_WIFI "wifi"

// Log level constants
#define LOG_LEVEL_ERROR "error"
#define LOG_LEVEL_WARNING "warning"
#define LOG_LEVEL_INFO "info"
#ifdef CONFIG_MQTT_LOG_LEVEL_DEBUG
#define LOG_LEVEL_DEBUG "debug"
#endif

// Define log level thresholds based on config
#define LOG_LEVEL_ENABLED_DEBUG  (strcmp(CONFIG_LOG_OUTPUT_MODE, "mqtt") != 0 && \
                                        strcmp(CONFIG_MQTT_LOG_LEVEL, "debug") == 0)
#define LOG_LEVEL_ENABLED_INFO   (strcmp(CONFIG_LOG_OUTPUT_MODE, "mqtt") != 0 && \
                                        (strcmp(CONFIG_MQTT_LOG_LEVEL, "info") == 0 || \
                                         strcmp(CONFIG_MQTT_LOG_LEVEL, "debug") == 0))
#define LOG_LEVEL_ENABLED_WARNING (strcmp(CONFIG_LOG_OUTPUT_MODE, "mqtt") != 0 && \
                                        (strcmp(CONFIG_MQTT_LOG_LEVEL, "warning") == 0 || \
                                         strcmp(CONFIG_MQTT_LOG_LEVEL, "info") == 0 || \
                                         strcmp(CONFIG_MQTT_LOG_LEVEL, "debug") == 0))
#define LOG_LEVEL_ENABLED_ERROR   (strcmp(CONFIG_LOG_OUTPUT_MODE, "mqtt") != 0 && \
                                        (strcmp(CONFIG_MQTT_LOG_LEVEL, "error") == 0 || \
                                         strcmp(CONFIG_MQTT_LOG_LEVEL, "warning") == 0 || \
                                         strcmp(CONFIG_MQTT_LOG_LEVEL, "info") == 0 || \
                                         strcmp(CONFIG_MQTT_LOG_LEVEL, "debug") == 0))

void publish_log_message(const char *level, const char *module, const char *message);
void publish_error_log(const char *module, const char *format, ...);
void publish_warning_log(const char *module, const char *format, ...);
void publish_info_log(const char *module, const char *format, ...);

#ifdef CONFIG_MQTT_LOG_LEVEL_DEBUG
void publish_debug_log(const char *module, const char *format, ...);
#endif

// Dual logging wrapper macros
#define LOGE(tag, module, ...) \
    do { \
        ESP_LOGE(tag, __VA_ARGS__); \
        if (LOG_LEVEL_ENABLED_ERROR) { \
            publish_error_log(module, __VA_ARGS__); \
        } \
    } while(0)

#define LOGW(tag, module, ...) \
    do { \
        ESP_LOGW(tag, __VA_ARGS__); \
        if (LOG_LEVEL_ENABLED_WARNING) { \
            publish_warning_log(module, __VA_ARGS__); \
        } \
    } while(0)

#define LOGI(tag, module, ...) \
    do { \
        ESP_LOGI(tag, __VA_ARGS__); \
        if (LOG_LEVEL_ENABLED_INFO) { \
            publish_info_log(module, __VA_ARGS__); \
        } \
    } while(0)

#define LOGD(tag, module, ...) \
    do { \
        ESP_LOGD(tag, __VA_ARGS__); \
        if (LOG_LEVEL_ENABLED_DEBUG) { \
            publish_debug_log(module, __VA_ARGS__); \
        } \
    } while(0)

#endif // APP_LOGGING_H
