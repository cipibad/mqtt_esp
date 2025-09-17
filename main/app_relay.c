#include "esp_log.h"

#include "driver/gpio.h"
#include "rom/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "driver/uart.h"

#include <string.h>

#include "app_main.h"
#include "app_relay.h"
#include "app_nvs.h"
#include "app_at.h"

#include "app_publish_data.h"

#if CONFIG_MQTT_RELAYS_NB

#define QUEUE_TIMEOUT (60 * 1000 / portTICK_PERIOD_MS)


int relayStatus[CONFIG_MQTT_RELAYS_NB];
int relaySleepTimeout[CONFIG_MQTT_RELAYS_NB];
TimerHandle_t relaySleepTimer[CONFIG_MQTT_RELAYS_NB];

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

inline bool is_relay_gpio_type(int id)
{
  switch(id) {
  #ifdef CONFIG_MQTT_RELAY_0_TYPE_GPIO
    case 0:
      return true;
  #endif // CONFIG_MQTT_RELAY_0_TYPE_GPIO
  #ifdef CONFIG_MQTT_RELAY_1_TYPE_GPIO
    case 1:
      return true;
  #endif // CONFIG_MQTT_RELAY_1_TYPE_GPIO
  #ifdef CONFIG_MQTT_RELAY_2_TYPE_GPIO
    case 2:
      return true;
  #endif // CONFIG_MQTT_RELAY_2_TYPE_GPIO
  #ifdef CONFIG_MQTT_RELAY_3_TYPE_GPIO
    case 3:
      return true;
  #endif // CONFIG_MQTT_RELAY_3_TYPE_GPIO
    default:
      return false;
  }
}

inline int get_relay_gpio(const char * tag, int id)
{
  switch(id) {
  #ifdef CONFIG_MQTT_RELAY_0_GPIO
    case 0:
      return CONFIG_MQTT_RELAY_0_GPIO;
  #endif // CONFIG_MQTT_RELAY_0_GPIO
  #ifdef CONFIG_MQTT_RELAY_1_GPIO
    case 1:
      return CONFIG_MQTT_RELAY_1_GPIO;
  #endif // CONFIG_MQTT_RELAY_1_GPIO
  #ifdef CONFIG_MQTT_RELAY_2_GPIO
    case 2:
      return CONFIG_MQTT_RELAY_2_GPIO;
  #endif // CONFIG_MQTT_RELAY_2_GPIO
  #ifdef CONFIG_MQTT_RELAY_3_GPIO
    case 3:
      return CONFIG_MQTT_RELAY_3_GPIO;
  #endif // CONFIG_MQTT_RELAY_3_GPIO
    default:
      ESP_LOGE(tag, "Cannot get relay gpio for %d", id);
      return -1;
  }
}

bool is_relay_serial_type(int id)
{
  switch(id) {
#ifdef CONFIG_AT_SERVER
  #ifdef CONFIG_MQTT_RELAY_0_TYPE_SERIAL
    case 0:
      return true;
  #endif // CONFIG_MQTT_RELAY_0_TYPE_SERIAL
  #ifdef CONFIG_MQTT_RELAY_1_TYPE_SERIAL
    case 1:
      return true;
  #endif // CONFIG_MQTT_RELAY_1_TYPE_SERIAL
  #ifdef CONFIG_MQTT_RELAY_2_TYPE_SERIAL
    case 2:
      return true;
  #endif // CONFIG_MQTT_RELAY_2_TYPE_SERIAL
  #ifdef CONFIG_MQTT_RELAY_3_TYPE_SERIAL
    case 3:
      return true;
  #endif // CONFIG_MQTT_RELAY_3_TYPE_SERIAL
#endif // CONFIG_AT_SERVER
    default:
      return false;
  }
}

#ifdef CONFIG_AT_SERVER

inline int get_relay_serial_on_cmd(const char * tag, int id)
{
  switch(id) {
  #ifdef CONFIG_MQTT_RELAY_0_SERIAL_ON
    case 0:
      return CONFIG_MQTT_RELAY_0_SERIAL_ON;
  #endif // CONFIG_MQTT_RELAY_0_SERIAL_ON
  #ifdef CONFIG_MQTT_RELAY_1_SERIAL_ON
    case 1:
      return CONFIG_MQTT_RELAY_1_SERIAL_ON;
  #endif // CONFIG_MQTT_RELAY_1_SERIAL_ON
  #ifdef CONFIG_MQTT_RELAY_2_SERIAL_ON
    case 2:
      return CONFIG_MQTT_RELAY_2_SERIAL_ON;
  #endif // CONFIG_MQTT_RELAY_2_SERIAL_ON
  #ifdef CONFIG_MQTT_RELAY_3_SERIAL_ON
    case 3:
      return CONFIG_MQTT_RELAY_3_SERIAL_ON;
  #endif // CONFIG_MQTT_RELAY_3_SERIAL_ON
    default:
      ESP_LOGE(tag, "Cannot get serial on cmd for %d", id);
      return -1;
  }
}

inline int get_relay_serial_off_cmd(const char * tag, int id)
{
  switch(id) {
  #ifdef CONFIG_MQTT_RELAY_0_SERIAL_OFF
    case 0:
      return CONFIG_MQTT_RELAY_0_SERIAL_OFF;
  #endif // CONFIG_MQTT_RELAY_0_SERIAL_OFF
  #ifdef CONFIG_MQTT_RELAY_1_SERIAL_OFF
    case 1:
      return CONFIG_MQTT_RELAY_1_SERIAL_OFF;
  #endif // CONFIG_MQTT_RELAY_1_SERIAL_OFF
  #ifdef CONFIG_MQTT_RELAY_2_SERIAL_OFF
    case 2:
      return CONFIG_MQTT_RELAY_2_SERIAL_OFF;
  #endif // CONFIG_MQTT_RELAY_2_SERIAL_OFF
  #ifdef CONFIG_MQTT_RELAY_3_SERIAL_OFF
    case 3:
      return CONFIG_MQTT_RELAY_3_SERIAL_OFF;
  #endif // CONFIG_MQTT_RELAY_3_SERIAL_OFF
    default:
      ESP_LOGE(tag, "Cannot get serial off cmd for %d", id);
      return -1;
  }
}
#endif // CONFIG_AT_SERVER

void vTimerCallback( TimerHandle_t xTimer )
{
  int id = (int)pvTimerGetTimerID( xTimer );
  ESP_LOGI(TAG, "timer %d expired, sending stop msg", id);
  struct RelayMessage r = {RELAY_CMD_STATUS, id, RELAY_STATUS_OFF};
  if (xQueueSend( relayQueue
                  ,( void * )&r
                  ,QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to relayQueue");
  }
}

void relays_init()
{
  for(int i = 0; i < CONFIG_MQTT_RELAYS_NB; i++) {
    relayStatus[i] = GPIO_LOW;
    if (is_relay_gpio_type(i)) {
      gpio_pad_select_gpio(get_relay_gpio(TAG, i));
      gpio_set_direction(get_relay_gpio(TAG, i), GPIO_MODE_OUTPUT);
      gpio_set_level(get_relay_gpio(TAG, i), relayStatus[i]);
    }
    esp_err_t err=read_nvs_integer(relaySleepTag[i], &relaySleepTimeout[i]);
    ESP_ERROR_CHECK( err );

    relaySleepTimer[i] = NULL;
  }
}

void publish_relay_availability(int id)
{
  const char * relays_topic = CONFIG_DEVICE_TYPE"/"CONFIG_CLIENT_ID"/evt/available/relay";
  char data[16];
  memset(data,0,16);

  bool online = true;
#ifdef CONFIG_AT_SERVER
  online = is_serial_interface_online();
#endif // CONFIG_AT_SERVER

  sprintf(data, "%s", online ? "online" : "offline");

  char topic[MAX_TOPIC_LEN];
  memset(topic,0,MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", relays_topic, id);

  publish_persistent_data(topic, data);
}

void publish_relay_status(int id)
{
  const char * relays_topic = CONFIG_DEVICE_TYPE"/"CONFIG_CLIENT_ID"/evt/status/relay";
  char data[16];
  memset(data,0,16);
  sprintf(data, "%s", relayStatus[id] == GPIO_HIGH ? "ON" : "OFF");

  char topic[MAX_TOPIC_LEN];
  memset(topic,0,MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", relays_topic, id);

  publish_persistent_data(topic, data);
}

void publish_relay_timeout(int id)
{
  const char * relays_topic = CONFIG_DEVICE_TYPE"/"CONFIG_CLIENT_ID"/evt/sleep/relay";
  char data[16];
  memset(data,0,16);
  sprintf(data, "%d", relaySleepTimeout[id]);

  char topic[MAX_TOPIC_LEN];
  memset(topic,0,MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", relays_topic, id);

  publish_non_persistent_data(topic, data);
}

void publish_all_relays_status()
{
  for(int id = 0; id < CONFIG_MQTT_RELAYS_NB; id++) {
    publish_relay_status(id);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void publish_all_relays_availability()
{
  for(int id = 0; id < CONFIG_MQTT_RELAYS_NB; id++) {
    publish_relay_availability(id);
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
  if ((relayStatus[id] == GPIO_HIGH) && relaySleepTimeout[id] != 0) {
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
      ESP_LOGE(TAG, "No Timer can be created for %d, cannot handle timeout", id);
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
  ESP_LOGI(TAG, "relayStatus[%d] = %d", id, relayStatus[id] == GPIO_HIGH);
  if (is_relay_serial_type(id) && !is_serial_interface_online()) {
    return;
  }

  if (value != (relayStatus[id] == GPIO_HIGH)) {
    if (value == RELAY_STATUS_ON) {
      relayStatus[id] = GPIO_HIGH;
      ESP_LOGI(TAG, "enabling relay %d", id);
    }
    if (value == RELAY_STATUS_OFF) {
      relayStatus[id] = GPIO_LOW;
      ESP_LOGI(TAG, "disabling relay %d", id);
    }
    if (is_relay_gpio_type(id)) {
      ESP_LOGI(TAG, "updating GPIO %d", get_relay_gpio(TAG, id));
      gpio_set_level(get_relay_gpio(TAG, id), relayStatus[id]);
    }
    #ifdef CONFIG_AT_SERVER
    else if (is_relay_serial_type(id)) {
        char at_cmd[16];
        unsigned int cmd;
        if (relayStatus[id] == GPIO_HIGH) {
          cmd = get_relay_serial_on_cmd(TAG, id);
        } else {
          cmd = get_relay_serial_off_cmd(TAG, id);
        }
        sprintf(at_cmd, "+IPD,0,4:%c%c%c%c\r\n",
          (cmd >> 24) & 0xFF,
          (cmd >> 16) & 0xFF,
          (cmd >> 8) & 0xFF,
          cmd & 0xFF
        );

        ESP_LOGI(TAG, "sending serial cmd %s", at_cmd);
        uart_write_bytes(UART_NUM_0, at_cmd, 16);
    }
    #endif // CONFIG_AT_SERVER

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
