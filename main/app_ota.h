#ifndef APP_OTA_H
#define APP_OTA_H

#define OTA_FAILED -1
#define OTA_SUCCESFULL 0
#define OTA_ONGOING 1
#define OTA_READY 2
#include "mqtt_client.h"

struct OtaMessage
{
  char url[64];
};

void handle_ota_update_task(void *pvParameters);
void publish_ota_data(esp_mqtt_client_handle_t client, int status);



#endif /* APP_OTA_H */
