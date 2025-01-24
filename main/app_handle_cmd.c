#include "esp_system.h"
#if defined(CONFIG_NORTH_INTERFACE_UDP) || defined(CONFIG_NORTH_INTERFACE_MQTT)

#include <string.h>

#include "app_mqtt.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef CONFIG_MQTT_SCHEDULERS
#include "app_scheduler.h"
extern QueueHandle_t schedulerCfgQueue;
#endif  // CONFIG_MQTT_SCHEDULERS

#if CONFIG_MQTT_RELAYS_NB
#include "app_relay.h"
extern QueueHandle_t relayQueue;
#endif  // CONFIG_MQTT_RELAYS_NB

#ifdef CONFIG_MQTT_OTA
#include "app_ota.h"
extern QueueHandle_t otaQueue;
#define OTA_TOPIC CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/cmd/ota"
#endif  // CONFIG_MQTT_OTA

#if CONFIG_MQTT_THERMOSTATS_NB > 0
#include "app_thermostat.h"
extern QueueHandle_t thermostatQueue;
#endif  // CONFIG_MQTT_THERMOSTATS_NB > 0

#ifdef CONFIG_SOUTH_INTERFACE_UDP
#include "app_udp_common.h"
#endif  // CONFIG_SOUTH_INTERFACE_UDP

#include "app_system.h"

static const char* TAG = "CMD_HANDLING";

bool handle_ota_mqtt_event(char* topic) {
#ifdef CONFIG_MQTT_OTA
  if (strncmp(topic, OTA_TOPIC, strlen(OTA_TOPIC)) == 0) {
    struct OtaMessage o = {"https://sw.iot.cipex.ro:8911/" CONFIG_CLIENT_ID
                           ".bin"};
    if (xQueueSend(otaQueue, (void*)&o, OTA_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to otaQueue");
    }
    return true;
  }
#endif  // CONFIG_MQTT_OTA
  return false;
}

char* getToken(char* buffer, const char* topic, int topic_len,
               unsigned char place) {
  if (topic == NULL) return NULL;

  if (topic_len >= 64) return NULL;

  char str[64];
  memcpy(str, topic, topic_len);
  str[topic_len] = 0;

  char* token = strtok(str, "/");
  int i = 0;
  while (i < place && token) {
    token = strtok(NULL, "/");
    i += 1;
  }
  if (!token) return NULL;

  return strcpy(buffer, token);
}

char* getActionType(char* actionType, const char* topic, int topic_len) {
  return getToken(actionType, topic, topic_len, 2);
}
char* getAction(char* action, const char* topic, int topic_len) {
  return getToken(action, topic, topic_len, 3);
}

char* getService(char* service, const char* topic, int topic_len) {
  return getToken(service, topic, topic_len, 4);
}

signed char getServiceId(const char* topic, int topic_len) {
  signed char serviceId = -1;
  char buffer[8];
  char* s = getToken(buffer, topic, topic_len, 5);
  if (s) {
    serviceId = atoi(s);
  }
  return serviceId;
}

void handle_system_mqtt_cmd(const char* topic, int topic_len,
                            const char* payload) {
  char action[16];
  getAction(action, topic, topic_len);
  if (strcmp(action, "restart") == 0) {
    system_restart();
    return;
  }
  ESP_LOGW(TAG, "unhandled system action: %s", action);
}

#if CONFIG_MQTT_THERMOSTATS_NB > 0
void handle_thermostat_mqtt_bump_cmd(const char* payload) {
  struct ThermostatMessage tm;
  memset(&tm, 0, sizeof(struct ThermostatMessage));
  tm.msgType = THERMOSTAT_CMD_BUMP;
  tm.thermostatId = -1;
  if (xQueueSend(thermostatQueue, (void*)&tm, MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to thermostatQueue");
  }
}

void handle_thermostat_mqtt_mode_cmd(signed char thermostatId,
                                     const char* payload) {
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

  if (xQueueSend(thermostatQueue, (void*)&tm, MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to thermostatQueue");
  }
}

void handle_thermostat_mqtt_temp_cmd(signed char thermostatId,
                                     const char* payload) {
  struct ThermostatMessage tm;
  memset(&tm, 0, sizeof(struct ThermostatMessage));
  tm.msgType = THERMOSTAT_CMD_TARGET_TEMPERATURE;
  tm.thermostatId = thermostatId;
  tm.data.targetTemperature = atof(payload) * 10;

  if (xQueueSend(thermostatQueue, (void*)&tm, MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to thermostatQueue");
  }
}

void handle_thermostat_mqtt_tolerance_cmd(signed char thermostatId,
                                          const char* payload) {
  struct ThermostatMessage tm;
  memset(&tm, 0, sizeof(struct ThermostatMessage));
  tm.msgType = THERMOSTAT_CMD_TOLERANCE;
  tm.thermostatId = thermostatId;
  tm.data.tolerance = atof(payload) * 10;

  if (xQueueSend(thermostatQueue, (void*)&tm, MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to thermostatQueue");
  }
}

void handle_thermostat_mqtt_cmd(const char* topic, int topic_len,
                                const char* payload) {
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

#endif  // CONFIG_MQTT_THERMOSTATS_NB > 0

#if CONFIG_MQTT_RELAYS_NB

void handle_relay_mqtt_status_cmd(signed char relayId, const char* payload) {
  struct RelayMessage rm;
  memset(&rm, 0, sizeof(struct RelayMessage));
  rm.msgType = RELAY_CMD_STATUS;
  rm.relayId = relayId;

  if (strcmp(payload, "ON") == 0)
    rm.data = RELAY_STATUS_ON;
  else if (strcmp(payload, "OFF") == 0)
    rm.data = RELAY_STATUS_OFF;

  if (xQueueSend(relayQueue, (void*)&rm, MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to relayQueue");
  }
}

void handle_relay_mqtt_sleep_cmd(signed char relayId, const char* payload) {
  struct RelayMessage rm;
  memset(&rm, 0, sizeof(struct RelayMessage));
  rm.msgType = RELAY_CMD_SLEEP;
  rm.relayId = relayId;

  rm.data = atoi(payload);

  if (xQueueSend(relayQueue, (void*)&rm, MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to relayQueue");
  }
}

void handle_relay_mqtt_cmd(const char* topic, int topic_len,
                           const char* payload) {
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

#endif  // CONFIG_MQTT_RELAYS_NB

#ifdef CONFIG_MQTT_SCHEDULERS
void handle_scheduler_mqtt_action_cmd(signed char schedulerId,
                                      const char* payload) {
  struct SchedulerCfgMessage sm;
  memset(&sm, 0, sizeof(struct SchedulerCfgMessage));
  sm.msgType = SCHEDULER_CMD_ACTION;
  sm.schedulerId = schedulerId;

  if (strcmp(payload, "relay_on") == 0)
    sm.data.action = SCHEDULER_ACTION_RELAY_ON;
  else if (strcmp(payload, "relay_off") == 0)
    sm.data.action = SCHEDULER_ACTION_RELAY_OFF;
  else if (strcmp(payload, "water_temp_low") == 0)
    sm.data.action = SCHEDULER_ACTION_WATER_TEMP_LOW;
  else if (strcmp(payload, "water_temp_high") == 0)
    sm.data.action = SCHEDULER_ACTION_WATER_TEMP_HIGH;
  else if (strcmp(payload, "ow_on") == 0)
    sm.data.action = SCHEDULER_ACTION_OW_ON;
  else if (strcmp(payload, "ow_off") == 0)
    sm.data.action = SCHEDULER_ACTION_OW_OFF;
  else {
    ESP_LOGW(TAG, "unhandled scheduler action: %s", payload);
    return;
  }
  if (xQueueSend(schedulerCfgQueue, (void*)&sm, MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to schedulerCfgQueue");
  }
}

void handle_scheduler_mqtt_dow_cmd(signed char schedulerId,
                                   const char* payload) {
  struct SchedulerCfgMessage sm;
  memset(&sm, 0, sizeof(struct SchedulerCfgMessage));
  sm.msgType = SCHEDULER_CMD_DOW;
  sm.schedulerId = schedulerId;
  sm.data.dow = atoi(payload);

  if (xQueueSend(schedulerCfgQueue, (void*)&sm, MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to schedulerCfgQueue");
  }
}

void handle_scheduler_mqtt_time_cmd(signed char schedulerId,
                                    const char* payload) {
  struct SchedulerCfgMessage sm;
  memset(&sm, 0, sizeof(struct SchedulerCfgMessage));
  sm.msgType = SCHEDULER_CMD_TIME;
  sm.schedulerId = schedulerId;

  char str[16];
  strncpy(str, payload, 15);
  str[15] = '\0';

  char* token = strtok(str, ":");
  if (!token) {
    ESP_LOGW(TAG, "unhandled scheduler time token 1: %s", payload);
    return;
  }

  sm.data.time.hour = atoi(token);

  token = strtok(NULL, ":");
  if (!token) {
    ESP_LOGW(TAG, "unhandled scheduler time token 2: %s", payload);
    return;
  }
  sm.data.time.minute = atoi(token);

  if (xQueueSend(schedulerCfgQueue, (void*)&sm, MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to schedulerCfgQueue");
  }
}

void handle_scheduler_mqtt_status_cmd(signed char schedulerId,
                                      const char* payload) {
  struct SchedulerCfgMessage sm;
  memset(&sm, 0, sizeof(struct SchedulerCfgMessage));
  sm.msgType = SCHEDULER_CMD_STATUS;
  sm.schedulerId = schedulerId;

  if (strcmp(payload, "ON") == 0)
    sm.data.status = SCHEDULER_STATUS_ON;
  else if (strcmp(payload, "OFF") == 0)
    sm.data.status = SCHEDULER_STATUS_OFF;
  else {
    ESP_LOGW(TAG, "unhandled scheduler status: %s", payload);
    return;
  }

  if (xQueueSend(schedulerCfgQueue, (void*)&sm, MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to schedulerCfgQueue");
  }
}

void handle_scheduler_mqtt_cmd(const char* topic, int topic_len,
                               const char* payload) {
  char action[16];
  getAction(action, topic, topic_len);

  signed char schedulerId = getServiceId(topic, topic_len);
  if (schedulerId != -1) {
    if (strcmp(action, "action") == 0) {
      handle_scheduler_mqtt_action_cmd(schedulerId, payload);
      return;
    }
    if (strcmp(action, "dow") == 0) {
      handle_scheduler_mqtt_dow_cmd(schedulerId, payload);
      return;
    }
    if (strcmp(action, "time") == 0) {
      handle_scheduler_mqtt_time_cmd(schedulerId, payload);
      return;
    }
    if (strcmp(action, "status") == 0) {
      handle_scheduler_mqtt_status_cmd(schedulerId, payload);
      return;
    }
    ESP_LOGW(TAG, "unhandled relay action: %s", action);
    return;
  }
  ESP_LOGW(TAG, "unhandled relay id: %d", schedulerId);
}

#endif  // CONFIG_MQTT_SCHEDULERS

#ifdef CONFIG_SOUTH_INTERFACE_UDP
bool handleSouthInterface(char* topic, int topic_len, char* data,
                          int data_len) {
  if (topic_len >= 64) return NULL;

  char topic_str[64];
  memcpy(topic_str, topic, topic_len);
  topic_str[topic_len] = 0;

  char* device_type = strtok(topic_str, "/");
  char* client_id = strtok(NULL, "/");
  if (strcmp(CONFIG_SOUTH_INTERFACE_UDP_DEVICE_TYPE, device_type) == 0 &&
      strcmp(CONFIG_SOUTH_INTERFACE_UDP_CLIENT_ID, client_id) == 0) {
    if (data_len >= 64) return false;
    char data_str[16];
    memcpy(data_str, data, data_len);
    data_str[data_len] = 0;

    memcpy(topic_str, topic, topic_len);
    topic_str[topic_len] = 0;

    return udp_publish_data(topic_str, data_str, QOS_0, NO_RETAIN);
  }
  return false;
}
#endif  // CONFIG_SOUTH_INTERFACE_UDP

#if CONFIG_MQTT_THERMOSTATS_MQTT_SENSORS > 0
void thermostat_publish_data(int thermostat_id, const char* payload) {
  struct ThermostatMessage tm;
  memset(&tm, 0, sizeof(struct ThermostatMessage));
  tm.msgType = THERMOSTAT_CURRENT_TEMPERATURE;
  tm.thermostatId = thermostat_id;
  tm.data.currentTemperature = atof(payload) * 10;

  if (xQueueSend(thermostatQueue, (void*)&tm, MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to thermostatQueue");
  }
}

bool handleThermostatMqttSensor(char* topic, int topic_len, char* data,
                                int data_len) {
  char payload[16];
  memcpy(payload, data, data_len);
  payload[data_len] = 0;

#ifdef CONFIG_MQTT_THERMOSTATS_NB0_SENSOR_TYPE_MQTT
  if (strncmp(topic, CONFIG_MQTT_THERMOSTATS_NB0_MQTT_SENSOR_TOPIC,
              strlen(CONFIG_MQTT_THERMOSTATS_NB0_MQTT_SENSOR_TOPIC)) == 0) {
    thermostat_publish_data(0, payload);
    return true;
  }
#endif  // CONFIG_MQTT_THERMOSTATS_NB0_SENSOR_TYPE_MQTT

#ifdef CONFIG_MQTT_THERMOSTATS_NB1_SENSOR_TYPE_MQTT
  if (strncmp(topic, CONFIG_MQTT_THERMOSTATS_NB1_MQTT_SENSOR_TOPIC,
              strlen(CONFIG_MQTT_THERMOSTATS_NB1_MQTT_SENSOR_TOPIC)) == 0) {
    thermostat_publish_data(1, payload);
    return true;
  }
#endif  // CONFIG_MQTT_THERMOSTATS_NB1_SENSOR_TYPE_MQTT

#ifdef CONFIG_MQTT_THERMOSTATS_NB2_SENSOR_TYPE_MQTT
  if (strncmp(topic, CONFIG_MQTT_THERMOSTATS_NB2_MQTT_SENSOR_TOPIC,
              strlen(CONFIG_MQTT_THERMOSTATS_NB2_MQTT_SENSOR_TOPIC)) == 0) {
    thermostat_publish_data(2, payload);
    return true;
  }
#endif  // CONFIG_MQTT_THERMOSTATS_NB2_SENSOR_TYPE_MQTT

#ifdef CONFIG_MQTT_THERMOSTATS_NB3_SENSOR_TYPE_MQTT
  if (strncmp(topic, CONFIG_MQTT_THERMOSTATS_NB3_MQTT_SENSOR_TOPIC,
              strlen(CONFIG_MQTT_THERMOSTATS_NB3_MQTT_SENSOR_TOPIC)) == 0) {
    thermostat_publish_data(3, payload);
    return true;
  }
#endif  // CONFIG_MQTT_THERMOSTATS_NB3_SENSOR_TYPE_MQTT

#ifdef CONFIG_MQTT_THERMOSTATS_NB4_SENSOR_TYPE_MQTT
  if (strncmp(topic, CONFIG_MQTT_THERMOSTATS_NB4_MQTT_SENSOR_TOPIC,
              strlen(CONFIG_MQTT_THERMOSTATS_NB4_MQTT_SENSOR_TOPIC)) == 0) {
    thermostat_publish_data(4, payload);
    return true;
  }
#endif  // CONFIG_MQTT_THERMOSTATS_NB4_SENSOR_TYPE_MQTT

#ifdef CONFIG_MQTT_THERMOSTATS_NB5_SENSOR_TYPE_MQTT
  if (strncmp(topic, CONFIG_MQTT_THERMOSTATS_NB5_MQTT_SENSOR_TOPIC,
              strlen(CONFIG_MQTT_THERMOSTATS_NB5_MQTT_SENSOR_TOPIC)) == 0) {
    thermostat_publish_data(5, payload);
    return true;
  }
#endif  // CONFIG_MQTT_THERMOSTATS_NB5_SENSOR_TYPE_MQTT

  return false;
}
#endif  // CONFIG_MQTT_THERMOSTATS_MQTT_SENSORS > 0

void handle_cmd_topic(char* topic, int topic_len, char* data, int data_len) {
  // FIXME this check should be generic and 16 should get a define
  if (data_len > 16 - 1) {  // including '\0'
    ESP_LOGE(TAG, "payload to big");
    return;
  }

#ifdef CONFIG_SOUTH_INTERFACE_UDP
  if (handleSouthInterface(topic, topic_len, data, data_len)) {
    return;
  }
#endif  // CONFIG_SOUTH_INTERFACE_UDP

#if CONFIG_MQTT_THERMOSTATS_MQTT_SENSORS > 0
  if (handleThermostatMqttSensor(topic, topic_len, data, data_len)) {
    return;
  }
#endif  // CONFIG_MQTT_THERMOSTATS_MQTT_SENSORS > 0

  char actionType[16];
  getActionType(actionType, topic, topic_len);

  if (getActionType(actionType, topic, topic_len) &&
      strcmp(actionType, "cmd") == 0) {
    char payload[16];

    memcpy(payload, data, data_len);
    payload[data_len] = 0;

    char service[16];
    getService(service, topic, topic_len);

    if (strcmp(service, "system") == 0) {
      handle_system_mqtt_cmd(topic, topic_len, payload);
      return;
    }

#if CONFIG_MQTT_THERMOSTATS_NB > 0
    if (strcmp(service, "thermostat") == 0) {
      handle_thermostat_mqtt_cmd(topic, topic_len, payload);
      return;
    }
#endif  // CONFIG_MQTT_THERMOSTATS_NB > 0

#if CONFIG_MQTT_RELAYS_NB
    if (strcmp(service, "relay") == 0) {
      handle_relay_mqtt_cmd(topic, topic_len, payload);
      return;
    }
#endif  // CONFIG_MQTT_RELAYS_NB

#ifdef CONFIG_MQTT_SCHEDULERS
    if (strcmp(service, "scheduler") == 0) {
      handle_scheduler_mqtt_cmd(topic, topic_len, payload);
      return;
    }
#endif  // CONFIG_MQTT_SCHEDULERS
    ESP_LOGE(TAG, "Unhandled service %s for cmd action", service);
  }

  if (handle_ota_mqtt_event(topic)) return;
  ESP_LOGI(TAG, "Unhandled topic %.*s", topic_len, topic);
}

#endif  // defined(CONFIG_NORTH_INTERFACE_UDP) ||
        // defined(CONFIG_NORTH_INTERFACE_MQTT)
