#ifndef APP_MQTT_H
#define APP_MQTT_H

#include "MQTTClient.h"

MQTTClient* mqtt_init();
void mqtt_start(MQTTClient* client);
void handle_mqtt_sub_pub(void* pvParameters);

#endif /* APP_MQTT_H */
