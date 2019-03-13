#include <string.h>
#include "esp_log.h"

#include "app_sensors_publish.h"

static const char *TAG = "app_sensors_publish";

extern int16_t temperature;
extern int16_t humidity;

int publish_sensors_data(MQTTClient* pClient)
{

  const char * relays_topic = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/sensors";
  char data[256];
  memset(data,0,256);
  sprintf(data, "{\"h\":%d.%d, \"t\":%d.%d}",humidity/10, humidity%10 , temperature/10,temperature%10);

  MQTTMessage message;
  message.qos = QOS1;
  message.retained = 1;
  message.payload = data;
  message.payloadlen = strlen(data);
  int rc;
  if ((rc = MQTTPublish(pClient, relays_topic, &message)) != 0) {
    ESP_LOGI(TAG, "Return code from MQTT publish is %d", rc);
  } else {
    ESP_LOGI(TAG, "MQTT publish topic \"%s\"", relays_topic);
  }
  return rc;
}

