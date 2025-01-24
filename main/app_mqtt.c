#include "esp_system.h"
#ifdef CONFIG_NORTH_INTERFACE_MQTT

#include "esp_log.h"
#include "esp_wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "version.h"

#include <string.h>
#include <stdlib.h>

#include "mqtt_client.h"

#include "app_main.h"

#include "app_sensors.h"
#include "app_publish_data.h"
#include "app_handle_cmd.h"

#include "app_mqtt.h"


esp_mqtt_client_handle_t client = NULL;
int connect_reason;
const int mqtt_disconnect = 33; //32+1

const char * available_topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/status/available";
const char * config_topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/config/available";

EventGroupHandle_t mqtt_event_group;
const int MQTT_CONNECTED_BIT = BIT0;
const int MQTT_SUBSCRIBED_BIT = BIT1;
const int MQTT_PUBLISHED_BIT = BIT2;
const int MQTT_INIT_FINISHED_BIT = BIT3;

int mqtt_reconnect_counter;

extern QueueHandle_t mqttQueue;
extern SemaphoreHandle_t xSemaphore;

static const char *TAG = "MQTTS_MQTTS";

#ifdef CONFIG_MQTT_THERMOSTATS_NB0_SENSOR_TYPE_MQTT
#define CONFIG_MQTT_THERMOSTATS_NB0_SENSOR_MQTT 1
#else //CONFIG_MQTT_THERMOSTATS_NB0_SENSOR_TYPE_MQTT
#define CONFIG_MQTT_THERMOSTATS_NB0_SENSOR_MQTT 0
#endif //CONFIG_MQTT_THERMOSTATS_NB0_SENSOR_TYPE_MQTT

#ifdef CONFIG_MQTT_THERMOSTATS_NB1_SENSOR_TYPE_MQTT
#define CONFIG_MQTT_THERMOSTATS_NB1_SENSOR_MQTT 1
#else //CONFIG_MQTT_THERMOSTATS_NB1_SENSOR_TYPE_MQTT
#define CONFIG_MQTT_THERMOSTATS_NB1_SENSOR_MQTT 0
#endif //CONFIG_MQTT_THERMOSTATS_NB1_SENSOR_TYPE_MQTT

#ifdef CONFIG_MQTT_THERMOSTATS_NB2_SENSOR_TYPE_MQTT
#define CONFIG_MQTT_THERMOSTATS_NB2_SENSOR_MQTT 1
#else //CONFIG_MQTT_THERMOSTATS_NB2_SENSOR_TYPE_MQTT
#define CONFIG_MQTT_THERMOSTATS_NB2_SENSOR_MQTT 0
#endif //CONFIG_MQTT_THERMOSTATS_NB2_SENSOR_TYPE_MQTT

#ifdef CONFIG_MQTT_THERMOSTATS_NB3_SENSOR_TYPE_MQTT
#define CONFIG_MQTT_THERMOSTATS_NB3_SENSOR_MQTT 1
#else //CONFIG_MQTT_THERMOSTATS_NB3_SENSOR_TYPE_MQTT
#define CONFIG_MQTT_THERMOSTATS_NB3_SENSOR_MQTT 0
#endif //CONFIG_MQTT_THERMOSTATS_NB3_SENSOR_TYPE_MQTT

#ifdef CONFIG_MQTT_THERMOSTATS_NB4_SENSOR_TYPE_MQTT
#define CONFIG_MQTT_THERMOSTATS_NB4_SENSOR_MQTT 1
#else //CONFIG_MQTT_THERMOSTATS_NB4_SENSOR_TYPE_MQTT
#define CONFIG_MQTT_THERMOSTATS_NB4_SENSOR_MQTT 0
#endif //CONFIG_MQTT_THERMOSTATS_NB4_SENSOR_TYPE_MQTT

#ifdef CONFIG_MQTT_THERMOSTATS_NB5_SENSOR_TYPE_MQTT
#define CONFIG_MQTT_THERMOSTATS_NB5_SENSOR_MQTT 1
#else //CONFIG_MQTT_THERMOSTATS_NB5_SENSOR_TYPE_MQTT
#define CONFIG_MQTT_THERMOSTATS_NB5_SENSOR_MQTT 0
#endif //CONFIG_MQTT_THERMOSTATS_NB5_SENSOR_TYPE_MQTT

#define CONFIG_MQTT_THERMOSTATS_MQTT_SENSORS (CONFIG_MQTT_THERMOSTATS_NB0_SENSOR_MQTT + CONFIG_MQTT_THERMOSTATS_NB1_SENSOR_MQTT + CONFIG_MQTT_THERMOSTATS_NB2_SENSOR_MQTT + CONFIG_MQTT_THERMOSTATS_NB3_SENSOR_MQTT + CONFIG_MQTT_THERMOSTATS_NB4_SENSOR_MQTT + CONFIG_MQTT_THERMOSTATS_NB5_SENSOR_MQTT)

#define CMD_ACTION_TYPE_TOPIC CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/cmd/#"
#define CMD_ACTION_TYPE_TOPICS_NB 1

#ifdef CONFIG_SOUTH_INTERFACE_UDP
#define SOUTH_INTERFACE_UDP_TOPICS_NB 1
#define SOUTH_INTERFACE_UDP_TOPIC CONFIG_SOUTH_INTERFACE_UDP_DEVICE_TYPE "/" CONFIG_SOUTH_INTERFACE_UDP_CLIENT_ID "/cmd/#"
#else // CONFIG_SOUTH_INTERFACE_UDP
#define SOUTH_INTERFACE_UDP_TOPICS_NB 0
#endif // CONFIG_SOUTH_INTERFACE_UDP

#define NB_SUBSCRIPTIONS  (CMD_ACTION_TYPE_TOPICS_NB + CONFIG_MQTT_THERMOSTATS_MQTT_SENSORS + SOUTH_INTERFACE_UDP_TOPICS_NB)

const char *SUBSCRIPTIONS[NB_SUBSCRIPTIONS] =
  {
    CMD_ACTION_TYPE_TOPIC,
#ifdef CONFIG_MQTT_THERMOSTATS_NB0_SENSOR_TYPE_MQTT
    CONFIG_MQTT_THERMOSTATS_NB0_MQTT_SENSOR_TOPIC,
#endif //CONFIG_MQTT_THERMOSTATS_NB0_SENSOR_TYPE_MQTT
#ifdef CONFIG_MQTT_THERMOSTATS_NB1_SENSOR_TYPE_MQTT
    CONFIG_MQTT_THERMOSTATS_NB1_MQTT_SENSOR_TOPIC,
#endif //CONFIG_MQTT_THERMOSTATS_NB1_SENSOR_TYPE_MQTT
#ifdef CONFIG_MQTT_THERMOSTATS_NB2_SENSOR_TYPE_MQTT
    CONFIG_MQTT_THERMOSTATS_NB2_MQTT_SENSOR_TOPIC,
#endif //CONFIG_MQTT_THERMOSTATS_NB2_SENSOR_TYPE_MQTT
#ifdef CONFIG_MQTT_THERMOSTATS_NB3_SENSOR_TYPE_MQTT
    CONFIG_MQTT_THERMOSTATS_NB3_MQTT_SENSOR_TOPIC,
#endif //CONFIG_MQTT_THERMOSTATS_NB3_SENSOR_TYPE_MQTT
#ifdef CONFIG_MQTT_THERMOSTATS_NB4_SENSOR_TYPE_MQTT
    CONFIG_MQTT_THERMOSTATS_NB4_MQTT_SENSOR_TOPIC,
#endif //CONFIG_MQTT_THERMOSTATS_NB4_SENSOR_TYPE_MQTT
#ifdef CONFIG_MQTT_THERMOSTATS_NB5_SENSOR_TYPE_MQTT
    CONFIG_MQTT_THERMOSTATS_NB5_MQTT_SENSOR_TOPIC,
#endif //CONFIG_MQTT_THERMOSTATS_NB5s_SENSOR_TYPE_MQTT
#ifdef CONFIG_SOUTH_INTERFACE_UDP
    SOUTH_INTERFACE_UDP_TOPIC,
#endif // CONFIG_SOUTH_INTERFACE_UDP
  };


extern const char cert_bundle_pem_start[] asm("_binary_cert_bundle_pem_start");

void mqtt_publish_data(const char * topic,
                       const char * data,
                       int qos, int retain)
{
  if (xEventGroupGetBits(mqtt_event_group) & MQTT_INIT_FINISHED_BIT) {
    if (( xSemaphore != NULL ) && xSemaphoreTake( xSemaphore, 2 * MQTT_FLAG_TIMEOUT ) == pdTRUE) {
      xEventGroupClearBits(mqtt_event_group, MQTT_PUBLISHED_BIT);
      int msg_id = esp_mqtt_client_publish(client, topic, data, strlen(data), qos, retain);
      if (qos == QOS_0) {
        ESP_LOGI(TAG, "published qos0 data, topic: %s", topic);
      } else if (msg_id > 0) {
        ESP_LOGI(TAG, "published qos1 data, msg_id=%d, topic=%s", msg_id, topic);
        EventBits_t bits = xEventGroupWaitBits(mqtt_event_group, MQTT_PUBLISHED_BIT, false, true, MQTT_FLAG_TIMEOUT);
        if (bits & MQTT_PUBLISHED_BIT) {
          ESP_LOGI(TAG, "publish ack received, msg_id=%d", msg_id);
        } else {
          ESP_LOGW(TAG, "publish ack not received, msg_id=%d", msg_id);
        }
      } else {
        ESP_LOGW(TAG, "failed to publish qos1, msg_id=%d", msg_id);
      }
      xSemaphoreGive( xSemaphore );
    } else {
      ESP_LOGW(TAG, "cannot get semaphore");
    }
  } else {
    ESP_LOGW(TAG, "cannot publish topic %s, mqtt init not finished", topic);
  }
}

void publish_config_msg()
{
  char data[64];
  memset(data,0,64);

  sprintf(data, "{\"fw_version\":\"" FW_VERSION "\", \"connect_reason\":%d}", connect_reason);
  mqtt_publish_data(config_topic, data, QOS_1, RETAIN);
}

void publish_available_msg()
{
  const char* data = "online";
  mqtt_publish_data(available_topic, data, QOS_1, RETAIN);
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
  switch (event->event_id) {
  case MQTT_EVENT_CONNECTED:
    xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_BIT);
    void * unused;
    if (xQueueSend( mqttQueue
                    ,( void * )&unused
                    ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to mqttQueue");
    }
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    mqtt_reconnect_counter=0;
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
    connect_reason=mqtt_disconnect;
    xEventGroupClearBits(mqtt_event_group, MQTT_CONNECTED_BIT | MQTT_SUBSCRIBED_BIT | MQTT_PUBLISHED_BIT | MQTT_INIT_FINISHED_BIT);
    mqtt_reconnect_counter += 1; //one reconnect each 10 seconds
    ESP_LOGI(TAG, "mqtt_reconnect_counter: %d", mqtt_reconnect_counter);
    if (mqtt_reconnect_counter  > (6 * 5)) //5 mins, force wifi reconnect
      {
        esp_wifi_stop();
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        esp_wifi_start();
        mqtt_reconnect_counter = 0;
      }
    break;

  case MQTT_EVENT_SUBSCRIBED:
    xEventGroupSetBits(mqtt_event_group, MQTT_SUBSCRIBED_BIT);
    ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_PUBLISHED:
    xEventGroupSetBits(mqtt_event_group, MQTT_PUBLISHED_BIT);
    ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_DATA:
    ESP_LOGI(TAG, "MQTT_EVENT_DATA");
    ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
    ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
    handle_cmd_topic(event->topic, event->topic_len, event->data, event->data_len);
    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
    break;
  case MQTT_EVENT_BEFORE_CONNECT:
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
    break;
#ifdef CONFIG_TARGET_DEVICE_ESP32
  case MQTT_EVENT_ANY:
    ESP_LOGI(TAG, "MQTT_EVENT_ANY");
    break;
#endif //CONFIG_TARGET_DEVICE_ESP32
  }
  return ESP_OK;
}

#ifndef CONFIG_DEEP_SLEEP_MODE
static void mqtt_subscribe(esp_mqtt_client_handle_t client)
{
  int msg_id;

  for (int i = 0; i < NB_SUBSCRIPTIONS; i++) {
    if (strlen(SUBSCRIPTIONS[i])) {
      xEventGroupClearBits(mqtt_event_group, MQTT_SUBSCRIBED_BIT);
      msg_id = esp_mqtt_client_subscribe(client, SUBSCRIPTIONS[i], 1);
      if (msg_id > 0) {
        ESP_LOGI(TAG, "sent subscribe %s successful, msg_id=%d", SUBSCRIPTIONS[i], msg_id);
        EventBits_t bits = xEventGroupWaitBits(mqtt_event_group, MQTT_SUBSCRIBED_BIT, false, true, MQTT_FLAG_TIMEOUT);
        if (bits & MQTT_SUBSCRIBED_BIT) {
          ESP_LOGI(TAG, "subscribe ack received, msg_id=%d", msg_id);
        } else {
          ESP_LOGW(TAG, "subscribe ack not received, msg_id=%d", msg_id);
        }
      } else {
        ESP_LOGW(TAG, "failed to subscribe %s, msg_id=%d", SUBSCRIPTIONS[i], msg_id);
      }
    }
  }
}
#endif // CONFIG_DEEP_SLEEP_MODE


void mqtt_init_and_start()
{
#ifndef CONFIG_DEEP_SLEEP_MODE
  const char * lwtmsg = "offline";
#endif // CONFIG_DEEP_SLEEP_MODE
  const esp_mqtt_client_config_t mqtt_cfg = {
    .uri = "mqtts://" CONFIG_MQTT_USERNAME ":" CONFIG_MQTT_PASSWORD "@" CONFIG_MQTT_SERVER ":" CONFIG_MQTT_PORT,
    .event_handle = mqtt_event_handler,
    .cert_pem = (const char *)cert_bundle_pem_start,
    .client_id = CONFIG_CLIENT_ID,
#ifndef CONFIG_DEEP_SLEEP_MODE
    .lwt_topic = available_topic,
    .lwt_msg = lwtmsg,
    .lwt_qos = 1,
    .lwt_retain = 1,
    .lwt_msg_len = strlen(lwtmsg),
#endif // CONFIG_DEEP_SLEEP_MODE

    .keepalive = MQTT_TIMEOUT
  };

  client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_start(client);
  xEventGroupWaitBits(mqtt_event_group, MQTT_CONNECTED_BIT, false, true, portMAX_DELAY);
}

void handle_mqtt_sub_pub(void* pvParameters)
{
  connect_reason=esp_reset_reason();
  ESP_LOGI(TAG, "Reset reason: %d", connect_reason);
  void * unused;
  while(1) {
    if( xQueueReceive( mqttQueue, &unused , portMAX_DELAY) )
      {
        xEventGroupClearBits(mqtt_event_group, MQTT_INIT_FINISHED_BIT);
#ifndef CONFIG_DEEP_SLEEP_MODE
        mqtt_subscribe(client);
#endif // CONFIG_DEEP_SLEEP_MODE
        xEventGroupSetBits(mqtt_event_group, MQTT_INIT_FINISHED_BIT);
#ifndef CONFIG_DEEP_SLEEP_MODE
        publish_available_msg();
        publish_config_msg();
#endif // CONFIG_DEEP_SLEEP_MODE
        xEventGroupSetBits(mqtt_event_group, MQTT_INIT_FINISHED_BIT);
#if CONFIG_MQTT_RELAYS_NB
        publish_all_relays_status();
        publish_all_relays_timeout();
#endif//CONFIG_MQTT_RELAYS_NB
#if CONFIG_MQTT_THERMOSTATS_NB > 0
        publish_thermostat_data();
#endif // CONFIG_MQTT_THERMOSTATS_NB > 0
#ifdef CONFIG_MQTT_SCHEDULERS
        publish_schedulers_data();
#endif // CONFIG_MQTT_SCHEDULERS
#ifdef CONFIG_MQTT_OTA
        publish_ota_data(OTA_READY);
#endif // CONFIG_MQTT_OTA
#ifdef CONFIG_SENSOR_SUPPORT
        publish_sensors_data();
#endif // CONFIG_SENSOR_SUPPORT
      }
  }
}

#endif // CONFIG_NORTH_INTERFACE_MQTT
