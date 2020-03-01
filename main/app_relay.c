#include "esp_log.h"

#include "driver/gpio.h"
#include "rom/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include <string.h>

#include "app_main.h"
#include "app_relay.h"
#include "app_nvs.h"

#include "app_mqtt.h"

#if CONFIG_MQTT_RELAYS_NB

int relayStatus[CONFIG_MQTT_RELAYS_NB];
int relayOnTimeout[CONFIG_MQTT_RELAYS_NB];
TimerHandle_t relayOnTimer[CONFIG_MQTT_RELAYS_NB];

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
  for(int i = 0; i < CONFIG_MQTT_RELAYS_NB; i++) {
    relayStatus[i] = RELAY_OFF;
    gpio_pad_select_gpio(relayToGpioMap[i]);
    gpio_set_direction(relayToGpioMap[i], GPIO_MODE_OUTPUT);
    gpio_set_level(relayToGpioMap[i], RELAY_OFF);
    relayOnTimer[i] = NULL;
    relayOnTimeout[i] = 0;
  }
}

void publish_relay_status(int id)
{
  const char * relays_topic = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/status/relay/";
  char data[16];
  memset(data,0,16);
  sprintf(data, "%s", relayStatus[id] == RELAY_ON ? "ON" : "OFF");

  char topic[MQTT_MAX_TOPIC_LEN];
  memset(topic,0,MQTT_MAX_TOPIC_LEN);
  sprintf(topic, "%s%d", relays_topic, id);

  mqtt_publish_data(topic, data, QOS_1, RETAIN);
}

void publish_relay_timeout(int id)
{
  const char * relays_topic = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/sleep/relay/";
  char data[16];
  memset(data,0,16);
  sprintf(data, "%d", relayOnTimeout[id]);

  char topic[MQTT_MAX_TOPIC_LEN];
  memset(topic,0,MQTT_MAX_TOPIC_LEN);
  sprintf(topic, "%s%d", relays_topic, id);

  mqtt_publish_data(topic, data, QOS_1, RETAIN);
}


void publish_all_relays_status()
{
  for(int id = 0; id < CONFIG_MQTT_RELAYS_NB; id++) {
    publish_relay_status(id);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void publish_all_relays_timeout()
{
  for(int id = 0; id < CONFIG_MQTT_RELAYS_NB; id++) {
    publish_relay_timeout(id);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void vTimerCallback( TimerHandle_t xTimer )
{
  int id = (int)pvTimerGetTimerID( xTimer );
  ESP_LOGI(TAG, "timer %d expired, sending stop msg", id);
  struct RelayMessage r = {RELAY_CMD_STATUS, id, RELAY_STATUS_OFF};
  if (xQueueSend( relayQueue
                  ,( void * )&r
                  ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to relayQueue");
  }
}

void update_timer(int id)
{
  ESP_LOGI(TAG, "update_timer for %d, timeout: %d", id, relayOnTimeout[id]);

  if ((relayStatus[id] == RELAY_OFF)) {
    if (relayOnTimer[id] != NULL) {
      if (xTimerIsTimerActive(relayOnTimer[id]) != pdFALSE){
        ESP_LOGI(TAG, "found started timer, stopping");
        xTimerStop( relayOnTimer[id], portMAX_DELAY );
      }
    }
  }

  if ((relayStatus[id] == RELAY_ON) && relayOnTimeout[id]) {
    if (relayOnTimer[id] == NULL) {
      ESP_LOGI(TAG, "creating timer");
      relayOnTimer[id] =
        xTimerCreate( "relayTimer",           /* Text name. */
                      pdMS_TO_TICKS(relayOnTimeout[id]*1000),  /* Period. */
                      pdFALSE,                /* Autoreload. */
                      (void *)id,                  /* No ID. */
                      vTimerCallback );  /* Callback function. */
    }
    if( relayOnTimer[id] != NULL ) {
      ESP_LOGI(TAG, "timer is created");
      if (xTimerIsTimerActive(relayOnTimer[id]) != pdFALSE){
        ESP_LOGI(TAG, "timer is active, stopping");
        xTimerStop( relayOnTimer[id], portMAX_DELAY );
      }

      TickType_t xTimerPeriod = xTimerGetPeriod(relayOnTimer[id]);
      if (xTimerPeriod != pdMS_TO_TICKS(relayOnTimeout[id]*1000)) {
        ESP_LOGI(TAG, "timer change period, starting also");
        xTimerChangePeriod(relayOnTimer[id], pdMS_TO_TICKS(relayOnTimeout[id]*1000), portMAX_DELAY ); //FIXME check return value
      }
      else {
        ESP_LOGI(TAG, "timer starting");
        xTimerStart( relayOnTimer[id], portMAX_DELAY );
      }
    }
  }
}

void update_relay_status(int id, char value)
{
  ESP_LOGI(TAG, "update_relay_status: id: %d, value: %d", id, value);
  ESP_LOGI(TAG, "relayStatus[%d] = %d", id, relayStatus[id] == RELAY_ON);
  if (value != (relayStatus[id] == RELAY_ON)) {
    if (value == RELAY_STATUS_ON) {
      relayStatus[id] = RELAY_ON;
      ESP_LOGI(TAG, "enabling GPIO %d", relayToGpioMap[id]);
    }
    if (value == RELAY_STATUS_OFF) {
      relayStatus[id] = RELAY_OFF;
      ESP_LOGI(TAG, "disabling GPIO %d", relayToGpioMap[id]);
    }
    gpio_set_level(relayToGpioMap[id], relayStatus[id]);
    update_timer(id);
  }
  publish_relay_status(id);
}

void update_relay_onTimeout(int id, char onTimeout)
{
  ESP_LOGI(TAG, "update_relay_onTimeout: id: %d, value: %d", id, onTimeout);
  ESP_LOGI(TAG, "relayStatus[%d] = %d", id, relayOnTimeout[id]);

  if (onTimeout != relayOnTimeout[id]) {
    relayOnTimeout[id] = onTimeout;
    update_timer(id);

    //FIXME extract NVS to separate function
    char onTimeoutTag[32]; //FIXME duplication with read function
    snprintf(onTimeoutTag, 32, "relayOnTimeout%d", id); //FIXME should check return value
    esp_err_t err = write_nvs_integer(onTimeoutTag, relayOnTimeout[id]);
    ESP_ERROR_CHECK( err );
  }
  publish_relay_timeout(id);
}

void handle_relay_task(void* pvParameters)
{
  ESP_LOGI(TAG, "handle_relay_cmd_task started");

  struct RelayMessage r;
  int id;
  int value;
  int onTimeout;
  while(1) {
    if( xQueueReceive( relayQueue, &r , portMAX_DELAY) )
      {
        if (r.msgType == RELAY_CMD_STATUS) {
          id=r.relayId;
          value=r.data;
          update_relay_status(id, value);
          continue;
        }
        if (r.msgType == RELAY_CMD_SLEEP) {
          id=r.relayId;
          onTimeout=r.data;
          update_relay_onTimeout(id, onTimeout);
          continue;
        }
      }
  }
}
#endif //CONFIG_MQTT_RELAYS_NB
