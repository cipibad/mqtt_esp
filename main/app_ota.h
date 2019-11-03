#ifndef APP_OTA_H
#define APP_OTA_H

#define OTA_FAILED -1
#define OTA_SUCCESFULL 0
#define OTA_ONGOING 1
#define OTA_READY 2

enum OtaMsgType {
  OTA_MQTT_CONNECTED=1,
  OTA_MQTT_UPGRADE
};

struct OtaUpgradeData {
  char url[64];
};

struct OtaMessage
{
  enum OtaMsgType msgType;
  struct OtaUpgradeData upgradeData;
};

void handle_ota_update_task(void *pvParameters);
void publish_ota_data(int status);



#endif /* APP_OTA_H */
