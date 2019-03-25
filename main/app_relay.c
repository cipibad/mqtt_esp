#include "esp_system.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "string.h"

#include "app_esp8266.h"
#include "app_relay.h"

extern EventGroupHandle_t mqtt_event_group;
extern const int INIT_FINISHED_BIT;

static int relayStatus[CONFIG_MQTT_RELAYS_NB];

const int relayToGpioMap[CONFIG_MQTT_RELAYS_NB] = {
  CONFIG_MQTT_RELAYS_NB0_GPIO,
#if CONFIG_MQTT_RELAYS_NB > 1
  CONFIG_MQTT_RELAYS_NB1_GPIO,
#if CONFIG_MQTT_RELAYS_NB > 2
  CONFIG_MQTT_RELAYS_NB2_GPIO,
#if CONFIG_MQTT_RELAYS_NB > 3
  CONFIG_MQTT_RELAYS_NB3_GPIO,
#endif //CONFIG_MQTT_RELAYS_NB > 3
#endif //CONFIG_MQTT_RELAYS_NB > 2
#endif //CONFIG_MQTT_RELAYS_NB > 1
};

static const char *TAG = "MQTTS_RELAY";

extern QueueHandle_t relayQueue;

void relays_init()
{
  gpio_config_t io_conf;
  io_conf.pin_bit_mask = 0;
  for(int i = 0; i < CONFIG_MQTT_RELAYS_NB; i++) {
    io_conf.pin_bit_mask |= (1ULL << relayToGpioMap[i]) ;
  }
  gpio_config(&io_conf);
  
  for(int i = 0; i < CONFIG_MQTT_RELAYS_NB; i++) {
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
  for(int id = 0; id < CONFIG_MQTT_RELAYS_NB; id++) {
    publish_relay_data(id, client);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}


void update_relay_state(int id, char value, MQTTClient* client)
{
<<<<<<< HEAD
=======
  ESP_LOGI(TAG, "update_relay_state: id: %d, value: %d", id, value);
  ESP_LOGI(TAG, "relayStatus[%d] = %d", id, relayStatus[id]);
>>>>>>> fix relay handling
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
  ESP_LOGI(TAG, "handle_relay_cmd_task started");
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
    if( xQueueReceive( relayQueue, &r , portMAX_DELAY) )
      {
        id=r.relayId;
        value=r.relayValue;
        update_relay_state(id, value, client);
      }
  }
}

