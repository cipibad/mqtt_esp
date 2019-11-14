#include <stdbool.h>
#include <stdio.h>

#define CONFIG_MQTT_THERMOSTAT 1
#define CONFIG_MQTT_DEVICE_TYPE "device_type"
#define CONFIG_MQTT_CLIENT_ID "client_id"
#define CONFIG_MQTT_THERMOSTAT_RELAY_ID 0

typedef int esp_err_t;
typedef void * QueueHandle_t;
typedef void * TimerHandle_t;

void ESP_ERROR_CHECK(int a);
