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

struct Relay relays[CONFIG_MQTT_RELAYS_NB];

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
    relays[i].id = i;
    relays[i].status = RELAY_OFF;
    relays[i].timeout = 0;
    relays[i].timer = NULL;
    gpio_pad_select_gpio(relayToGpioMap[i]);
    gpio_set_direction(relayToGpioMap[i], GPIO_MODE_OUTPUT);
    gpio_set_level(relayToGpioMap[i], RELAY_OFF);
  }
}

void publish_relay_data(int id, esp_mqtt_client_handle_t client)
{
  if (xEventGroupGetBits(mqtt_event_group) & MQTT_INIT_FINISHED_BIT)
    {
      const char * relays_topic = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/relay/";
      char data[64];
      memset(data,0,64);
      sprintf(data, "{\"state\":%d,\"onTimeout\":%d}",
              relays[id].status == RELAY_ON,
              relays[id].timeout);

      char topic[MQTT_MAX_TOPIC_LEN];
      memset(topic,0,MQTT_MAX_TOPIC_LEN);
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
  ESP_LOGI(TAG, "update_timer for %d, timeout: %d", id, relays[id].timeout);

  if ((relays[id].status == RELAY_OFF)) {
    if (relays[id].timer != NULL) {
      if (xTimerIsTimerActive(relays[id].timer) != pdFALSE){
        ESP_LOGI(TAG, "found started timer, stopping");
        xTimerStop( relays[id].timer, portMAX_DELAY );
      }
    }
  }

  if ((relays[id].status == RELAY_ON) && relays[id].timeout) {
    if (relays[id].timer == NULL) {
      ESP_LOGI(TAG, "creating timer");
      relays[id].timer =
        xTimerCreate( "relayTimer",           /* Text name. */
                      pdMS_TO_TICKS(relays[id].timeout*1000),  /* Period. */
                      pdFALSE,                /* Autoreload. */
                      (void *)id,                  /* No ID. */
                      vTimerCallback );  /* Callback function. */
    }
    if( relays[id].timer != NULL ) {
      ESP_LOGI(TAG, "timer is created");
      if (xTimerIsTimerActive(relays[id].timer) != pdFALSE){
        ESP_LOGI(TAG, "timer is active, stopping");
        xTimerStop( relays[id].timer, portMAX_DELAY );
      }

      TickType_t xTimerPeriod = xTimerGetPeriod(relays[id].timer);
      if (xTimerPeriod != pdMS_TO_TICKS(relays[id].timeout*1000)) {
        ESP_LOGI(TAG, "timer change period, starting also");
        xTimerChangePeriod(relays[id].timer, pdMS_TO_TICKS(relays[id].timeout*1000), portMAX_DELAY ); //FIXME check return value
      }
      else {
        ESP_LOGI(TAG, "timer starting");
        xTimerStart( relays[id].timer, portMAX_DELAY );
      }
    }
  }
}

void update_relay_state(int id, char value, esp_mqtt_client_handle_t client)
{
  ESP_LOGI(TAG, "update_relay_state: id: %d, value: %d", id, value);
  ESP_LOGI(TAG, "relays[%d].status = %d", id, relays[id].status);
  if (value != (relays[id].status == RELAY_ON)) {
    if (value == 1) {
      relays[id].status = RELAY_ON;
      ESP_LOGI(TAG, "enabling GPIO %d", relayToGpioMap[id]);
    }
    if (value == 0) {
      relays[id].status = RELAY_OFF;
      ESP_LOGI(TAG, "disabling GPIO %d", relayToGpioMap[id]);
    }
    gpio_set_level(relayToGpioMap[id], relays[id].status);
    update_timer(id);
  }
  publish_relay_data(id, client);
}

void update_relay_onTimeout(int id, char onTimeout, esp_mqtt_client_handle_t client)
{
  ESP_LOGI(TAG, "update_relay_onTimeout: id: %d, value: %d", id, onTimeout);
  ESP_LOGI(TAG, "relays[%d].timeout = %d", id, relays[id].timeout);

  if (onTimeout != relays[id].timeout) {
    relays[id].timeout = onTimeout;
    update_timer(id);

    //FIXME extract NVS to separate function
    char onTimeoutTag[32]; //FIXME duplication with read function
    snprintf(onTimeoutTag, 32, "relayOnTimeout%d", id); //FIXME should check return value
    esp_err_t err = write_nvs_integer(onTimeoutTag, relays[id].timeout);
    ESP_ERROR_CHECK( err );
  }
  publish_relay_data(id, client);
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
  int id;
  int onTimeout;
  while(1) {
    if( xQueueReceive( relayCfgQueue, &r , portMAX_DELAY) )
      {
        id=r.relayId;
        onTimeout=r.onTimeout;
        update_relay_onTimeout(id, onTimeout, client);
      }
  }
}

#endif //CONFIG_MQTT_RELAYS_NB
