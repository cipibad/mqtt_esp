#include "esp_system.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "cJSON.h"

#include "app_relay.h"


#define MAX_RELAYS 4
#define ON 0
#define OFF 1

extern EventGroupHandle_t mqtt_publish_event_group;
extern const int MQTT_PUBLISH_RELAYS_BIT;

const int relayBase = CONFIG_RELAYS_BASE;
const int relaysNb = CONFIG_RELAYS_NB;
static int relayStatus[MAX_RELAYS];

static const char *TAG = "MQTTS_RELAY";

#include <string.h>

void relays_init()
{

    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;

    //bit mask of the pins that you want to set,e.g.GPIO15/16
    io_conf.pin_bit_mask = 0;
    for(int i = 0; i < relaysNb; i++) {
      io_conf.pin_bit_mask |= (1ULL << (relayBase + i)) ;
    }
    //configure GPIO with the given settings
    gpio_config(&io_conf);


  
  for(int i = 0; i < relaysNb; i++) {
    gpio_set_level(relayBase + i, OFF);
    relayStatus[i] = OFF;
  }
}

void publish_relay_data(MQTTClient* pClient)
{

  const char * relays_topic = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/relays";
  ESP_LOGI(TAG, "starting publish_relay_data");
  char data[256];
  char relayData[32];
  memset(data,0,256);
  strcat(data, "{");
  for(int i = 0; i < relaysNb; i++) {
    memset(relayData,0,32);
    sprintf(relayData, "\"r%dS\":%d", i, relayStatus[i] == ON);
    if (i != (relaysNb-1)) {
      strcat(relayData, ",");
    }
    strcat(data, relayData);
  }
  strcat(data, "}");
  ESP_LOGI(TAG, "mqtt_data: %s", data);
  ESP_LOGI(TAG, "mqtt_strlen: %d", strlen(data));
  MQTTMessage message;
  message.qos = QOS2;
  message.retained = 1;
  message.payload = data;
  message.payloadlen = strlen(data);
  int rc;
  ESP_LOGI(TAG, "before MQTTPublish");
  if ((rc = MQTTPublish(pClient, relays_topic, &message)) != 0) {
    ESP_LOGI(TAG, "Return code from MQTT publish is %d\n", rc);
  } else {
    ESP_LOGI(TAG, "MQTT publish topic \"%s\"\n", relays_topic);
  }
}


int handle_relay_cmd(MQTTMessage *data)
{
  if (data->payloadlen >= 32 )
    {
      ESP_LOGI(TAG, "unextected relay cmd payload");
      return -1;
    }
  char tmpBuf[32];
  memcpy(tmpBuf, data->payload, data->payloadlen);
  tmpBuf[data->payloadlen] = 0;
  cJSON * root   = cJSON_Parse(tmpBuf);
  int id = cJSON_GetObjectItem(root,"id")->valueint;
  int value = cJSON_GetObjectItem(root,"value")->valueint;
  printf("id: %d\r\n", id);
  printf("value: %d\r\n", value);

  if (value == relayStatus[id]) {
    //reversed logic
    if (value == OFF) {
      relayStatus[id] = ON;
    }
    if (value == ON) {
      relayStatus[id] = OFF;
    }
    gpio_set_level(relayBase + id, relayStatus[id]);
    xEventGroupSetBits(mqtt_publish_event_group, MQTT_PUBLISH_RELAYS_BIT);
  }
  return 0;

}

