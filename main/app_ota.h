#ifndef APP_OTA_H
#define APP_OTA_H

#define OTA_FAILED -1
#define OTA_SUCCESFULL 0
#define OTA_ONGOING 1
#define OTA_READY 2

struct OtaMessage
{
  char url[64];
};

#include "MQTTClient.h"
void handle_ota_update_task(void *pvParameters);
void publish_ota_data(MQTTClient* pclient, int status);



#endif /* APP_OTA_H */
