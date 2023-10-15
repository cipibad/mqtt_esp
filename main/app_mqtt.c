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
#include "app_mqtt.h"

#include "cJSON.h"

#ifdef CONFIG_MQTT_SCHEDULERS

#include "app_scheduler.h"
extern QueueHandle_t schedulerCfgQueue;
//FIXME hack until decide if queue+thread really usefull
extern struct SchedulerCfgMessage schedulerCfg;
//FIXME end hack until decide if queue+thread really usefull

#define SCHEDULER_CFG_TOPIC CONFIG_DEVICE_TYPE"/"CONFIG_CLIENT_ID"/cfg/scheduler/"
#define SCHEDULER_TOPICS_NB 1

#else // CONFIG_MQTT_SCHEDULERS

#define SCHEDULER_TOPICS_NB 0

#endif // CONFIG_MQTT_SCHEDULERS

#if CONFIG_MQTT_RELAYS_NB
#include "app_relay.h"
extern QueueHandle_t relayQueue;

#endif //CONFIG_MQTT_RELAYS_NB

#ifdef CONFIG_MQTT_OTA

#include "app_system.h"

#include "app_ota.h"
extern QueueHandle_t otaQueue;
#define OTA_TOPIC CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/cmd/ota"
#endif // CONFIG_MQTT_OTA

#if CONFIG_MQTT_THERMOSTATS_NB > 0

#include "app_thermostat.h"
extern QueueHandle_t thermostatQueue;

#endif // CONFIG_MQTT_THERMOSTATS_NB > 0

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

#define CONFIG_MQTT_THERMOSTATS_MQTT_SENSORS (CONFIG_MQTT_THERMOSTATS_NB0_SENSOR_MQTT + CONFIG_MQTT_THERMOSTATS_NB1_SENSOR_MQTT + CONFIG_MQTT_THERMOSTATS_NB2_SENSOR_MQTT + CONFIG_MQTT_THERMOSTATS_NB3_SENSOR_MQTT)

#define CMD_ACTION_TYPE_TOPIC CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/cmd/#"
#define CMD_ACTION_TYPE_TOPICS_NB 1

#define NB_SUBSCRIPTIONS  (CMD_ACTION_TYPE_TOPICS_NB + SCHEDULER_TOPICS_NB + CONFIG_MQTT_THERMOSTATS_MQTT_SENSORS)

const char *SUBSCRIPTIONS[NB_SUBSCRIPTIONS] =
  {
    CMD_ACTION_TYPE_TOPIC,
#ifdef CONFIG_MQTT_SCHEDULERS
    SCHEDULER_CFG_TOPIC "+",
#endif // CONFIG_MQTT_SCHEDULERS
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
  };


extern const char cert_bundle_pem_start[] asm("_binary_cert_bundle_pem_start");

unsigned char get_topic_id(esp_mqtt_event_handle_t event, int maxTopics, const char * topic)
{
  char fullTopic[MAX_TOPIC_LEN];
  memset(fullTopic, 0, MAX_TOPIC_LEN);

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
            s.data.relayActionData.data = relayValue->valueint;
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


bool handle_ota_mqtt_event(esp_mqtt_event_handle_t event)
{
#ifdef CONFIG_MQTT_OTA
  if (strncmp(event->topic, OTA_TOPIC, strlen(OTA_TOPIC)) == 0) {
    struct OtaMessage o={"https://sw.iot.cipex.ro:8911/" CONFIG_CLIENT_ID ".bin"};
    if (xQueueSend( otaQueue
                    ,( void * )&o
                    ,OTA_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to otaQueue");

    }
    return true;
  }
#endif //CONFIG_MQTT_OTA
  return false;
}

char* getToken(char* buffer, const char* topic, int topic_len, unsigned char place)
{
  if (topic == NULL)
    return NULL;

  if (topic_len >= 64)
    return NULL;

  char str[64];
  memcpy(str, topic, topic_len);
  str[topic_len] = 0;

  char *token = strtok(str, "/");
  int i = 0;
  while(i < place && token) {
    token = strtok(NULL, "/");
    i += 1;
  }
  if (!token)
    return NULL;

  return strcpy(buffer, token);
}

char* getActionType(char* actionType, const char* topic, int topic_len)
{
  return getToken(actionType, topic, topic_len, 2);
}
char* getAction(char* action, const char* topic, int topic_len)
{
  return getToken(action, topic, topic_len, 3);
}

char* getService(char* service, const char* topic, int topic_len)
{
  return getToken(service, topic, topic_len, 4);
}

signed char getServiceId(const char* topic, int topic_len)
{
  signed char serviceId=-1;
  char buffer[8];
  char* s = getToken(buffer, topic, topic_len, 5);
  if (s) {
    serviceId = atoi(s);
  }
  return serviceId;
}


void handle_system_mqtt_cmd(const char* topic, int topic_len, const char* payload)
{
  char action[16];
  getAction(action, topic, topic_len);
    if (strcmp(action, "restart") == 0) {
      system_restart();
      return;
    }
  ESP_LOGW(TAG, "unhandled system action: %s", action);
}

#if CONFIG_MQTT_THERMOSTATS_NB > 0
void handle_thermostat_mqtt_bump_cmd(const char *payload)
{
  struct ThermostatMessage tm;
  memset(&tm, 0, sizeof(struct ThermostatMessage));
  tm.msgType = THERMOSTAT_CMD_BUMP;
  tm.thermostatId = -1;
  if (xQueueSend( thermostatQueue
                  ,( void * )&tm
                  ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to thermostatQueue");
  }
}

void handle_thermostat_mqtt_mode_cmd(signed char thermostatId, const char *payload)
{

  struct ThermostatMessage tm;
  memset(&tm, 0, sizeof(struct ThermostatMessage));
  tm.msgType = THERMOSTAT_CMD_MODE;
  tm.thermostatId = thermostatId;
  if (strcmp(payload, "heat") == 0)
    tm.data.thermostatMode = THERMOSTAT_MODE_HEAT;
  else if (strcmp(payload, "off") == 0)
    tm.data.thermostatMode = THERMOSTAT_MODE_OFF;

  if (tm.data.thermostatMode == THERMOSTAT_MODE_UNSET) {
    ESP_LOGE(TAG, "wrong payload");
    return;
  }

  if (xQueueSend( thermostatQueue
                  ,( void * )&tm
                  ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to thermostatQueue");
  }
}

void handle_thermostat_mqtt_temp_cmd(signed char thermostatId, const char *payload)
{
  struct ThermostatMessage tm;
  memset(&tm, 0, sizeof(struct ThermostatMessage));
  tm.msgType = THERMOSTAT_CMD_TARGET_TEMPERATURE;
  tm.thermostatId = thermostatId;
  tm.data.targetTemperature = atof(payload) * 10;

  if (xQueueSend( thermostatQueue
                  ,( void * )&tm
                  ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to thermostatQueue");
  }
}

void handle_thermostat_mqtt_tolerance_cmd(signed char thermostatId, const char *payload)
{
  struct ThermostatMessage tm;
  memset(&tm, 0, sizeof(struct ThermostatMessage));
  tm.msgType = THERMOSTAT_CMD_TOLERANCE;
  tm.thermostatId = thermostatId;
  tm.data.tolerance = atof(payload) * 10;

  if (xQueueSend( thermostatQueue
                  ,( void * )&tm
                  ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to thermostatQueue");
  }
}

void handle_thermostat_mqtt_cmd(const char* topic, int topic_len, const char* payload)
{
  char action[16];
  getAction(action, topic, topic_len);
  if (strcmp(action, "bump") == 0) {
    handle_thermostat_mqtt_bump_cmd(payload);
    return;
  } else {
    signed char thermostatId = getServiceId(topic, topic_len);
    if (thermostatId != -1) {
      if (strcmp(action, "mode") == 0) {
        handle_thermostat_mqtt_mode_cmd(thermostatId, payload);
        return;
      }
      if (strcmp(action, "temp") == 0) {
        handle_thermostat_mqtt_temp_cmd(thermostatId, payload);
        return;
      }
      if (strcmp(action, "tolerance") == 0) {
        handle_thermostat_mqtt_tolerance_cmd(thermostatId, payload);
        return;
      }
    }
  }
  ESP_LOGW(TAG, "unhandled water thermostat: %s", action);
}

#endif // CONFIG_MQTT_THERMOSTATS_NB > 0

#if CONFIG_MQTT_RELAYS_NB

void handle_relay_mqtt_status_cmd(signed char relayId, const char *payload)
{
  struct RelayMessage rm;
  memset(&rm, 0, sizeof(struct RelayMessage));
  rm.msgType = RELAY_CMD_STATUS;
  rm.relayId = relayId;

  if (strcmp(payload, "ON") == 0)
    rm.data = RELAY_STATUS_ON;
  else if (strcmp(payload, "OFF") == 0)
    rm.data = RELAY_STATUS_OFF;

  if (xQueueSend( relayQueue
                  ,( void * )&rm
                  ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to relayQueue");
  }
}

void handle_relay_mqtt_sleep_cmd(signed char relayId, const char *payload)
{
  struct RelayMessage rm;
  memset(&rm, 0, sizeof(struct RelayMessage));
  rm.msgType = RELAY_CMD_SLEEP;
  rm.relayId = relayId;

  rm.data = atoi(payload);

  if (xQueueSend( relayQueue
                  ,( void * )&rm
                  ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to relayQueue");
  }
}


void handle_relay_mqtt_cmd(const char* topic, int topic_len, const char* payload)
{
  char action[16];
  getAction(action, topic, topic_len);

  signed char relayId = getServiceId(topic, topic_len);
  if (relayId != -1) {
    if (strcmp(action, "status") == 0) {
      handle_relay_mqtt_status_cmd(relayId, payload);
      return;
    }
    if (strcmp(action, "sleep") == 0) {
      handle_relay_mqtt_sleep_cmd(relayId, payload);
      return;
    }
    ESP_LOGW(TAG, "unhandled relay action: %s", action);
    return;
  }
  ESP_LOGW(TAG, "unhandled relay id: %d", relayId);
}

#endif // CONFIG_MQTT_RELAYS_NB

#if CONFIG_MQTT_THERMOSTATS_MQTT_SENSORS > 0
void thermostat_publish_data(int thermostat_id, const char * payload)
{
  struct ThermostatMessage tm;
  memset(&tm, 0, sizeof(struct ThermostatMessage));
  tm.msgType = THERMOSTAT_CURRENT_TEMPERATURE;
  tm.thermostatId = thermostat_id;
  tm.data.currentTemperature = atof(payload) * 10;

  if (xQueueSend( thermostatQueue
                  ,( void * )&tm
                  ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to thermostatQueue");
  }
}

bool handleThermostatMqttSensor(esp_mqtt_event_handle_t event)
{
  char payload[16];
  memcpy(payload, event->data, event->data_len);
  payload[event->data_len] = 0;

#ifdef CONFIG_MQTT_THERMOSTATS_NB0_SENSOR_TYPE_MQTT
  if (strncmp(event->topic, CONFIG_MQTT_THERMOSTATS_NB0_MQTT_SENSOR_TOPIC, strlen(CONFIG_MQTT_THERMOSTATS_NB0_MQTT_SENSOR_TOPIC)) == 0) {
    thermostat_publish_data(0, payload);
    return true;
  }
#endif //CONFIG_MQTT_THERMOSTATS_NB0_SENSOR_TYPE_MQTT

#ifdef CONFIG_MQTT_THERMOSTATS_NB1_SENSOR_TYPE_MQTT
  if (strncmp(event->topic, CONFIG_MQTT_THERMOSTATS_NB1_MQTT_SENSOR_TOPIC, strlen(CONFIG_MQTT_THERMOSTATS_NB1_MQTT_SENSOR_TOPIC)) == 0) {
    thermostat_publish_data(1, payload);
    return true;
  }
#endif //CONFIG_MQTT_THERMOSTATS_NB1_SENSOR_TYPE_MQTT

#ifdef CONFIG_MQTT_THERMOSTATS_NB2_SENSOR_TYPE_MQTT
  if (strncmp(event->topic, CONFIG_MQTT_THERMOSTATS_NB2_MQTT_SENSOR_TOPIC, strlen(CONFIG_MQTT_THERMOSTATS_NB2_MQTT_SENSOR_TOPIC)) == 0) {
    thermostat_publish_data(2, payload);
    return true;
  }
#endif //CONFIG_MQTT_THERMOSTATS_NB2_SENSOR_TYPE_MQTT

#ifdef CONFIG_MQTT_THERMOSTATS_NB3_SENSOR_TYPE_MQTT
  if (strncmp(event->topic, CONFIG_MQTT_THERMOSTATS_NB3_MQTT_SENSOR_TOPIC, strlen(CONFIG_MQTT_THERMOSTATS_NB3_MQTT_SENSOR_TOPIC)) == 0) {
    thermostat_publish_data(3, payload);
    return true;
  }
#endif //CONFIG_MQTT_THERMOSTATS_NB3_SENSOR_TYPE_MQTT

  return false;
}
#endif // CONFIG_MQTT_THERMOSTATS_MQTT_SENSORS > 0


void dispatch_mqtt_event(esp_mqtt_event_handle_t event)
{
  //FIXME this check should be generic and 16 should get a define
  if (event->data_len > 16 - 1) { //including '\0'
    ESP_LOGE(TAG, "payload to big");
    return;
  }
#if CONFIG_MQTT_THERMOSTATS_MQTT_SENSORS > 0
  if (handleThermostatMqttSensor(event)){
    return;
  }
#endif // CONFIG_MQTT_THERMOSTATS_MQTT_SENSORS > 0

  char actionType[16];
  getActionType(actionType, event->topic, event->topic_len);

  if (getActionType(actionType, event->topic, event->topic_len) && strcmp(actionType, "cmd") == 0) {
    char payload[16];

    memcpy(payload, event->data, event->data_len);
    payload[event->data_len] = 0;

    char service[16];
    getService(service, event->topic, event->topic_len);

    if (strcmp(service, "system") == 0) {
      handle_system_mqtt_cmd(event->topic, event->topic_len, payload);
      return;
    }

#if CONFIG_MQTT_THERMOSTATS_NB > 0
    if (strcmp(service, "thermostat") == 0) {
      handle_thermostat_mqtt_cmd(event->topic, event->topic_len, payload);
      return;
    }
#endif //CONFIG_MQTT_THERMOSTATS_NB > 0

#if CONFIG_MQTT_RELAYS_NB
    if (strcmp(service, "relay") == 0) {
      handle_relay_mqtt_cmd(event->topic, event->topic_len, payload);
      return;
    }
#endif // CONFIG_MQTT_RELAYS_NB
  ESP_LOGE(TAG, "Unhandled service %s for cmd action", service);
  }

  if (handle_scheduler_mqtt_event(event))
    return;
  if (handle_ota_mqtt_event(event))
    return;
  ESP_LOGI(TAG, "Unhandled topic %.*s", event->topic_len, event->topic);
}

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
  char* data = "online";
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
    dispatch_mqtt_event(event);
    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
    break;
  case MQTT_EVENT_BEFORE_CONNECT:
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

void mqtt_init_and_start()
{
  const char * lwtmsg = "offline";
  const esp_mqtt_client_config_t mqtt_cfg = {
    .uri = "mqtts://" CONFIG_MQTT_USERNAME ":" CONFIG_MQTT_PASSWORD "@" CONFIG_MQTT_SERVER ":" CONFIG_MQTT_PORT,
    .event_handle = mqtt_event_handler,
    .cert_pem = (const char *)cert_bundle_pem_start,
    .client_id = CONFIG_CLIENT_ID,
    .lwt_topic = available_topic,
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
        publish_available_msg();
        publish_config_msg();
#if CONFIG_MQTT_RELAYS_NB
        publish_all_relays_status();
        publish_all_relays_timeout();
#endif//CONFIG_MQTT_RELAYS_NB
#if CONFIG_MQTT_THERMOSTATS_NB > 0
        publish_thermostat_data();
#endif // CONFIG_MQTT_THERMOSTATS_NB > 0
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

