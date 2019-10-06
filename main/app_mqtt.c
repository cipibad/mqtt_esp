#include "esp_log.h"
#include "esp_wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "app_main.h"

#include "app_sensors.h"
#include "app_mqtt.h"

#include "cJSON.h"

#if CONFIG_MQTT_RELAYS_NB
#include "app_relay.h"
extern QueueHandle_t relayCmdQueue;
extern QueueHandle_t relayCfgQueue;

#include "app_scheduler.h"
extern QueueHandle_t schedulerCfgQueue;
//FIXME hack until decide if queue+thread really usefull
extern struct SchedulerCfgMessage schedulerCfg;
//FIXME end hack until decide if queue+thread really usefull

#define CONFIG_MQTT_SCHEDULER_NB 1

#else //CONFIG_MQTT_RELAYS_NB

#define CONFIG_MQTT_SCHEDULER_NB 0
#endif //CONFIG_MQTT_RELAYS_NB

#ifdef CONFIG_MQTT_OTA

#include "app_ota.h"
extern QueueHandle_t otaQueue;
#define OTA_TOPIC CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/cmd/ota"
#define OTA_NB 1

#else // CONFIG_MQTT_OTA

#define OTA_NB 0

#endif // CONFIG_MQTT_OTA

#ifdef CONFIG_MQTT_THERMOSTAT

#include "app_thermostat.h"
extern QueueHandle_t thermostatQueue;
#define THERMOSTAT_NB 1
#define THERMOSTAT_TOPIC CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/cmd/thermostat/cfg"

#else // CONFIG_MQTT_THERMOSTAT

#define THERMOSTAT_NB 0

#endif // CONFIG_MQTT_THERMOSTAT


int16_t connect_reason;
const int mqtt_disconnect = 33; //32+1
const char * connect_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/connection";

EventGroupHandle_t mqtt_event_group;
const int MQTT_CONNECTED_BIT = BIT0;
const int MQTT_SUBSCRIBED_BIT = BIT1;
const int MQTT_PUBLISHED_BIT = BIT2;
const int MQTT_INIT_FINISHED_BIT = BIT3;

int16_t mqtt_reconnect_counter;

#define FW_VERSION "0.02.10"

extern QueueHandle_t mqttQueue;

static const char *TAG = "MQTTS_MQTTS";


#define NB_SUBSCRIPTIONS  (OTA_NB + THERMOSTAT_NB + 2 * CONFIG_MQTT_RELAYS_NB + CONFIG_MQTT_SCHEDULER_NB)

#define RELAY_CMD_TOPIC CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/cmd/relay"

#define SCHEDULER_CMD_TOPIC CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/cmd/scheduler"

const char *SUBSCRIPTIONS[NB_SUBSCRIPTIONS] =
  {
#ifdef CONFIG_MQTT_OTA
    OTA_TOPIC,
#endif //CONFIG_MQTT_OTA
#if CONFIG_MQTT_RELAYS_NB
    SCHEDULER_CMD_TOPIC,
    RELAY_CMD_TOPIC"/0",
    RELAY_CMD_TOPIC"/0/cfg",
#if CONFIG_MQTT_RELAYS_NB > 1
    RELAY_CMD_TOPIC"/1",
    RELAY_CMD_TOPIC"/1/cfg",
#if CONFIG_MQTT_RELAYS_NB > 2
    RELAY_CMD_TOPIC"/2",
    RELAY_CMD_TOPIC"/2/cfg",
#if CONFIG_MQTT_RELAYS_NB > 3
    RELAY_CMD_TOPIC"/3",
    RELAY_CMD_TOPIC"/3/cfg",
#endif //CONFIG_MQTT_RELAYS_NB > 3
#endif //CONFIG_MQTT_RELAYS_NB > 2
#endif //CONFIG_MQTT_RELAYS_NB > 1
#endif //CONFIG_MQTT_RELAYS_NB
#ifdef CONFIG_MQTT_THERMOSTAT
    THERMOSTAT_TOPIC
#endif // CONFIG_MQTT_THERMOSTAT
  };


extern const uint8_t mqtt_iot_cipex_ro_pem_start[] asm("_binary_mqtt_iot_cipex_ro_pem_start");

/**
 * returns the value of a specific Json request
 * @param: tag - the json event, i.e: "state" or "onTimeout"
 * @param: event - mqtt event handler instance
 */
char get_relay_json_value(const char* tag, esp_mqtt_event_handle_t event);

void dispatch_mqtt_event(esp_mqtt_event_handle_t event)
{
#if CONFIG_MQTT_RELAYS_NB

  if (strncmp(event->topic, SCHEDULER_CMD_TOPIC, strlen(SCHEDULER_CMD_TOPIC)) == 0) {
    struct SchedulerCfgMessage s;

    char tmpBuf[64];
    memcpy(tmpBuf, event->data, event->data_len);
    tmpBuf[event->data_len] = 0;
    cJSON * root   = cJSON_Parse(tmpBuf);
    if (root) {
      cJSON * schedulerTimestamp = cJSON_GetObjectItem(root,"schTst");
      if (schedulerTimestamp) {
        s.schedulerTimestamp = schedulerTimestamp->valueint;
        cJSON_Delete(schedulerTimestamp);
      }
      //FIME add other fields, schRId, schRSt
      cJSON_Delete(root);

      //FIXME hack until decide if queue+thread really usefull
      schedulerCfg.schedulerTimestamp = s.schedulerTimestamp;

      /* if (xQueueSend(schedulerCfgQueue */
      /*                ,( void * )&s */
      /*                ,MQTT_QUEUE_TIMEOUT) != pdPASS) { */
      /*   ESP_LOGE(TAG, "Cannot send to scheduleCfgQueue"); */
      /* } */
      //FIXME end hack until decide if queue+thread really usefull
}
    return;
  }

  char relayId=JSON_BAD_RELAY_ID;
  if (strncmp(event->topic, RELAY_CMD_TOPIC "/0/cfg", strlen(RELAY_CMD_TOPIC "/0/cfg")) == 0) {
    relayId=0;
  }
  if (strncmp(event->topic, RELAY_CMD_TOPIC "/1/cfg", strlen(RELAY_CMD_TOPIC "/1/cfg")) == 0) {
    relayId=1;
  }
  if (strncmp(event->topic, RELAY_CMD_TOPIC "/2/cfg", strlen(RELAY_CMD_TOPIC "/2/cfg")) == 0) {
    relayId=2;
  }
  if (strncmp(event->topic, RELAY_CMD_TOPIC "/3/cfg", strlen(RELAY_CMD_TOPIC "/3/cfg")) == 0) {
    relayId=3;
  }
  if(relayId != JSON_BAD_RELAY_ID) {
     if (event->data_len >= MAX_MQTT_DATA_LEN_RELAY) {
       ESP_LOGI(TAG, "unexpected relay cfg payload");
       return;
     }
     char value = get_relay_json_value("onTimeout", event);
     if (value != JSON_BAD_RELAY_VALUE) {
        ESP_LOGI(TAG, "relayId: %d, onTimeout: %d", relayId, value);
        struct RelayCfgMessage r={relayId, value};
        if (xQueueSend( relayCfgQueue
                        ,( void * )&r
                        ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
           ESP_LOGE(TAG, "Cannot send to relayCfgQueue");
        }
    }
    else {
       ESP_LOGE(TAG, "bad json payload");
    }
    return;
  }

  relayId=JSON_BAD_RELAY_ID;
  if (strncmp(event->topic, RELAY_CMD_TOPIC "/0", strlen(RELAY_CMD_TOPIC "/0")) == 0) {
    relayId=0;
  }
  if (strncmp(event->topic, RELAY_CMD_TOPIC "/1", strlen(RELAY_CMD_TOPIC "/1")) == 0) {
    relayId=1;
  }
  if (strncmp(event->topic, RELAY_CMD_TOPIC "/2", strlen(RELAY_CMD_TOPIC "/2")) == 0) {
    relayId=2;
  }
  if (strncmp(event->topic, RELAY_CMD_TOPIC "/3", strlen(RELAY_CMD_TOPIC "/3")) == 0) {
    relayId=3;
  }
  if(relayId != JSON_BAD_RELAY_ID) {
    if (event->data_len >= MAX_MQTT_DATA_LEN_RELAY) {
       ESP_LOGI(TAG, "unexpected relay cmd payload");
       return;
    }
    char value = get_relay_json_value("state", event);
    if (value != JSON_BAD_RELAY_VALUE) {
       ESP_LOGI(TAG, "relayId: %d, value: %d", relayId, value);
       struct RelayCmdMessage r={relayId, value};
       if (xQueueSend( relayCmdQueue
                       ,( void * )&r
                       ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
          ESP_LOGE(TAG, "Cannot send to relayCmdQueue");
       }
    }
    else {
       ESP_LOGE(TAG, "bad json payload");
    }
    return;
  }
#endif //CONFIG_MQTT_RELAYS_NB

#ifdef CONFIG_MQTT_OTA
  if (strncmp(event->topic, OTA_TOPIC, strlen(OTA_TOPIC)) == 0) {
    struct OtaMessage o={"https://sw.iot.cipex.ro:8911/" CONFIG_MQTT_CLIENT_ID ".bin"};
    if (xQueueSend( otaQueue
                    ,( void * )&o
                    ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to otaQueue");

    }
    return;
  }
#endif //CONFIG_MQTT_OTA

#ifdef CONFIG_MQTT_THERMOSTAT
  if (strncmp(event->topic, THERMOSTAT_TOPIC, strlen(THERMOSTAT_TOPIC)) == 0) {
    if (event->data_len >= MAX_MQTT_DATA_THERMOSTAT )
      {
        ESP_LOGI(TAG, "unextected relay cmd payload");
        return;
      }
    char tmpBuf[MAX_MQTT_DATA_THERMOSTAT];
    memcpy(tmpBuf, event->data, event->data_len);
    tmpBuf[event->data_len] = 0;
    cJSON * root   = cJSON_Parse(tmpBuf);
    if (root) {
      cJSON * cttObject = cJSON_GetObjectItem(root,"columnTargetTemperature");
      cJSON * ttObject = cJSON_GetObjectItem(root,"targetTemperature");
      cJSON * ttsObject = cJSON_GetObjectItem(root,"targetTemperatureSensibility");
      struct ThermostatMessage t = {0,0,0};
      if (cttObject) {
        float columnTargetTemperature = cttObject->valuedouble;
        ESP_LOGI(TAG, "columnTargetTemperature: %f", columnTargetTemperature);
        t.columnTargetTemperature = columnTargetTemperature;
        cJSON_Delete(cttObject);
      }
      if (ttObject) {
        float targetTemperature = ttObject->valuedouble;
        ESP_LOGI(TAG, "targetTemperature: %f", targetTemperature);
        t.targetTemperature = targetTemperature;
        cJSON_Delete(ttObject);
      }
      if (ttsObject) {
        float targetTemperatureSensibility = ttsObject->valuedouble;
        ESP_LOGI(TAG, "targetTemperatureSensibility: %f", targetTemperatureSensibility);
        t.targetTemperatureSensibility = targetTemperatureSensibility;
        cJSON_Delete(ttsObject);
      }
      if (t.targetTemperature || t.targetTemperatureSensibility || t.columnTargetTemperature) {
        if (xQueueSend( thermostatQueue
                        ,( void * )&t
                        ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
          ESP_LOGE(TAG, "Cannot send to thermostatQueue");
        }
      }
      cJSON_Delete(root);
    }
    return;
  }
#endif // CONFIG_MQTT_THERMOSTAT
}

char get_relay_json_value(const char* tag, esp_mqtt_event_handle_t event)
{
  char ret = JSON_BAD_RELAY_VALUE;
  char tmpBuf[MAX_MQTT_DATA_LEN_RELAY];
  memcpy(tmpBuf, event->data, event->data_len);
  tmpBuf[event->data_len] = 0;
  cJSON * root   = cJSON_Parse(tmpBuf);
  if (root)
  {
     cJSON * state = cJSON_GetObjectItem(root, tag);
     if (state)
     {
        char value = state->valueint;
        cJSON_Delete(state);
        ret= value;
     }
     cJSON_Delete(root);
   }
   return ret;
}

void publish_connected_data(esp_mqtt_client_handle_t client)
{
  if (xEventGroupGetBits(mqtt_event_group) & MQTT_INIT_FINISHED_BIT)
    {
      char data[256];
      memset(data,0,256);

      sprintf(data, "{\"state\":\"connected\", \"v\":\"" FW_VERSION "\", \"connect_reason\":%d}", connect_reason);
      xEventGroupClearBits(mqtt_event_group, MQTT_PUBLISHED_BIT);
      int msg_id = esp_mqtt_client_publish(client, connect_topic, data,strlen(data), 1, 1);
      if (msg_id > 0) {
        ESP_LOGI(TAG, "sent publish connected data successful, msg_id=%d", msg_id);
        EventBits_t bits = xEventGroupWaitBits(mqtt_event_group, MQTT_PUBLISHED_BIT, false, true, MQTT_FLAG_TIMEOUT);
        if (bits & MQTT_PUBLISHED_BIT) {
          ESP_LOGI(TAG, "publish ack received, msg_id=%d", msg_id);
        } else {
          ESP_LOGW(TAG, "publish ack not received, msg_id=%d", msg_id);
        }
      } else {
        ESP_LOGW(TAG, "failed to publish connected data, msg_id=%d", msg_id);
      }
    }
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
    dispatch_mqtt_event(event);
    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
    break;
  }
  return ESP_OK;
}

static void mqtt_subscribe(esp_mqtt_client_handle_t client)
{
  int msg_id;

  for (int i = 0; i < NB_SUBSCRIPTIONS; i++) {
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

esp_mqtt_client_handle_t mqtt_init()
{
  const char * lwtmsg = "{\"state\":\"disconnected\"}";
  const esp_mqtt_client_config_t mqtt_cfg = {
    .uri = "mqtts://" CONFIG_MQTT_USERNAME ":" CONFIG_MQTT_PASSWORD "@" CONFIG_MQTT_SERVER ":" CONFIG_MQTT_PORT,
    .event_handle = mqtt_event_handler,
    .cert_pem = (const char *)mqtt_iot_cipex_ro_pem_start,
    .client_id = CONFIG_MQTT_CLIENT_ID,
    .lwt_topic = connect_topic,
    .lwt_msg = lwtmsg,
    .lwt_qos = 1,
    .lwt_retain = 1,
    .lwt_msg_len = strlen(lwtmsg),
    .keepalive = MQTT_TIMEOUT
  };

  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
  return client;
}

void mqtt_start(esp_mqtt_client_handle_t client)
{
  esp_mqtt_client_start(client);
  xEventGroupWaitBits(mqtt_event_group, MQTT_CONNECTED_BIT, false, true, portMAX_DELAY);
}

void handle_mqtt_sub_pub(void* pvParameters)
{
  connect_reason=esp_reset_reason();
  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;
  void * unused;
  while(1) {
    if( xQueueReceive( mqttQueue, &unused , portMAX_DELAY) )
      {
        xEventGroupClearBits(mqtt_event_group, MQTT_INIT_FINISHED_BIT);
        mqtt_subscribe(client);
        xEventGroupSetBits(mqtt_event_group, MQTT_INIT_FINISHED_BIT);
        publish_connected_data(client);
#if CONFIG_MQTT_RELAYS_NB
        publish_all_relays_data(client);
        publish_all_relays_cfg_data(client);
#endif//CONFIG_MQTT_RELAYS_NB
#ifdef CONFIG_MQTT_THERMOSTAT
        publish_thermostat_data(client);
#endif // CONFIG_MQTT_THERMOSTAT
#ifdef CONFIG_MQTT_OTA
        publish_ota_data(client, OTA_READY);
#endif //CONFIG_MQTT_OTA
#ifdef CONFIG_MQTT_SENSOR
        publish_sensors_data(client);
#endif//
      }
  }
}
