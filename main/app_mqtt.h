#ifndef APP_MQTT_H
#define APP_MQTT_H

#include "mqtt_client.h"

esp_mqtt_client_handle_t mqtt_init();
void mqtt_start(esp_mqtt_client_handle_t client);
void handle_mqtt_sub_pub(void* pvParameters);

#endif /* APP_MQTT_H */
