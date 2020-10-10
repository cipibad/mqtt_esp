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
int relaySleepTimeout[CONFIG_MQTT_RELAYS_NB];
TimerHandle_t relaySleepTimer[CONFIG_MQTT_RELAYS_NB];

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

const char * relaySleepTag[CONFIG_MQTT_RELAYS_NB] = {
  "relaySleep0",
#if CONFIG_MQTT_RELAYS_NB > 1
  "relaySleep1",
#if CONFIG_MQTT_RELAYS_NB > 2
  "relaySleep2",
#if CONFIG_MQTT_RELAYS_NB > 3
  "relaySleep3",
#endif //CONFIG_MQTT_RELAYS_NB > 3
#endif //CONFIG_MQTT_RELAYS_NB > 2
#endif //CONFIG_MQTT_RELAYS_NB > 1
};


const char * relayTimerName[CONFIG_MQTT_RELAYS_NB] = {
  "relayTimer0",
#if CONFIG_MQTT_RELAYS_NB > 1
  "relayTimer1",
#if CONFIG_MQTT_RELAYS_NB > 2
  "relayTimer2",
#if CONFIG_MQTT_RELAYS_NB > 3
  "relayTimer3",
#endif //CONFIG_MQTT_RELAYS_NB > 3
#endif //CONFIG_MQTT_RELAYS_NB > 2
#endif //CONFIG_MQTT_RELAYS_NB > 1
};


static const char *TAG = "MQTTS_RELAY";

extern QueueHandle_t relayQueue;

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

void relays_init()
{
  esp_err_t err;
  for(int i = 0; i < CONFIG_MQTT_RELAYS_NB; i++) {
    relayStatus[i] = RELAY_OFF;
    gpio_pad_select_gpio(relayToGpioMap[i]);
    gpio_set_direction(relayToGpioMap[i], GPIO_MODE_OUTPUT);
    gpio_set_level(relayToGpioMap[i], relayStatus[i]);
    
    err=read_nvs_integer(relaySleepTag[i], &relaySleepTimeout[i]);
    ESP_ERROR_CHECK( err );

    relaySleepTimer[i] = NULL;
  }
}

void publish_relay_status(int id)
{
  const char * relays_topic = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/status/relay";
  char data[16];
  memset(data,0,16);
  sprintf(data, "%s", relayStatus[id] == RELAY_ON ? "ON" : "OFF");

  char topic[MQTT_MAX_TOPIC_LEN];
  memset(topic,0,MQTT_MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", relays_topic, id);

  mqtt_publish_data(topic, data, QOS_1, RETAIN);
}

void publish_relay_timeout(int id)
{
  const char * relays_topic = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/sleep/relay";
  char data[16];
  memset(data,0,16);
  sprintf(data, "%d", relaySleepTimeout[id]);

  char topic[MQTT_MAX_TOPIC_LEN];
  memset(topic,0,MQTT_MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", relays_topic, id);

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

void update_timer(int id)
{
  ESP_LOGI(TAG, "update_timer for %d, timeout: %d", id, relaySleepTimeout[id]);
  if (relaySleepTimer[id] != NULL) {
    if (xTimerIsTimerActive(relaySleepTimer[id]) != pdFALSE){
      ESP_LOGI(TAG, "Found started timer, stopping");
      xTimerStop( relaySleepTimer[id], portMAX_DELAY );
    }
  }
  if ((relayStatus[id] == RELAY_ON) && relaySleepTimeout[id] != 0) {
    if (relaySleepTimer[id] == NULL) {
      ESP_LOGI(TAG, "No Timer found for %d, creating one", id);
      relaySleepTimer[id] =
        xTimerCreate( relayTimerName[id],           /* Text name. */
                      pdMS_TO_TICKS(relaySleepTimeout[id]*1000),  /* Period. */
                      pdFALSE,                /* Autoreload. */
                      (void *)id,                  /* ID. */
                      vTimerCallback );  /* Callback function. */
    }
    if (relaySleepTimer[id] == NULL) {
      ESP_LOGE(TAG, "No Timer found for %d, cannot handle timeout", id);
      return;
    }
    if (xTimerChangePeriod(relaySleepTimer[id], pdMS_TO_TICKS(relaySleepTimeout[id]*1000), portMAX_DELAY) != pdPASS) {
      ESP_LOGE(TAG, "Cannot change period for relay %d timer", id);
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

void update_relay_sleep(int id, int onTimeout)
{
  ESP_LOGI(TAG, "update_relay_sleep: id: %d, value: %d", id, onTimeout);
  ESP_LOGI(TAG, "relayStatus[%d] = %d", id, relaySleepTimeout[id]);

  if (onTimeout != relaySleepTimeout[id]) {
    relaySleepTimeout[id] = onTimeout;
    update_timer(id);

    esp_err_t err = write_nvs_integer(relaySleepTag[id], relaySleepTimeout[id]);
    ESP_ERROR_CHECK( err );
  }
  publish_relay_timeout(id);
}

void handle_relay_task(void* pvParameters)
{
  ESP_LOGI(TAG, "handle_relay_cmd_task started");

  struct RelayMessage r;
  while(1) {
    if( xQueueReceive( relayQueue, &r , portMAX_DELAY) )
      {
        if (r.msgType == RELAY_CMD_STATUS) {
          update_relay_status(r.relayId, r.data);
          continue;
        }
        if (r.msgType == RELAY_CMD_SLEEP) {
          update_relay_sleep(r.relayId, r.data);
          continue;
        }
      }
  }
}
#endif //CONFIG_MQTT_RELAYS_NB
