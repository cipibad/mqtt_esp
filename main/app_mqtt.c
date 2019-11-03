#include "esp_log.h"
#include "esp_wifi.h"
#include "mqtt_client.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "app_main.h"
#include "app_mqtt.h"


//forward declaration start

/**
 * returns the value of a specific Json request
 * @param: tag - the json event, i.e: "state" or "onTimeout"
 * @param: event - mqtt event handler instance
 */
char get_relay_json_value(const char* tag, esp_mqtt_event_handle_t event);

unsigned char get_topic_id(esp_mqtt_event_handle_t event, int maxTopics, const char * topic);

bool handle_scheduler_mqtt_event(esp_mqtt_event_handle_t event);
bool handle_relay_cfg_mqtt_event(esp_mqtt_event_handle_t event);
bool handle_relay_cmd_mqtt_event(esp_mqtt_event_handle_t event);
bool handle_ota_mqtt_event(esp_mqtt_event_handle_t event);
bool handle_thermostat_mqtt_event(esp_mqtt_event_handle_t event);

//forward declaration end

#ifdef CONFIG_MQTT_SENSOR
#include "app_sensors.h"
extern QueueHandle_t sensorQueue;
#endif //CONFIG_MQTT_SENSOR


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

#define SCHEDULER_TOPICS_NB 1
#define RELAYS_TOPICS_NB 2 //CMD and CFG

#else //CONFIG_MQTT_RELAYS_NB

#define SCHEDULER_TOPICS_NB 0
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


int16_t connect_reason;
const int mqtt_disconnect = 33; //32+1
const char * connect_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/connection";
esp_mqtt_client_handle_t client = NULL;


EventGroupHandle_t mqtt_event_group;
const int MQTT_CONNECTED_BIT = BIT0;
const int MQTT_SUBSCRIBED_BIT = BIT1;
const int MQTT_PUBLISHED_BIT = BIT2;
const int MQTT_INIT_FINISHED_BIT = BIT3;

int16_t mqtt_reconnect_counter;

#define FW_VERSION "0.02.12a"

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
#if CONFIG_MQTT_RELAYS_NB
    SCHEDULER_CFG_TOPIC "+",
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


extern const uint8_t mqtt_iot_cipex_ro_pem_start[] asm("_binary_mqtt_iot_cipex_ro_pem_start");

bool handle_scheduler_mqtt_event(esp_mqtt_event_handle_t event)
{
#if CONFIG_MQTT_RELAYS_NB
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
#endif //CONFIG_MQTT_RELAYS_NB
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
      struct RelayCMessage r;
      r.msgType = RELAY_CFG;
      r.relayCfg.relayId = relayId;
      r.relayCfg.onTimeout = value;
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
      struct RelayMessage r;
      r.msgType = RELAY_CMD;
      r.relayData.relayId = relayId;
      r.relayData.relayValue = value;

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
    struct OtaMessage m = {
      OTA_MQTT_UPGRADE,
      {"https://sw.iot.cipex.ro:8911/" CONFIG_MQTT_CLIENT_ID ".bin"}
    };
    if (xQueueSend( otaQueue
                    ,( void * )&m
                    ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to otaQueue");

    }
    return true;
  }
#endif //CONFIG_MQTT_OTA
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
      struct ThermostatCfgMessage t;
      memset(&t, 0, sizeof(struct ThermostatCfgMessage));
      cJSON * cttObject = cJSON_GetObjectItem(root,"columnTargetTemperature");
      if (cttObject) {
        int32_t columnTargetTemperature = cttObject->valuedouble * 10;
        ESP_LOGI(TAG, "columnTargetTemperature: %d.%01d0",
                 columnTargetTemperature/10,
                 columnTargetTemperature%10);
        t.columnTargetTemperature = columnTargetTemperature;
      }
      cJSON * wttObject = cJSON_GetObjectItem(root,"waterTargetTemperature");
      if (wttObject) {
        int32_t waterTargetTemperature = wttObject->valuedouble * 10;
        ESP_LOGI(TAG, "waterTargetTemperature: %d.%01d",
                 waterTargetTemperature/10,
                 waterTargetTemperature%10);
        t.waterTargetTemperature = waterTargetTemperature;
      }
      cJSON * wtsObject = cJSON_GetObjectItem(root,"waterTemperatureSensibility");
      if (wtsObject) {
        int32_t waterTemperatureSensibility = wtsObject->valuedouble * 10;
        ESP_LOGI(TAG, "waterTemperatureSensibility: %d.%01d",
                 waterTemperatureSensibility/10,
                 waterTemperatureSensibility%10);
        t.waterTemperatureSensibility = waterTemperatureSensibility;
      }
      cJSON * r0ttObject = cJSON_GetObjectItem(root,"room0TargetTemperature");
      if (r0ttObject) {
        int32_t room0TargetTemperature = r0ttObject->valuedouble * 10;
        ESP_LOGI(TAG, "room0TargetTemperature: %d.%01d",
                 room0TargetTemperature/10,
                 room0TargetTemperature%10);
        t.room0TargetTemperature = room0TargetTemperature;
      }
      cJSON * r0tsObject = cJSON_GetObjectItem(root,"room0TemperatureSensibility");
      if (r0tsObject) {
        int32_t room0TemperatureSensibility = r0tsObject->valuedouble * 10;
        ESP_LOGI(TAG, "room0TemperatureSensibility: %d.%01d",
                 room0TemperatureSensibility/10,
                 room0TemperatureSensibility%10);
        t.room0TemperatureSensibility = room0TemperatureSensibility;
      }
      if (t.columnTargetTemperature ||
          t.waterTargetTemperature || t.waterTemperatureSensibility ||
          t.room0TargetTemperature || t.room0TemperatureSensibility) {
        struct ThermostatMessage tm;
        memset(&tm, 0, sizeof(struct ThermostatMessage));
        tm.msgType = THERMOSTAT_CFG_MSG;
        tm.data.cfgData = t;
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
  ESP_LOGI(TAG, "got topic");
  if (strncmp(event->topic, CONFIG_MQTT_THERMOSTAT_ROOM_0_SENSORS_TOPIC, strlen(CONFIG_MQTT_THERMOSTAT_ROOM_0_SENSORS_TOPIC)) == 0) {
    ESP_LOGI(TAG, "got room0 topic");

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
      ESP_LOGI(TAG, "got root");
      struct ThermostatRoomMessage t;
      cJSON * tObject = cJSON_GetObjectItem(root,"temperature");
      if (tObject) {
        ESP_LOGI(TAG, "got got temperature");
        float temperature = tObject->valuedouble;
        ESP_LOGI(TAG, "temperature: %f", temperature);
        t.temperature = temperature * 10;
      }
      if (t.temperature) {
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


void mqtt_publish(const char* topic, const char* data, int qos, int retain)
{
  if (xEventGroupGetBits(mqtt_event_group) & MQTT_INIT_FINISHED_BIT) {
    xEventGroupClearBits(mqtt_event_group, MQTT_PUBLISHED_BIT);
    int msg_id = esp_mqtt_client_publish(client, topic,
                                         data, strlen(data), qos, retain);

    if (qos == QOS_0) {
      ESP_LOGI(TAG, "sent QOS==0 publish data, msg_id=%d", msg_id);
    } else if (msg_id > 0) {
      ESP_LOGI(TAG, "sent QOS!=0 publish data successful, msg_id=%d", msg_id);
      EventBits_t bits = xEventGroupWaitBits(mqtt_event_group, MQTT_PUBLISHED_BIT, false, true, MQTT_FLAG_TIMEOUT);
      if (bits & MQTT_PUBLISHED_BIT) {
        ESP_LOGI(TAG, "publish ack received, msg_id=%d", msg_id);
      } else {
        ESP_LOGW(TAG, "publish ack not received, msg_id=%d", msg_id);
      }
    } else {
      ESP_LOGW(TAG, "failed to publish QOS!=0 data, msg_id=%d", msg_id);
    }
  }
}

void mqtt_publish_msg(const struct MqttPublishData* msg)
{
  mqtt_publish(msg->topic, msg->data, msg->qos, msg->retain);
}

void publish_connected_data()
{
  char data[256];
  memset(data,0,256);
  sprintf(data, "{\"state\":\"connected\", \"v\":\"" FW_VERSION "\", \"connect_reason\":%d}", connect_reason);

  mqtt_publish(connect_topic, data, QOS_1, RETAIN);
}


static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
  switch (event->event_id) {
  case MQTT_EVENT_CONNECTED:
    xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_BIT);
    struct MqttMessage m;
    memset(&m, 0, sizeof(struct MqttMessage));
    m.msgType = MQTT_CONNECTED;
    if (xQueueSend( mqttQueue
                    ,( void * )&m
                    ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send MQTT_EVENT_CONNECTED to mqttQueue");
    }
    ESP_LOGI(TAG, "Sent MQTT_EVENT_CONNECTED to mqttQueue");
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

static void mqtt_subscribe()
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

void notify_relay_mqtt_connected()
{
  struct RelayMessage r;
  r.msgType = RELAY_MQTT_CONNECTED;
  if (xQueueSend(relayCmdQueue
                  ,(void *)&r
                  ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to relayCmdQueue");
  }
}

void notify_relay_cfg_mqtt_connected()
{
  struct RelayCMessage r;
  r.msgType = RELAY_CFG_MQTT_CONNECTED;
  if (xQueueSend(relayCfgQueue
                  ,(void *)&r
                  ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to relayCfgQueue");
  }
}

#ifdef CONFIG_MQTT_THERMOSTAT
void notify_thermostat_mqtt_connected()
{
  struct ThermostatMessage m;
  m.msgType = THERMOSTAT_MQTT_CONNECTED;
  if (xQueueSend(thermostatQueue
                  ,(void *)&m
                  ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to thermostatQueue");
  }
}
#endif // CONFIG_MQTT_THERMOSTAT
#ifdef CONFIG_MQTT_OTA
void notify_ota_mqtt_connected()
{
  struct OtaMessage m;
  m.msgType = OTA_MQTT_CONNECTED;
  if (xQueueSend(otaQueue
                  ,(void *)&m
                  ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to otaQueue");
  }
}
#endif //CONFIG_MQTT_OTA

#ifdef CONFIG_MQTT_SENSOR
void notify_sensor_mqtt_connected()
{
  struct SensorMessage m;
  m.msgType = SENSOR_MQTT_CONNECTED;
  if (xQueueSend(sensorQueue
                  ,(void *)&m
                  ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to sensorQueue");
  }
}
#endif//CONFIG_MQTT_SENSOR


void handle_mqtt_sub_pub(void* pvPrameters)
{
  connect_reason=esp_reset_reason();
  struct MqttMessage msg;
  while(1) {
    if(xQueueReceive(mqttQueue, &msg, portMAX_DELAY))
      {
        if (msg.msgType == MQTT_CONNECTED) {
          mqtt_subscribe();
          xEventGroupSetBits(mqtt_event_group, MQTT_INIT_FINISHED_BIT);
          publish_connected_data();

          //FIXME ongoing broadcast connect
          notify_relay_mqtt_connected();
          notify_relay_cfg_mqtt_connected();
#ifdef CONFIG_MQTT_THERMOSTAT
          //FIXME this code if not practically enabled ...
          notify_thermostat_mqtt_connected();
#endif // CONFIG_MQTT_THERMOSTAT
#ifdef CONFIG_MQTT_OTA
          notify_ota_mqtt_connected();
#endif //CONFIG_MQTT_OTA
#ifdef CONFIG_MQTT_SENSOR
          notify_sensor_mqtt_connected();
#endif//
        }
        if (msg.msgType == MQTT_PUBLISH) {
          mqtt_publish_msg(&msg.publishData);
        }
      }
  }
}
