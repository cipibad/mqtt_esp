#ifndef APP_OPS_H
#define APP_OPS_H

#include "mqtt_client.h"

void ops_pub_task(void* pvParameters);
void publish_ops_data(esp_mqtt_client_handle_t client);

#endif /* APP_OPS_H */
