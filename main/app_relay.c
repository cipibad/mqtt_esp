#include "esp_log.h"

#include "driver/gpio.h"
#include "rom/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "app_main.h"
#include "app_relay.h"
#include "app_nvs.h"

extern EventGroupHandle_t mqtt_event_group;
extern const int MQTT_INIT_FINISHED_BIT;
extern const int MQTT_PUBLISHED_BIT;

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

void publish_relay_data(int id, esp_mqtt_client_handle_t client)
{
  if (xEventGroupGetBits(mqtt_event_group) & MQTT_INIT_FINISHED_BIT)
    {
      const char * relays_topic = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/relay/";
      char data[32];
      memset(data,0,32);
      sprintf(data, "{\"state\":%d}", relayStatus[id] == RELAY_ON);

      char topic[64];
      memset(topic,0,64);
      sprintf(topic, "%s%d", relays_topic, id);

      xEventGroupClearBits(mqtt_event_group, MQTT_PUBLISHED_BIT);
      int msg_id = esp_mqtt_client_publish(client, topic, data,strlen(data), 1, 1);
      if (msg_id > 0) {
        ESP_LOGI(TAG, "sent publish relay successful, msg_id=%d", msg_id);
        EventBits_t bits = xEventGroupWaitBits(mqtt_event_group, MQTT_PUBLISHED_BIT, false, true, MQTT_FLAG_TIMEOUT);
        if (bits & MQTT_PUBLISHED_BIT) {
          ESP_LOGI(TAG, "publish ack received, msg_id=%d", msg_id);
        } else {
          ESP_LOGW(TAG, "publish ack not received, msg_id=%d", msg_id);
        }
      } else {
        ESP_LOGW(TAG, "failed to publish relay, msg_id=%d", msg_id);
      }
    }
}

void publish_all_relays_cfg_data(esp_mqtt_client_handle_t client)
{

  if (xEventGroupGetBits(mqtt_event_group) & MQTT_INIT_FINISHED_BIT)
    {
      const char * topic = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/relay/cfg";

      char data[256];
      memset(data,0,256);
      strcat(data, "{");

      char t[16];
      for(int id = 0; id < CONFIG_MQTT_RELAYS_NB; id++) {
        sprintf(t, "\"onTimeout%d\":%d,",
                id, relayOnTimeout[id]);
        strcat(data, t);
      }
      data[strlen(data)-1] = 0;
      strcat(data, "}");

      xEventGroupClearBits(mqtt_event_group, MQTT_PUBLISHED_BIT);
      int msg_id = esp_mqtt_client_publish(client, topic, data,strlen(data), 1, 1);
      if (msg_id > 0) {
        ESP_LOGI(TAG, "sent publish relay cfg successful, msg_id=%d", msg_id);
        EventBits_t bits = xEventGroupWaitBits(mqtt_event_group, MQTT_PUBLISHED_BIT, false, true, MQTT_FLAG_TIMEOUT);
        if (bits & MQTT_PUBLISHED_BIT) {
          ESP_LOGI(TAG, "publish ack received, msg_id=%d", msg_id);
        } else {
          ESP_LOGW(TAG, "publish ack not received, msg_id=%d", msg_id);
        }
      } else {
        ESP_LOGW(TAG, "failed to publish relay cfg, msg_id=%d", msg_id);
      }
    }
}

void publish_all_relays_data(esp_mqtt_client_handle_t client)
{
  for(int id = 0; id < CONFIG_MQTT_RELAYS_NB; id++) {
    publish_relay_data(id, client);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void vTimerCallback( TimerHandle_t xTimer )
{
  int id = (int)pvTimerGetTimerID( xTimer );
  ESP_LOGI(TAG, "timer %d expired, sending stop msg", id);
  struct RelayCmdMessage r={id, 0};
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

void update_relay_state(int id, char value, esp_mqtt_client_handle_t client)
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
  publish_relay_data(id, client);
}

int update_relay_onTimeout(int id, char onTimeout, esp_mqtt_client_handle_t client)
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
  return 1;
}

void handle_relay_cmd_task(void* pvParameters)
{
  ESP_LOGI(TAG, "handle_relay_cmd_task started");
  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;
  struct RelayCmdMessage r;
  int id;
  int value;
  while(1) {
    if( xQueueReceive( relayCmdQueue, &r , portMAX_DELAY) )
      {
        id=r.relayId;
        value=r.relayValue;
        update_relay_state(id, value, client);
      }
  }
}

void handle_relay_cfg_task(void* pvParameters)
{
  ESP_LOGI(TAG, "handle_relay_cfg_task started");
  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;
  struct RelayCfgMessage r;
  while(1) {
    if( xQueueReceive( relayCfgQueue, &r , portMAX_DELAY) )
      {
        for(int id = 0; id < CONFIG_MQTT_RELAYS_NB; id++) {
          if (r.onTimeout[id] != -1) {
            update_relay_onTimeout(id, r.onTimeout[id], client);
          }
        }
        publish_all_relays_cfg_data(client);
      }
  }
}

#endif //CONFIG_MQTT_RELAYS_NB
