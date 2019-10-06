#ifndef APP_MQTT_H
#define APP_MQTT_H

/* some useful values for relay Json exchanges */
#define MAX_MQTT_DATA_LEN_RELAY 32
#define MAX_MQTT_DATA_THERMOSTAT 64
#define JSON_BAD_RELAY_ID 255
#define JSON_BAD_RELAY_VALUE 255

#include "mqtt_client.h"

esp_mqtt_client_handle_t mqtt_init();
void mqtt_start(esp_mqtt_client_handle_t client);
void handle_mqtt_sub_pub(void* pvParameters);

#endif /* APP_MQTT_H */
