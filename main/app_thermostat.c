#include "esp_system.h"

#if CONFIG_MQTT_THERMOSTATS_NB > 0

#include <limits.h>
#include <string.h>
#include <stdlib.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "app_main.h"
#include "app_relay.h"
#include "app_thermostat.h"
#include "app_nvs.h"
#include "app_mqtt.h"

enum ThermostatState thermostatState = THERMOSTAT_STATE_IDLE;
unsigned int thermostatDuration = 0;

enum HeatingState heatingState = HEATING_STATE_IDLE;
unsigned int heatingDuration = 0;
int circuitThermostatId = -1;


enum ThermostatMode thermostatMode[CONFIG_MQTT_THERMOSTATS_NB] = {
  THERMOSTAT_MODE_UNSET,
#if CONFIG_MQTT_THERMOSTATS_NB > 1
  THERMOSTAT_MODE_UNSET,
#if CONFIG_MQTT_THERMOSTATS_NB > 2
  THERMOSTAT_MODE_UNSET,
#if CONFIG_MQTT_THERMOSTATS_NB > 3
  THERMOSTAT_MODE_UNSET,
#endif //CONFIG_MQTT_THERMOSTATS_NB > 3
#endif //CONFIG_MQTT_THERMOSTATS_NB > 2
#endif //CONFIG_MQTT_THERMOSTATS_NB > 1
};

const char * thermostatModeTAG[CONFIG_MQTT_THERMOSTATS_NB] = {
  "thermMode0",
#if CONFIG_MQTT_THERMOSTATS_NB > 1
  "thermMode1",
#if CONFIG_MQTT_THERMOSTATS_NB > 2
  "thermMode2",
#if CONFIG_MQTT_THERMOSTATS_NB > 3
  "thermMode3",
#endif //CONFIG_MQTT_THERMOSTATS_NB > 3
#endif //CONFIG_MQTT_THERMOSTATS_NB > 2
#endif //CONFIG_MQTT_THERMOSTATS_NB > 1
};

short targetTemperature[CONFIG_MQTT_THERMOSTATS_NB] = {
  21*10,
#if CONFIG_MQTT_THERMOSTATS_NB > 1
  21*10,
#if CONFIG_MQTT_THERMOSTATS_NB > 2
  21*10,
#if CONFIG_MQTT_THERMOSTATS_NB > 3
  21*10,
#endif //CONFIG_MQTT_THERMOSTATS_NB > 3
#endif //CONFIG_MQTT_THERMOSTATS_NB > 2
#endif //CONFIG_MQTT_THERMOSTATS_NB > 1
};

const char * targetTemperatureTAG[CONFIG_MQTT_THERMOSTATS_NB] = {
  "targetTemp0",
#if CONFIG_MQTT_THERMOSTATS_NB > 1
  "targetTemp1",
#if CONFIG_MQTT_THERMOSTATS_NB > 2
  "targetTemp2",
#if CONFIG_MQTT_THERMOSTATS_NB > 3
  "targetTemp3",
#endif //CONFIG_MQTT_THERMOSTATS_NB > 3
#endif //CONFIG_MQTT_THERMOSTATS_NB > 2
#endif //CONFIG_MQTT_THERMOSTATS_NB > 1
};

short temperatureTolerance[CONFIG_MQTT_THERMOSTATS_NB] = {
  5,
#if CONFIG_MQTT_THERMOSTATS_NB > 1
  5,
#if CONFIG_MQTT_THERMOSTATS_NB > 2
  5,
#if CONFIG_MQTT_THERMOSTATS_NB > 3
  5,
#endif //CONFIG_MQTT_THERMOSTATS_NB > 3
#endif //CONFIG_MQTT_THERMOSTATS_NB > 2
#endif //CONFIG_MQTT_THERMOSTATS_NB > 1
}; //0.5

const char * temperatureToleranceTAG[CONFIG_MQTT_THERMOSTATS_NB] = {
  "tempToler0",
#if CONFIG_MQTT_THERMOSTATS_NB > 1
  "tempToler1",
#if CONFIG_MQTT_THERMOSTATS_NB > 2
  "tempToler2",
#if CONFIG_MQTT_THERMOSTATS_NB > 3
  "tempToler3",
#endif //CONFIG_MQTT_THERMOSTATS_NB > 3
#endif //CONFIG_MQTT_THERMOSTATS_NB > 2
#endif //CONFIG_MQTT_THERMOSTATS_NB > 1
};

enum ThermostatType thermostatType[CONFIG_MQTT_THERMOSTATS_NB] = {
  THERMOSTAT_TYPE_NORMAL,
#if CONFIG_MQTT_THERMOSTATS_NB > 1
  THERMOSTAT_TYPE_NORMAL,
#if CONFIG_MQTT_THERMOSTATS_NB > 2
  THERMOSTAT_TYPE_NORMAL,
#if CONFIG_MQTT_THERMOSTATS_NB > 3
  THERMOSTAT_TYPE_NORMAL,
#endif //CONFIG_MQTT_THERMOSTATS_NB > 3
#endif //CONFIG_MQTT_THERMOSTATS_NB > 2
#endif //CONFIG_MQTT_THERMOSTATS_NB > 1
};

short currentTemperature[CONFIG_MQTT_THERMOSTATS_NB] = {
  SHRT_MIN,
#if CONFIG_MQTT_THERMOSTATS_NB > 1
  SHRT_MIN,
#if CONFIG_MQTT_THERMOSTATS_NB > 2
  SHRT_MIN,
#if CONFIG_MQTT_THERMOSTATS_NB > 3
  SHRT_MIN,
#endif //CONFIG_MQTT_THERMOSTATS_NB > 3
#endif //CONFIG_MQTT_THERMOSTATS_NB > 2
#endif //CONFIG_MQTT_THERMOSTATS_NB > 1
};

const char* thermostatFriendlyName[CONFIG_MQTT_THERMOSTATS_NB] = {
  CONFIG_MQTT_THERMOSTATS_NB0_FRIENDLY_NAME,
#if CONFIG_MQTT_THERMOSTATS_NB > 1
  CONFIG_MQTT_THERMOSTATS_NB1_FRIENDLY_NAME,
#if CONFIG_MQTT_THERMOSTATS_NB > 2
  CONFIG_MQTT_THERMOSTATS_NB2_FRIENDLY_NAME,
#if CONFIG_MQTT_THERMOSTATS_NB > 3
  CONFIG_MQTT_THERMOSTATS_NB3_FRIENDLY_NAME,
#endif //CONFIG_MQTT_THERMOSTATS_NB > 3
#endif //CONFIG_MQTT_THERMOSTATS_NB > 2
#endif //CONFIG_MQTT_THERMOSTATS_NB > 1
};


short currentTemperatureFlag[CONFIG_MQTT_THERMOSTATS_NB] = {0};

short currentTemperature_1 = SHRT_MIN;
short currentTemperature_2 = SHRT_MIN;
short currentTemperature_3 = SHRT_MIN;

extern QueueHandle_t thermostatQueue;

static const char *TAG = "APP_THERMOSTAT";

void thermostat_publish_local_data(int thermostat_id, int value)
{
  struct ThermostatMessage tm;
  memset(&tm, 0, sizeof(struct ThermostatMessage));
  tm.msgType = THERMOSTAT_CURRENT_TEMPERATURE;
  tm.thermostatId = thermostat_id;
  tm.data.currentTemperature = value;

  if (xQueueSend( thermostatQueue
                  ,( void * )&tm
                  ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to thermostatQueue");
  }
}

void publish_thermostat_current_temperature_evt(int id)
{
  if (currentTemperature[id] == SHRT_MIN)
    return;

  const char * thermostat_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/ctemp/thermostat";

  char data[16];
  memset(data,0,16);
  sprintf(data, "%d.%d",
          currentTemperature[id] > 0 ? currentTemperature[id] / 10 : 0,
          currentTemperature[id] > 0 ? abs(currentTemperature[id] % 10) : 0);

  char topic[MQTT_MAX_TOPIC_LEN];
  memset(topic,0,MQTT_MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", thermostat_topic, id);

  mqtt_publish_data(topic, data, QOS_1, RETAIN);
}

void publish_all_thermostats_current_temperature_evt()
{
  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    publish_thermostat_current_temperature_evt(id);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void publish_thermostat_target_temperature_evt(int id)
{
  const char * thermostat_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/temp/thermostat";

  char data[16];
  memset(data,0,16);
  sprintf(data, "%d.%d", targetTemperature[id] / 10, abs(targetTemperature[id] % 10));

  char topic[MQTT_MAX_TOPIC_LEN];
  memset(topic,0,MQTT_MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", thermostat_topic, id);

  mqtt_publish_data(topic, data, QOS_1, RETAIN);
}

void publish_all_thermostats_target_temperature_evt()
{
  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    publish_thermostat_target_temperature_evt(id);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void publish_thermostat_temperature_tolerance_evt(int id)
{
  const char * thermostat_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/tolerance/thermostat";

  char data[16];
  memset(data,0,16);
  sprintf(data, "%d.%d", temperatureTolerance[id] / 10, abs(temperatureTolerance[id] % 10));

  char topic[MQTT_MAX_TOPIC_LEN];
  memset(topic,0,MQTT_MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", thermostat_topic, id);

  mqtt_publish_data(topic, data, QOS_1, RETAIN);
}

void publish_all_thermostats_temperature_tolerance_evt()
{
  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    publish_thermostat_temperature_tolerance_evt(id);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void publish_thermostat_mode_evt(int id)
{
  const char * thermostat_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/mode/thermostat";

  char data[16];
  memset(data,0,16);
  sprintf(data, "%s", thermostatMode[id] == THERMOSTAT_MODE_HEAT ? "heat" : "off");

  char topic[MQTT_MAX_TOPIC_LEN];
  memset(topic,0,MQTT_MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", thermostat_topic, id);

  mqtt_publish_data(topic, data, QOS_1, RETAIN);
}

void publish_all_thermostats_mode_evt()
{
  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    publish_thermostat_mode_evt(id);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void get_normal_thermostat_action(char * data, int id)
{
  if (thermostatMode[id] == THERMOSTAT_MODE_HEAT) {
    switch(thermostatState) {
    case THERMOSTAT_STATE_IDLE:
      sprintf(data, "idle");
      break;
    case THERMOSTAT_STATE_HEATING:
      sprintf(data, "heating");
      break;
    default:
      ESP_LOGE(TAG, "bad heating state");
    }
  } else {
    sprintf(data, "off");
  }
}
void get_circuit_thermostat_action(char * data, int id)
{
    if (thermostatMode[id] == THERMOSTAT_MODE_HEAT) {
    switch(heatingState) {
    case HEATING_STATE_IDLE:
      sprintf(data, "idle");
      break;
    case HEATING_STATE_ENABLED:
      sprintf(data, "heating");
      break;
    default:
      ESP_LOGE(TAG, "bad heating state");
    }
  } else {
    sprintf(data, "off");
  }
}

void publish_thermostat_action_evt(int id)
{
  const char * thermostat_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/action/thermostat";

  char data[16];
  memset(data,0,16);
  if (thermostatType[id] == THERMOSTAT_TYPE_NORMAL) {
    get_normal_thermostat_action(data, id);
  } else {
    get_circuit_thermostat_action(data, id);
  }

  char topic[MQTT_MAX_TOPIC_LEN];
  memset(topic,0,MQTT_MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", thermostat_topic, id);

  mqtt_publish_data(topic, data, QOS_1, RETAIN);
}

void publish_all_normal_thermostats_action_evt()
{
  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    if (thermostatType[id] == THERMOSTAT_TYPE_NORMAL) {
      publish_thermostat_action_evt(id);
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }
  }
}

void publish_all_circuit_thermostats_action_evt()
{
  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    if (thermostatType[id] == THERMOSTAT_TYPE_CIRCUIT) {
      publish_thermostat_action_evt(id);
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }
  }
}

void publish_all_thermostats_action_evt()
{
  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    publish_thermostat_action_evt(id);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void publish_thermostat_data()
{
  publish_all_thermostats_current_temperature_evt();
  publish_all_thermostats_target_temperature_evt();
  publish_all_thermostats_temperature_tolerance_evt();
  publish_all_thermostats_mode_evt();
  publish_all_thermostats_action_evt();
}

#if CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS
void publish_thermostat_notification_evt(const char* msg)
{
  const char * topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/notification/thermostat";
  mqtt_publish_data(topic, msg, QOS_0, NO_RETAIN);
}

void publish_normal_thermostat_notification(enum ThermostatState state,
                                            unsigned int duration,
                                            const char *reason)
{
  char data[256];
  memset(data,0,256);

  sprintf(data, "Thermostat changed to %s due to %s. It was %s for %u minutes",
          state == THERMOSTAT_STATE_HEATING ? "on"  : "off", reason,
          state == THERMOSTAT_STATE_HEATING ? "off" : "on", duration);

  publish_thermostat_notification_evt(data);
}
#endif // CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS

void disableThermostat(const char * reason)
{
  thermostatState=THERMOSTAT_STATE_IDLE;
  update_relay_status(CONFIG_MQTT_THERMOSTAT_RELAY_ID, RELAY_STATUS_OFF);

  publish_all_normal_thermostats_action_evt();
#if CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS
  publish_normal_thermostat_notification(thermostatState, thermostatDuration, reason);
#endif // CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS

  thermostatDuration = 0;
  ESP_LOGI(TAG, "thermostat disabled");
}

void enableThermostat(const char * reason)
{
  thermostatState=THERMOSTAT_STATE_HEATING;
  update_relay_status(CONFIG_MQTT_THERMOSTAT_RELAY_ID, RELAY_STATUS_ON);

  publish_all_normal_thermostats_action_evt();
#if CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS
  publish_normal_thermostat_notification(thermostatState, thermostatDuration, reason);
#endif // CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS

  thermostatDuration = 0;
  ESP_LOGI(TAG, "thermostat enabled");
}

#if CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS
void publish_circuit_thermostat_notification(enum HeatingState state,
                                             unsigned int duration)
{
  char data[256];
  memset(data,0,256);

  sprintf(data, "Heating state changed to %s. It was %s for %u minutes",
          state == HEATING_STATE_ENABLED ? "on"  : "off",
          state == HEATING_STATE_ENABLED ? "off" : "on", duration);

  publish_thermostat_notification_evt(data);
}
#endif // CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS

void enableHeating()
{
  heatingState = HEATING_STATE_ENABLED;

  publish_all_circuit_thermostats_action_evt();
#if CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS
  publish_circuit_thermostat_notification(heatingState, heatingDuration);
#endif // CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS

  heatingDuration = 0;
  ESP_LOGI(TAG, "heating enabled");
}

void disableHeating()
{
  heatingState = HEATING_STATE_IDLE;

  publish_all_circuit_thermostats_action_evt();
#if CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS
  publish_circuit_thermostat_notification(heatingState, heatingDuration);
#endif // CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS

  heatingDuration = 0;
  ESP_LOGI(TAG, "heating2 disabled");
}

void dump_data()
{
  ESP_LOGI(TAG, "thermostat state is %d", thermostatState);
  ESP_LOGI(TAG, "heating state is %d", heatingState);
  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    ESP_LOGI(TAG, "thermostatMode[%d] is %d", id, thermostatMode[id]);
    ESP_LOGI(TAG, "currentTemperature[%d] is %d", id, currentTemperature[id]);
    ESP_LOGI(TAG, "currentTemperatureFlag[%d] is %d", id, currentTemperatureFlag[id]);
    ESP_LOGI(TAG, "targetTemperature[%d] is %d", id, targetTemperature[id]);
    if (thermostatType[id] == THERMOSTAT_TYPE_NORMAL) {
      ESP_LOGI(TAG, "temperatureTolerance[%d] is %d", id, temperatureTolerance[id]);
    }
    if (thermostatType[id] == THERMOSTAT_TYPE_CIRCUIT) {
      ESP_LOGI(TAG, "currentTemperature_1[%d] is %d", id, currentTemperature_1);
      ESP_LOGI(TAG, "currentTemperature_2[%d] is %d", id, currentTemperature_2);
      ESP_LOGI(TAG, "currentTemperature_3[%d] is %d", id, currentTemperature_3);
    }
  }
}

bool sensor_reporting()
{
  bool sensorReporting = false;
  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    if (thermostatType[id] == THERMOSTAT_TYPE_NORMAL && currentTemperatureFlag[id] != 0) {
      sensorReporting = true;
      break;
    }
  }
  return sensorReporting;
}

bool heating()
{
  if (circuitThermostatId == -1)
    return false;
  bool heating = false;
  if ((currentTemperatureFlag[circuitThermostatId] > 0) && thermostatMode[circuitThermostatId] == THERMOSTAT_MODE_HEAT) {
    if (currentTemperature_3 < currentTemperature_2 &&
        currentTemperature_2 < currentTemperature_1 &&
        currentTemperature_1 < currentTemperature[circuitThermostatId]) {
      heating = true;
    }
  }
  return heating;
}

bool not_heating()
{
  if (circuitThermostatId == -1)
    return false;
  bool not_heating = false;
  if ((currentTemperatureFlag[circuitThermostatId] > 0) && thermostatMode[circuitThermostatId] == THERMOSTAT_MODE_HEAT) {
    if (currentTemperature_3 >= currentTemperature_2 &&
        currentTemperature_2 >= currentTemperature_1 &&
        currentTemperature_1 >= currentTemperature[circuitThermostatId]) {
      not_heating = true;
    }

    //>decrease > 0.5 degree C means no longer heating but pump is working
    if (currentTemperature_1 - currentTemperature[circuitThermostatId] > 5) {
      not_heating = true;
    }
  }
  return not_heating;
}

bool circuitColdEnough()
{
  ESP_LOGI(TAG, "checking circuit %d cold enough", circuitThermostatId);
  if (circuitThermostatId == -1)
    return true;
  if ((currentTemperatureFlag[circuitThermostatId] > 0) && thermostatMode[circuitThermostatId] == THERMOSTAT_MODE_HEAT)
    return  (currentTemperature[circuitThermostatId] <= targetTemperature[circuitThermostatId]);
  else
    return true;
}

bool tooHot(char* reason)
{
  bool tooHot = true;
  char tstr[64];
  bool reasonUpdated = false;

  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    if ((currentTemperatureFlag[id] > 0) && thermostatMode[id] == THERMOSTAT_MODE_HEAT) {
      if (thermostatType[id] == THERMOSTAT_TYPE_NORMAL) {
        if (currentTemperature[id] > (targetTemperature[id] + temperatureTolerance[id])) {
          ESP_LOGI(TAG, "thermostat[%d] is hot enough", id);
          sprintf(tstr, "%s thermostat is hot enough, ", thermostatFriendlyName[id]);
          strcat(reason, tstr);
          reasonUpdated = true;
        } else {
          tooHot = false;
          ESP_LOGI(TAG, "thermostat[%d] is not too hot", id);
          break;
        }
      }
    }
  }
  if (tooHot && strlen(reason) == 0) {
    strcat(reason, "No normal thermostat is enabled");
  } else if (reasonUpdated) {
    reason[strlen(reason)-2] = 0;
  }
  return tooHot;
}

bool tooCold(char* reason)
{
  bool tooCold = false;
  char tstr[64];
  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    if ((currentTemperatureFlag[id] > 0) && thermostatMode[id] == THERMOSTAT_MODE_HEAT) {
      if (thermostatType[id] == THERMOSTAT_TYPE_NORMAL) {
        if (currentTemperature[id] < (targetTemperature[id] - temperatureTolerance[id])) {
          ESP_LOGI(TAG, "thermostat[%d] is too cold", id);
          sprintf(tstr, "%s thermostat is too cold, ", thermostatFriendlyName[id]);
          strcat(reason, tstr);
          tooCold = true;
          break;
        } else {
          ESP_LOGI(TAG, "thermostat[%d] is good", id);
        }
      }
    }
  }
  if (tooCold) {
    reason[strlen(reason)-2] = 0;
  }
  return tooCold;
}

void update_thermostat()
{
  dump_data();

  if (!sensor_reporting()) {
    ESP_LOGI(TAG, "no live sensor is reporting => no thermostat handling");
    if (thermostatState==THERMOSTAT_STATE_HEATING) {
      ESP_LOGI(TAG, "stop thermostat as no live sensor is reporting");
      disableThermostat("No live sensor is reporting");
    }
    return;
  }

  bool heatingToggledOff = false;

  if ((heatingState == HEATING_STATE_IDLE) && heating()) {
    enableHeating();
  } else if ((heatingState == HEATING_STATE_ENABLED) && not_heating()) {
    disableHeating();
    heatingToggledOff = true;
  }

  if (thermostatState == THERMOSTAT_STATE_HEATING &&
      heatingToggledOff) {
    ESP_LOGI(TAG, "reason: Heating is toggled off");
    disableThermostat("Heating is toggled off");
  }


  char reason[256];
  memset(reason,0,256);

  if (thermostatState == THERMOSTAT_STATE_HEATING) {
    if (tooHot(reason)) {
      ESP_LOGI(TAG, "Turning thermostat off, reason: %s", reason);
      disableThermostat(reason);
    }
  } else if (circuitColdEnough()) {
    if (tooCold(reason)) {
      ESP_LOGI(TAG, "Turning thermostat on, reason: %s", reason);
      enableThermostat(reason);
    }
  }
}

void vThermostatTimerCallback( TimerHandle_t xTimer )
{
  ESP_LOGI(TAG, "Thermostat timer expired");
  struct ThermostatMessage t;
  t.msgType = THERMOSTAT_LIFE_TICK;
  if (xQueueSend( thermostatQueue,
                  ( void * )&t,
                  MQTT_QUEUE_TIMEOUT) != pdPASS) {
  }
}

void handle_thermostat_cmd_task(void* pvParameters)
{
  //init remaining variables
#ifdef CONFIG_MQTT_THERMOSTATS_NB0_TYPE_CIRCUIT
  thermostatType[0]=THERMOSTAT_TYPE_CIRCUIT;
  circuitThermostatId=0;
#endif // CONFIG_MQTT_THERMOSTATS_NB0_TYPE_CIRCUIT

#ifdef CONFIG_MQTT_THERMOSTATS_NB1_TYPE_CIRCUIT
  thermostatType[1]=THERMOSTAT_TYPE_CIRCUIT;
  circuitThermostatId=1;
#endif // CONFIG_MQTT_THERMOSTATS_NB1_TYPE_CIRCUIT

#ifdef CONFIG_MQTT_THERMOSTATS_NB2_TYPE_CIRCUIT
  thermostatType[2]=THERMOSTAT_TYPE_CIRCUIT;
  circuitThermostatId=2;
#endif // CONFIG_MQTT_THERMOSTATS_NB2_TYPE_CIRCUIT

#ifdef CONFIG_MQTT_THERMOSTATS_NB3_TYPE_CIRCUIT
  thermostatType[3]=THERMOSTAT_TYPE_CIRCUIT;
  circuitThermostatId=3;
#endif // CONFIG_MQTT_THERMOSTATS_NB3_TYPE_CIRCUIT

  //create period read timer
  TimerHandle_t th =
    xTimerCreate( "thermostatLifeTickTimer",           /* Text name. */
                  pdMS_TO_TICKS(CONFIG_MQTT_THERMOSTATS_TICK_PERIOD * 1000),  /* Period. */
                  pdTRUE,                /* Autoreload. */
                  (void *)0,                  /* No ID. */
                  vThermostatTimerCallback );  /* Callback function. */
  if( th != NULL ) {
    ESP_LOGI(TAG, "timer is created");
    if (xTimerStart(th, portMAX_DELAY) != pdFALSE){
      ESP_LOGI(TAG, "timer is active");
    }
  }
  struct ThermostatMessage t;
  while(1) {
    if( xQueueReceive( thermostatQueue, &t , portMAX_DELAY) )
      {
        if (t.msgType == THERMOSTAT_LIFE_TICK) {
          thermostatDuration += 1;
          heatingDuration += 1; //fixme heatingControlStillNotClear4Me

          for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
            if (currentTemperatureFlag[id] > 0) {
              currentTemperatureFlag[id] -= 1;
              if (currentTemperatureFlag[id] == 0) {
                publish_thermostat_current_temperature_evt(id);
              }
            }
          }
          update_thermostat();
        }

        //new messages
        if (t.msgType == THERMOSTAT_CURRENT_TEMPERATURE) {
          ESP_LOGI(TAG, "Update temperature for thermostat %d", t.thermostatId);
          if (t.data.currentTemperature != SHRT_MIN) {
            currentTemperatureFlag[t.thermostatId] = SENSOR_LIFETIME;
          }

          if (circuitThermostatId == t.thermostatId) {
            if (currentTemperature_3 != currentTemperature_2) {
              currentTemperature_3 = currentTemperature_2;
            }
            if (currentTemperature_2 != currentTemperature_1) {
              currentTemperature_2 = currentTemperature_1;
            }
            if (currentTemperature_1 != currentTemperature[t.thermostatId]) {
              currentTemperature_1 = currentTemperature[t.thermostatId];
            }
          }
          if (currentTemperature[t.thermostatId] != t.data.currentTemperature) {
            currentTemperature[t.thermostatId] = t.data.currentTemperature;
            publish_thermostat_current_temperature_evt(t.thermostatId);
          }
        }

        if (t.msgType == THERMOSTAT_CMD_MODE) {
          if (thermostatMode[t.thermostatId] != t.data.thermostatMode) {
            thermostatMode[t.thermostatId] = t.data.thermostatMode;
            esp_err_t err = write_nvs_short(thermostatModeTAG[t.thermostatId],
                                            thermostatMode[t.thermostatId]);
            ESP_ERROR_CHECK( err );
          }
          publish_thermostat_mode_evt(t.thermostatId);
          publish_thermostat_action_evt(t.thermostatId);
        }

        if (t.msgType == THERMOSTAT_CMD_TARGET_TEMPERATURE) {
          if (targetTemperature[t.thermostatId] != t.data.targetTemperature) {
            targetTemperature[t.thermostatId] = t.data.thermostatMode;
            esp_err_t err = write_nvs_short(targetTemperatureTAG[t.thermostatId],
                                            targetTemperature[t.thermostatId]);
            ESP_ERROR_CHECK( err );
          }
          publish_thermostat_target_temperature_evt(t.thermostatId);
        }

        if (t.msgType == THERMOSTAT_CMD_TOLERANCE) {
          if (temperatureTolerance[t.thermostatId] != t.data.tolerance) {
            temperatureTolerance[t.thermostatId]=t.data.tolerance;
            esp_err_t err = write_nvs_short(temperatureToleranceTAG[t.thermostatId],
                                            temperatureTolerance[t.thermostatId]);
            ESP_ERROR_CHECK( err );
          }
          publish_thermostat_temperature_tolerance_evt(t.thermostatId);
        }
      }
  }
}

void read_nvs_thermostat_data()
{
  esp_err_t err;

  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    err=read_nvs_short(thermostatModeTAG[id], (short*) &thermostatMode[id]);
    ESP_ERROR_CHECK( err );
  }

  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    err=read_nvs_short(targetTemperatureTAG[id], (short*) &targetTemperature[id]);
    ESP_ERROR_CHECK( err );
  }

  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    err=read_nvs_short(temperatureToleranceTAG[id], (short*) &temperatureTolerance[id]);
    ESP_ERROR_CHECK( err );
  }
}
#endif // CONFIG_MQTT_THERMOSTATS_NB > 0
