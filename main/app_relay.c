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

extern QueueHandle_t relayCmdQueue;
extern QueueHandle_t relayCfgQueue;
extern QueueHandle_t mqttQueue;

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

void publish_relay_data(int id)
{
  struct MqttMsg m;
  memset(&m, 0, sizeof(struct MqttMsg));
  m.msgType = MQTT_PUBLISH;

  const char * relays_topic = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/relay/";
  sprintf(m.publishData.topic, "%s%d", relays_topic, id);

  sprintf(m.publishData.data, "{\"state\":%d}", relayStatus[id] == RELAY_ON);

  if (xQueueSend(mqttQueue
                 ,( void * )&m
                 ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send publishRelay to mqttQueue");
  }
  ESP_LOGE(TAG, "Sent publishRelay to mqttQueue");
}

void publish_relay_cfg_data(int id)
{
  struct MqttMsg m;
  memset(&m, 0, sizeof(struct MqttMsg));
  m.msgType = MQTT_PUBLISH;

  const char * relays_topic = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/relay/";
  sprintf(m.publishData.topic, "%s%d/cfg", relays_topic, id);

  sprintf(m.publishData.data, "{\"onTimeout\":%d}", relayOnTimeout[id]);

  if (xQueueSend(mqttQueue
                 ,( void * )&m
                 ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send publishCfgRelay to mqttQueue");
  }
  ESP_LOGE(TAG, "Sent publishCfgRelay to mqttQueue");
}


void publish_all_relays_data()
{
  for(int id = 0; id < CONFIG_MQTT_RELAYS_NB; id++) {
    publish_relay_data(id);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void publish_all_relays_cfg_data()
{
  for(int id = 0; id < CONFIG_MQTT_RELAYS_NB; id++) {
    publish_relay_cfg_data(id);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void vTimerCallback( TimerHandle_t xTimer )
{
  int id = (int)pvTimerGetTimerID( xTimer );
  ESP_LOGI(TAG, "timer %d expired, sending stop msg", id);
  struct RelayMessage r;
  r.msgType = RELAY_CMD;
  r.relayData.relayId = id;
  r.relayData.relayValue = 0;
  if (xQueueSend( relayCmdQueue
                  ,( void * )&r
                  ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to relayCmdQueue");
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

void update_relay_state(int id, char value)
{
  ESP_LOGI(TAG, "update_relay_state: id: %d, value: %d", id, value);
  ESP_LOGI(TAG, "relayStatus[%d] = %d", id, relayStatus[id]);
  if (value != (relayStatus[id] == RELAY_ON)) {
    if (value == 1) {
      relayStatus[id] = RELAY_ON;
      ESP_LOGI(TAG, "enabling GPIO %d", relayToGpioMap[id]);
    }
    if (value == 0) {
      relayStatus[id] = RELAY_OFF;
      ESP_LOGI(TAG, "disabling GPIO %d", relayToGpioMap[id]);
    }
    gpio_set_level(relayToGpioMap[id], relayStatus[id]);
    update_timer(id);
  }
  publish_relay_data(id);
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
  publish_relay_cfg_data(id);
}

void handle_relay_cmd_task(void* pvParameters)
{
  ESP_LOGI(TAG, "handle_relay_cmd_task started");
  struct RelayMessage r;
  while(1) {
    if( xQueueReceive( relayCmdQueue, &r , portMAX_DELAY) )
      {
        if (r.msgType == RELAY_MQTT_CONNECTED) {
          publish_all_relays_data();
        }
        if (r.msgType == RELAY_CMD) {
          update_relay_state(r.relayData.relayId, r.relayData.relayValue);
        }
      }
  }
}

void handle_relay_cfg_task(void* pvParameters)
{
  ESP_LOGI(TAG, "handle_relay_cfg_task started");
  struct RelayCMessage r;
  while(1) {
    if( xQueueReceive(relayCfgQueue, &r , portMAX_DELAY))
      {
        if (r.msgType == RELAY_CFG_MQTT_CONNECTED) {
          publish_all_relays_cfg_data();
        }

        if (r.msgType == RELAY_CFG) {
          update_relay_onTimeout(r.relayCfg.relayId, r.relayCfg.onTimeout);
        }
      }
  }
}

#endif //CONFIG_MQTT_RELAYS_NB
