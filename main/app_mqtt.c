#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "string.h"

#include "mqtt_client.h"

#include "app_main.h"

#include "app_sensors.h"
#include "app_mqtt.h"

#include "cJSON.h"

#ifdef CONFIG_MQTT_SCHEDULERS

#include "app_scheduler.h"
extern QueueHandle_t schedulerCfgQueue;
//FIXME hack until decide if queue+thread really usefull
extern struct SchedulerCfgMessage schedulerCfg;
//FIXME end hack until decide if queue+thread really usefull

#define SCHEDULER_TOPICS_NB 1

#else // CONFIG_MQTT_SCHEDULERS

#define SCHEDULER_TOPICS_NB 0

#endif // CONFIG_MQTT_SCHEDULERS

#if CONFIG_MQTT_RELAYS_NB
#include "app_relay.h"
extern QueueHandle_t relayCmdQueue;
extern QueueHandle_t relayCfgQueue;

#define RELAYS_TOPICS_NB 2 //CMD and CFG

#else //CONFIG_MQTT_RELAYS_NB

#define RELAYS_TOPICS_NB 0 //CMD and CFG

#endif //CONFIG_MQTT_RELAYS_NB

#ifdef CONFIG_MQTT_OTA

#include "app_ota.h"
extern QueueHandle_t otaQueue;
#define OTA_TOPIC CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/cmd/ota"
#define OTA_TOPICS_NB 1

#else // CONFIG_MQTT_OTA

#define OTA_TOPICS_NB 0

#endif // CONFIG_MQTT_OTA

#ifdef CONFIG_MQTT_THERMOSTAT

#include "app_thermostat.h"
extern QueueHandle_t thermostatQueue;
#define THERMOSTAT_TOPICS_NB 1
#define THERMOSTAT_TOPIC CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/cfg/thermostat"

#else // CONFIG_MQTT_THERMOSTAT

#define THERMOSTAT_TOPICS_NB 0

#endif // CONFIG_MQTT_THERMOSTAT

esp_mqtt_client_handle_t client = NULL;
int connect_reason;
const int mqtt_disconnect = 33; //32+1
const char * connect_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/connection";

EventGroupHandle_t mqtt_event_group;
const int MQTT_CONNECTED_BIT = BIT0;
const int MQTT_SUBSCRIBED_BIT = BIT1;
const int MQTT_PUBLISHED_BIT = BIT2;
const int MQTT_INIT_FINISHED_BIT = BIT3;

int mqtt_reconnect_counter;

#define FW_VERSION "0.02.12m"

extern QueueHandle_t mqttQueue;

static const char *TAG = "MQTTS_MQTTS";


#define NB_SUBSCRIPTIONS  (OTA_TOPICS_NB + THERMOSTAT_TOPICS_NB + RELAYS_TOPICS_NB + SCHEDULER_TOPICS_NB + CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB)

#define RELAY_CMD_TOPIC CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/cmd/relay/"

#define RELAY_CFG_TOPIC CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/cfg/relay/"

#define SCHEDULER_CFG_TOPIC CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/cfg/scheduler/"

const char *SUBSCRIPTIONS[NB_SUBSCRIPTIONS] =
  {
#ifdef CONFIG_MQTT_OTA
    OTA_TOPIC,
#endif //CONFIG_MQTT_OTA
#ifdef CONFIG_MQTT_SCHEDULERS
    SCHEDULER_CFG_TOPIC "+",
#endif // CONFIG_MQTT_SCHEDULERS
#if CONFIG_MQTT_RELAYS_NB
    RELAY_CMD_TOPIC "+",
    RELAY_CFG_TOPIC "+",
#endif //CONFIG_MQTT_RELAYS_NB
#ifdef CONFIG_MQTT_THERMOSTAT
    THERMOSTAT_TOPIC,
#endif // CONFIG_MQTT_THERMOSTAT
#if CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB
    CONFIG_MQTT_THERMOSTAT_ROOM_0_SENSORS_TOPIC,
#if CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 1
    CONFIG_MQTT_THERMOSTAT_ROOM_1_SENSORS_TOPIC,
#if CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 2
    CONFIG_MQTT_THERMOSTAT_ROOM_2_SENSORS_TOPIC,
#if CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 3
    CONFIG_MQTT_THERMOSTAT_ROOM_3_SENSORS_TOPIC,
#endif //CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 3
#endif //CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 2
#endif //CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 1
#endif //CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB
  };


extern const char mqtt_iot_cipex_ro_pem_start[] asm("_binary_mqtt_iot_cipex_ro_pem_start");

unsigned char get_topic_id(esp_mqtt_event_handle_t event, int maxTopics, const char * topic)
{
  char fullTopic[MQTT_MAX_TOPIC_LEN];
  memset(fullTopic,0,MQTT_MAX_TOPIC_LEN);

  unsigned char topicId = 0;
  bool found = false;
  while (!found && topicId <= maxTopics) {
    sprintf(fullTopic, "%s%d", topic, topicId);
    if (strncmp(event->topic, fullTopic, strlen(fullTopic)) == 0) {
      found = true;
    } else {
      topicId++;
    }
  }
  if (!found) {
    topicId = JSON_BAD_TOPIC_ID;
  }
  return topicId;
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
          ret= value;
        }
      cJSON_Delete(root);
    }
  return ret;
}

bool handle_scheduler_mqtt_event(esp_mqtt_event_handle_t event)
{
#ifdef CONFIG_MQTT_SCHEDULERS
  unsigned char schedulerId = get_topic_id(event, MAX_SCHEDULER_NB, SCHEDULER_CFG_TOPIC);

  if (schedulerId != JSON_BAD_TOPIC_ID) {
    if (event->data_len >= MAX_MQTT_DATA_SCHEDULER) {
      ESP_LOGI(TAG, "unexpected scheduler cfg payload length");
      return true;
    }
    struct SchedulerCfgMessage s = {0, 0, 0, 0, {{0}}};
    s.schedulerId = schedulerId;

    char tmpBuf[MAX_MQTT_DATA_SCHEDULER];
    memcpy(tmpBuf, event->data, event->data_len);
    tmpBuf[event->data_len] = 0;
    cJSON * root   = cJSON_Parse(tmpBuf);
    if (root) {
      cJSON * timestamp = cJSON_GetObjectItem(root,"ts");
      if (timestamp) {
        s.timestamp = timestamp->valueint;
      }
      cJSON * actionId = cJSON_GetObjectItem(root,"aId");
      if (actionId) {
        s.actionId = actionId->valueint;
      }
      cJSON * actionState = cJSON_GetObjectItem(root,"aState");
      if (actionState) {
        s.actionState = actionState->valueint;
      }
      cJSON * data = cJSON_GetObjectItem(root,"data");
      if (data) {
        if (s.actionId == ADD_RELAY_ACTION) {
          cJSON * relayId = cJSON_GetObjectItem(data,"relayId");
          if (relayId) {
            s.data.relayActionData.relayId = relayId->valueint;
          }
          cJSON * relayValue = cJSON_GetObjectItem(data,"relayValue");
          if (relayValue) {
            s.data.relayActionData.relayValue = relayValue->valueint;
          }
        }
      }
      cJSON_Delete(root);

      if (xQueueSend(schedulerCfgQueue
                     ,( void * )&s
                     ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
        ESP_LOGE(TAG, "Cannot send to scheduleCfgQueue");
      }
    }
    return true;
  }
#endif // CONFIG_MQTT_SCHEDULERS
  return false;
}

bool handle_relay_cfg_mqtt_event(esp_mqtt_event_handle_t event)
{
#if CONFIG_MQTT_RELAYS_NB
  unsigned char relayId = get_topic_id(event, CONFIG_MQTT_RELAYS_NB, RELAY_CFG_TOPIC);
  if(relayId != JSON_BAD_TOPIC_ID) {
    if (event->data_len >= MAX_MQTT_DATA_LEN_RELAY) {
      ESP_LOGI(TAG, "unexpected relay cfg payload length");
      return true;
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
    return true;
  }
#endif //CONFIG_MQTT_RELAYS_NB
  return false;
}



bool handle_relay_cmd_mqtt_event(esp_mqtt_event_handle_t event)
{
#if CONFIG_MQTT_RELAYS_NB
  unsigned char relayId = get_topic_id(event, CONFIG_MQTT_RELAYS_NB, RELAY_CMD_TOPIC);
  if(relayId != JSON_BAD_TOPIC_ID) {
    if (event->data_len >= MAX_MQTT_DATA_LEN_RELAY) {
      ESP_LOGI(TAG, "unexpected relay cmd payload length");
      return true;
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
    return true;
  }
#endif //CONFIG_MQTT_RELAYS_NB
  return false;
}

bool handle_ota_mqtt_event(esp_mqtt_event_handle_t event)
{
#ifdef CONFIG_MQTT_OTA
  if (strncmp(event->topic, OTA_TOPIC, strlen(OTA_TOPIC)) == 0) {
    struct OtaMessage o={"https://sw.iot.cipex.ro:8911/" CONFIG_MQTT_CLIENT_ID ".bin"};
    if (xQueueSend( otaQueue
                    ,( void * )&o
                    ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to otaQueue");

    }
    return true;
  }
#endif //CONFIG_MQTT_OTA
  return false;
}

bool getTemperatureValue(short* value, const cJSON* root, const char* tag)
{
  cJSON * object = cJSON_GetObjectItem(root,tag);
  if (object) {
    *value = (short) (object->valuedouble * 10);
    ESP_LOGI(TAG, "%s: %d.%01d", tag, (*value)/10, (*value)%10);
    return true;
  }
  return false;
}

bool handle_thermostat_mqtt_event(esp_mqtt_event_handle_t event)
{
#ifdef CONFIG_MQTT_THERMOSTAT
  if (strncmp(event->topic, THERMOSTAT_TOPIC, strlen(THERMOSTAT_TOPIC)) == 0) {
    if (event->data_len >= MAX_MQTT_DATA_THERMOSTAT )
      {
        ESP_LOGI(TAG, "unexpected thermostat cmd payload length");
        return true;
      }
    char tmpBuf[MAX_MQTT_DATA_THERMOSTAT];
    memcpy(tmpBuf, event->data, event->data_len);
    tmpBuf[event->data_len] = 0;
    cJSON * root   = cJSON_Parse(tmpBuf);
    if (root) {
      struct ThermostatMessage tm;
      memset(&tm, 0, sizeof(struct ThermostatMessage));
      tm.msgType = THERMOSTAT_CFG_MSG;
      bool updated = false;
      if (getTemperatureValue(&tm.data.cfgData.circuitTargetTemperature,
                              root, "circuitTargetTemperature")) {
        updated = true;
      }
      if (getTemperatureValue(&tm.data.cfgData.waterTargetTemperature,
                              root, "waterTargetTemperature")) {
        updated = true;
      }
      if (getTemperatureValue(&tm.data.cfgData.waterTemperatureSensibility,
                              root, "waterTemperatureSensibility")) {
        updated = true;
      }
      if (getTemperatureValue(&tm.data.cfgData.room0TargetTemperature,
                              root, "room0TargetTemperature")) {
        updated = true;
      }
      if (getTemperatureValue(&tm.data.cfgData.room0TemperatureSensibility,
                              root, "room0TemperatureSensibility")) {
        updated = true;
      }

      cJSON * tmMode = cJSON_GetObjectItem(root,"thermostatMode");
      if (tmMode) {
        tm.data.cfgData.thermostatMode = tmMode->valueint;
        ESP_LOGI(TAG, "thermostatMode: %d", tm.data.cfgData.thermostatMode);
        updated = true;
      }

      cJSON * holdOffMode = cJSON_GetObjectItem(root,"holdOffMode");
      if (holdOffMode) {
        tm.data.cfgData.holdOffMode = holdOffMode->valueint;
        ESP_LOGI(TAG, "holdOffMode: %d", tm.data.cfgData.holdOffMode);
        updated = true;
      }

      if (updated) {
        if (xQueueSend( thermostatQueue
                        ,( void * )&tm
                        ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
          ESP_LOGE(TAG, "Cannot send to thermostatQueue");
        }
      }
      cJSON_Delete(root);
    }
    return true;
  }
#endif // CONFIG_MQTT_THERMOSTAT
  return false;
}

bool handle_room_sensors_mqtt_event(esp_mqtt_event_handle_t event)
{
#if CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0
  if (strncmp(event->topic, CONFIG_MQTT_THERMOSTAT_ROOM_0_SENSORS_TOPIC, strlen(CONFIG_MQTT_THERMOSTAT_ROOM_0_SENSORS_TOPIC)) == 0) {
    if (event->data_len >= MAX_MQTT_DATA_SENSORS )
      {
        ESP_LOGI(TAG, "unexpected room sensors cfg payload length");
        return true;
      }
    char tmpBuf[MAX_MQTT_DATA_SENSORS];
    memcpy(tmpBuf, event->data, event->data_len);
    tmpBuf[event->data_len] = 0;
    cJSON * root = cJSON_Parse(tmpBuf);
    if (root) {
      struct ThermostatRoomMessage t;
      if (getTemperatureValue(&t.temperature, root, "temperature")) {
        struct ThermostatMessage tm;
        memset(&tm, 0, sizeof(struct ThermostatMessage));
        tm.msgType = THERMOSTAT_ROOM_0_MSG;
        tm.data.roomData = t;
        if (xQueueSend( thermostatQueue
                        ,( void * )&tm
                        ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
          ESP_LOGE(TAG, "Cannot send to thermostatQueue");
        }
      }
      cJSON_Delete(root);
    }
    return true;
  }

#endif // MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0
  return false;
}
void dispatch_mqtt_event(esp_mqtt_event_handle_t event)
{
  if (handle_scheduler_mqtt_event(event))
    return;
  if (handle_room_sensors_mqtt_event(event))
    return;
  if (handle_relay_cfg_mqtt_event(event))
    return;
  if (handle_relay_cmd_mqtt_event(event))
    return;
  if (handle_ota_mqtt_event(event))
    return;
  if (handle_thermostat_mqtt_event(event))
    return;
}

void mqtt_publish_data(const char * topic,
                       const char * data,
                       int qos, int retain)
{
  if (xEventGroupGetBits(mqtt_event_group) & MQTT_INIT_FINISHED_BIT) {
    xEventGroupClearBits(mqtt_event_group, MQTT_PUBLISHED_BIT);
    int msg_id = esp_mqtt_client_publish(client, topic, data, strlen(data), qos, retain);
    if (qos == QOS_0) {
      ESP_LOGI(TAG, "published qos0 data");
    } else if (msg_id > 0) {
      ESP_LOGI(TAG, "sent publish data successful, msg_id=%d", msg_id);
      EventBits_t bits = xEventGroupWaitBits(mqtt_event_group, MQTT_PUBLISHED_BIT, false, true, MQTT_FLAG_TIMEOUT);
      if (bits & MQTT_PUBLISHED_BIT) {
        ESP_LOGI(TAG, "publish ack received, msg_id=%d", msg_id);
      } else {
        ESP_LOGW(TAG, "publish ack not received, msg_id=%d", msg_id);
      }
    } else {
      ESP_LOGW(TAG, "failed to publish qos1, msg_id=%d", msg_id);
    }
  }
}

void publish_connected_data()
{
  char data[256];
  memset(data,0,256);

  sprintf(data, "{\"state\":\"connected\", \"v\":\"" FW_VERSION "\", \"connect_reason\":%d}", connect_reason);

  mqtt_publish_data(connect_topic, data, QOS_1, RETAIN);
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

void mqtt_init_and_start()
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
  client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_start(client);
  xEventGroupWaitBits(mqtt_event_group, MQTT_CONNECTED_BIT, false, true, portMAX_DELAY);
}

void handle_mqtt_sub_pub(void* pvParameters)
{
  connect_reason=esp_reset_reason();
  void * unused;
  while(1) {
    if( xQueueReceive( mqttQueue, &unused , portMAX_DELAY) )
      {
        xEventGroupClearBits(mqtt_event_group, MQTT_INIT_FINISHED_BIT);
        mqtt_subscribe(client);
        xEventGroupSetBits(mqtt_event_group, MQTT_INIT_FINISHED_BIT);
        publish_connected_data();
#if CONFIG_MQTT_RELAYS_NB
        publish_all_relays_data();
        publish_all_relays_cfg_data();
#endif//CONFIG_MQTT_RELAYS_NB
#ifdef CONFIG_MQTT_THERMOSTAT
        publish_thermostat_data();
#endif // CONFIG_MQTT_THERMOSTAT
#ifdef CONFIG_MQTT_OTA
        publish_ota_data(OTA_READY);
#endif //CONFIG_MQTT_OTA
#ifdef CONFIG_MQTT_SENSOR
        publish_sensors_data();
#endif//
      }
  }
}


