#ifndef APP_SENSORS_H
#define APP_SENSORS_H

#include "mqtt_client.h"

void sensors_read(void* pvParameters);
void publish_sensors_data(esp_mqtt_client_handle_t client);

#endif /* APP_SENSORS_H */
