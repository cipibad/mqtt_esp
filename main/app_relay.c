#include "esp_system.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "string.h"

#include "app_esp8266.h"
#include "app_relay.h"

#define MAX_RELAYS 4

extern EventGroupHandle_t mqtt_event_group;
extern const int INIT_FINISHED_BIT;

const int relaysNb = CONFIG_RELAYS_NB;
static int relayStatus[MAX_RELAYS];

const int relayToGpioMap[4] = {4, 14, 12, 13};

static const char *TAG = "MQTTS_RELAY";

extern QueueHandle_t relayQueue;

void relays_init()
{
  gpio_config_t io_conf;
  io_conf.pin_bit_mask = 0;
  for(int i = 0; i < relaysNb; i++) {
    io_conf.pin_bit_mask |= (1ULL << relayToGpioMap[i]) ;
  }
  gpio_config(&io_conf);
  
  for(int i = 0; i < relaysNb; i++) {
    relayStatus[i] = OFF;
    gpio_set_direction(relayToGpioMap[i], GPIO_MODE_OUTPUT);
    gpio_set_level(relayToGpioMap[i], OFF);
  }
}


void publish_relay_data(int id, MQTTClient* client)
{
  if (xEventGroupGetBits(mqtt_event_group) & INIT_FINISHED_BIT)
    {
      const char * relays_topic = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/relay/";
      char data[32];
      memset(data,0,32);
      sprintf(data, "{\"state\":%d}", relayStatus[id] == ON);

      char topic[64];
      memset(topic,0,64);
      sprintf(topic, "%s%d", relays_topic, id);
      
      MQTTMessage message;
      message.qos = QOS1;
      message.retained = 1;
      message.payload = data;
      message.payloadlen = strlen(data);

      int rc = MQTTPublish(client, topic, &message);
      if (rc == 0) {
        ESP_LOGI(TAG, "sent publish relay %d successful, rc=%d", id, rc);
      } else {
        ESP_LOGI(TAG, "failed to publish %d relay, rc=%d", id, rc);
      }
    }
}
void publish_all_relays_data(MQTTClient* client) //FIXME
{
  for(int id = 0; id < relaysNb; id++) {
    publish_relay_data(id, client);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}


void update_relay_state(int id, char value, MQTTClient* client)
{
  if (value != (relayStatus[id] == ON)) {
    if (value == 1) {
      relayStatus[id] = ON;
      ESP_LOGI(TAG, "enabling GPIO %d", relayToGpioMap[id]);
    }
    if (value == 0) {
      relayStatus[id] = OFF;
      ESP_LOGI(TAG, "disabling GPIO %d", relayToGpioMap[id]);
    }
    gpio_set_level(relayToGpioMap[id], relayStatus[id]);
    publish_relay_data(id, client);
  }
}

void handle_relay_cmd_task(void* pvParameters)
{
#ifdef ESP8266
  MQTTClient* client = (MQTTClient*) pvParameters;
#endif //ESP8266

#ifdef ESP32
  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;
#endif //ESP32
  struct RelayMessage r;
  int id;
  int value;
  while(1) {
    ESP_LOGI(TAG, "relay loop start, waiting in queue");
    if( xQueueReceive( relayQueue, &r , portMAX_DELAY) )
      {
        ESP_LOGI(TAG, "relay loop, queue message received");
        id=r.relayId;
        value=r.relayValue;
        ESP_LOGI(TAG, "relay loop, updatind relay state");
        update_relay_state(id, value, client);
        ESP_LOGI(TAG, "relay loop end, publish done");

      }
  }
}

