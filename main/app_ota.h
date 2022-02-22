#ifndef APP_OTA_H
#define APP_OTA_H

#define OTA_FAILED -1
#define OTA_SUCCESFULL 0
#define OTA_ONGOING 1
#define OTA_READY 2

#define OTA_QUEUE_TIMEOUT (60 * 1000 / portTICK_PERIOD_MS)

struct OtaMessage
{
  char url[64];
};

void handle_ota_update_task(void *pvParameters);
void publish_ota_data(int status);



#endif /* APP_OTA_H */
