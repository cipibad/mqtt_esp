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
#include "app_publish_data.h"

#include "app_waterpump.h"

#define QUEUE_TIMEOUT (60 * 1000 / portTICK_PERIOD_MS)

enum ThermostatState thermostatState = THERMOSTAT_STATE_IDLE;
unsigned int thermostatDuration = 0;

enum HeatingState heatingState = HEATING_STATE_IDLE;
unsigned int heatingDuration = 0;
int circuitThermostatId = -1;

bool thermostat_bump = false;
bool thermostat_was_bumped = false;

enum ThermostatMode thermostatMode[CONFIG_MQTT_THERMOSTATS_NB];

const char * thermostatModeTAG[CONFIG_MQTT_THERMOSTATS_NB] = {
  "thermMode0",
#if CONFIG_MQTT_THERMOSTATS_NB > 1
  "thermMode1",
#if CONFIG_MQTT_THERMOSTATS_NB > 2
  "thermMode2",
#if CONFIG_MQTT_THERMOSTATS_NB > 3
  "thermMode3",
#if CONFIG_MQTT_THERMOSTATS_NB > 4
  "thermMode4",
#if CONFIG_MQTT_THERMOSTATS_NB > 5
  "thermMode5",
#endif //CONFIG_MQTT_THERMOSTATS_NB > 5
#endif //CONFIG_MQTT_THERMOSTATS_NB > 4
#endif //CONFIG_MQTT_THERMOSTATS_NB > 3
#endif //CONFIG_MQTT_THERMOSTATS_NB > 2
#endif //CONFIG_MQTT_THERMOSTATS_NB > 1
};

short targetTemperature[CONFIG_MQTT_THERMOSTATS_NB];

const char * targetTemperatureTAG[CONFIG_MQTT_THERMOSTATS_NB] = {
  "targetTemp0",
#if CONFIG_MQTT_THERMOSTATS_NB > 1
  "targetTemp1",
#if CONFIG_MQTT_THERMOSTATS_NB > 2
  "targetTemp2",
#if CONFIG_MQTT_THERMOSTATS_NB > 3
  "targetTemp3",
#if CONFIG_MQTT_THERMOSTATS_NB > 4
  "targetTemp4",
#if CONFIG_MQTT_THERMOSTATS_NB > 5
  "targetTemp5",
#endif //CONFIG_MQTT_THERMOSTATS_NB > 5
#endif //CONFIG_MQTT_THERMOSTATS_NB > 4
#endif //CONFIG_MQTT_THERMOSTATS_NB > 3
#endif //CONFIG_MQTT_THERMOSTATS_NB > 2
#endif //CONFIG_MQTT_THERMOSTATS_NB > 1
};

short temperatureTolerance[CONFIG_MQTT_THERMOSTATS_NB];

const char * temperatureToleranceTAG[CONFIG_MQTT_THERMOSTATS_NB] = {
  "tempToler0",
#if CONFIG_MQTT_THERMOSTATS_NB > 1
  "tempToler1",
#if CONFIG_MQTT_THERMOSTATS_NB > 2
  "tempToler2",
#if CONFIG_MQTT_THERMOSTATS_NB > 3
  "tempToler3",
#if CONFIG_MQTT_THERMOSTATS_NB > 4
  "tempToler4",
#if CONFIG_MQTT_THERMOSTATS_NB > 5
  "tempToler5",
#endif //CONFIG_MQTT_THERMOSTATS_NB > 5
#endif //CONFIG_MQTT_THERMOSTATS_NB > 4
#endif //CONFIG_MQTT_THERMOSTATS_NB > 3
#endif //CONFIG_MQTT_THERMOSTATS_NB > 2
#endif //CONFIG_MQTT_THERMOSTATS_NB > 1
};

enum ThermostatType thermostatType[CONFIG_MQTT_THERMOSTATS_NB];

short currentTemperature[CONFIG_MQTT_THERMOSTATS_NB];

const char* thermostatFriendlyName[CONFIG_MQTT_THERMOSTATS_NB] = {
  CONFIG_MQTT_THERMOSTATS_NB0_FRIENDLY_NAME,
#if CONFIG_MQTT_THERMOSTATS_NB > 1
  CONFIG_MQTT_THERMOSTATS_NB1_FRIENDLY_NAME,
#if CONFIG_MQTT_THERMOSTATS_NB > 2
  CONFIG_MQTT_THERMOSTATS_NB2_FRIENDLY_NAME,
#if CONFIG_MQTT_THERMOSTATS_NB > 3
  CONFIG_MQTT_THERMOSTATS_NB3_FRIENDLY_NAME,
#if CONFIG_MQTT_THERMOSTATS_NB > 4
  CONFIG_MQTT_THERMOSTATS_NB4_FRIENDLY_NAME,
#if CONFIG_MQTT_THERMOSTATS_NB > 5
  CONFIG_MQTT_THERMOSTATS_NB5_FRIENDLY_NAME,
#endif //CONFIG_MQTT_THERMOSTATS_NB > 5
#endif //CONFIG_MQTT_THERMOSTATS_NB > 4
#endif //CONFIG_MQTT_THERMOSTATS_NB > 3
#endif //CONFIG_MQTT_THERMOSTATS_NB > 2
#endif //CONFIG_MQTT_THERMOSTATS_NB > 1
};

bool waterPumpOn[CONFIG_MQTT_THERMOSTATS_NB];

#define TEMPERATURE_SENSOR_ONLINE             0
#define TEMPERATURE_SENSOR_OFFLINE            1
#define TEMPERATURE_SENSOR_OBSOLETE           2

short currentTemperatureFlag[CONFIG_MQTT_THERMOSTATS_NB];

int inline temperatureSensorState(int id){
  if (currentTemperatureFlag[id] > (SENSOR_LIFETIME / 2)) {
    return TEMPERATURE_SENSOR_ONLINE;
  }
  if (currentTemperatureFlag[id] > 0) {
    return TEMPERATURE_SENSOR_OFFLINE;
  }
  return TEMPERATURE_SENSOR_OBSOLETE;
}

short currentTemperature_1 = SHRT_MIN;
short currentTemperature_2 = SHRT_MIN;
short currentTemperature_3 = SHRT_MIN;
short currentTemperature_4 = SHRT_MIN;

extern QueueHandle_t thermostatQueue;

static const char *TAG = "APP_THERMOSTAT";

void init_data(){
  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    thermostatMode[id] = THERMOSTAT_MODE_UNSET;
    targetTemperature[id] = 21*10; //21
    temperatureTolerance[id] = 5;  //0.5
    thermostatType[id] = THERMOSTAT_TYPE_NORMAL;
    currentTemperature[id] = SHRT_MIN;
    currentTemperatureFlag[id] = TEMPERATURE_SENSOR_OBSOLETE;
    waterPumpOn[id] = false;
  }
}

void update_thermostat_type() {
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

#ifdef CONFIG_MQTT_THERMOSTATS_NB4_TYPE_CIRCUIT
  thermostatType[4]=THERMOSTAT_TYPE_CIRCUIT;
  circuitThermostatId=4;
#endif // CONFIG_MQTT_THERMOSTATS_NB4_TYPE_CIRCUIT

#ifdef CONFIG_MQTT_THERMOSTATS_NB5_TYPE_CIRCUIT
  thermostatType[5]=THERMOSTAT_TYPE_CIRCUIT;
  circuitThermostatId=5;
#endif // CONFIG_MQTT_THERMOSTATS_NB5_TYPE_CIRCUIT

}

void update_waterpump_on() {
#ifdef CONFIG_MQTT_THERMOSTATS_NB0_WATERPUMP_ON
  waterPumpOn[0] = true;
#endif // CONFIG_MQTT_THERMOSTATS_NB0_WATERPUMP_ON

#ifdef CONFIG_MQTT_THERMOSTATS_NB1_WATERPUMP_ON
  waterPumpOn[1] = true;
#endif // CONFIG_MQTT_THERMOSTATS_NB1_WATERPUMP_ON

#ifdef CONFIG_MQTT_THERMOSTATS_NB2_WATERPUMP_ON
  waterPumpOn[2] = true;
#endif // CONFIG_MQTT_THERMOSTATS_NB2_WATERPUMP_ON

#ifdef CONFIG_MQTT_THERMOSTATS_NB3_WATERPUMP_ON
  waterPumpOn[3] = true;
#endif // CONFIG_MQTT_THERMOSTATS_NB3_WATERPUMP_ON

#ifdef CONFIG_MQTT_THERMOSTATS_NB4_WATERPUMP_ON
  waterPumpOn[4] = true;
#endif // CONFIG_MQTT_THERMOSTATS_NB4_WATERPUMP_ON

#ifdef CONFIG_MQTT_THERMOSTATS_NB5_WATERPUMP_ON
  waterPumpOn[5] = true;
#endif // CONFIG_MQTT_THERMOSTATS_NB5_WATERPUMP_ON

}

void thermostat_publish_local_data(int thermostat_id, int value)
{
  struct ThermostatMessage tm;
  memset(&tm, 0, sizeof(struct ThermostatMessage));
  tm.msgType = THERMOSTAT_CURRENT_TEMPERATURE;
  tm.thermostatId = thermostat_id;
  tm.data.currentTemperature = value;

  if (xQueueSend( thermostatQueue
                  ,( void * )&tm
                  ,QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to thermostatQueue");
  }
}

void publish_thermostat_current_temperature_evt(int id)
{
  if (currentTemperature[id] == SHRT_MIN)
    return;

  const char * thermostat_topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/ctemp/thermostat";

  char data[16];
  memset(data,0,16);
  sprintf(data, "%d.%d",
          currentTemperature[id] > 0 ? currentTemperature[id] / 10 : 0,
          currentTemperature[id] > 0 ? abs(currentTemperature[id] % 10) : 0);

  char topic[MAX_TOPIC_LEN];
  memset(topic,0,MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", thermostat_topic, id);

  publish_persistent_data(topic, data);
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
  const char * thermostat_topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/temp/thermostat";

  char data[16];
  memset(data,0,16);
  sprintf(data, "%d.%d", targetTemperature[id] / 10, abs(targetTemperature[id] % 10));

  char topic[MAX_TOPIC_LEN];
  memset(topic,0,MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", thermostat_topic, id);

  publish_persistent_data(topic, data);
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
  const char * thermostat_topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/tolerance/thermostat";

  char data[16];
  memset(data,0,16);
  sprintf(data, "%d.%d", temperatureTolerance[id] / 10, abs(temperatureTolerance[id] % 10));

  char topic[MAX_TOPIC_LEN];
  memset(topic,0,MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", thermostat_topic, id);

  publish_persistent_data(topic, data);
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
  const char * thermostat_topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/mode/thermostat";

  char data[16];
  memset(data,0,16);
  sprintf(data, "%s", thermostatMode[id] == THERMOSTAT_MODE_HEAT ? "heat" : "off");

  char topic[MAX_TOPIC_LEN];
  memset(topic,0,MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", thermostat_topic, id);

  publish_persistent_data(topic, data);
}

void publish_all_thermostats_mode_evt()
{
  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    publish_thermostat_mode_evt(id);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void publish_thermostat_status_evt(int id)
{
  const char * thermostat_topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/status/thermostat";

  char data[16];
  memset(data,0,16);
  sprintf(data, "%s", temperatureSensorState(id) == TEMPERATURE_SENSOR_ONLINE ? "online" : "offline");

  char topic[MAX_TOPIC_LEN];
  memset(topic,0,MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", thermostat_topic, id);

  publish_persistent_data(topic, data);
}

void publish_all_thermostats_status_evt()
{
  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    publish_thermostat_status_evt(id);
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
  const char * thermostat_topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/action/thermostat";

  char data[16];
  memset(data,0,16);
  if (thermostatType[id] == THERMOSTAT_TYPE_NORMAL) {
    get_normal_thermostat_action(data, id);
  } else {
    get_circuit_thermostat_action(data, id);
  }

  char topic[MAX_TOPIC_LEN];
  memset(topic,0,MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", thermostat_topic, id);

  publish_persistent_data(topic, data);
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
  publish_all_thermostats_status_evt();
  publish_all_thermostats_action_evt();
}

#if CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS
void publish_thermostat_notification_evt(const char* msg)
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/notification/thermostat";
  publish_non_persistent_data(topic, msg);
}

void publish_normal_thermostat_notification(unsigned int duration,
                                            const char *reason)
{
  char data[256];
  memset(data,0,256);

  sprintf(data, "Thermostat changed to %s due to %s. It was %s for %u minutes",
          thermostatState == THERMOSTAT_STATE_HEATING ? "on"  : "off", reason,
          thermostatState == THERMOSTAT_STATE_HEATING ? "off" : "on", duration);

  publish_thermostat_notification_evt(data);
}
#endif // CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS

bool heatingTermostatsNeedWaterPump()
{
  bool pumpNeeded = false;
  if (thermostatState == THERMOSTAT_STATE_HEATING) {
    for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
      if (waterPumpOn[id] && temperatureSensorState(id) != TEMPERATURE_SENSOR_OBSOLETE && thermostatMode[id] == THERMOSTAT_MODE_HEAT) {
        if (thermostatType[id] == THERMOSTAT_TYPE_NORMAL) {
          if (currentTemperature[id] <= (targetTemperature[id] + temperatureTolerance[id])) {
            pumpNeeded = true;
            break;
          }
        }
      }
    }
  }
  return pumpNeeded;
}

#ifdef CONFIG_WATERPUMP_SUPPORT
void update_water_pump_state()
{
  if (heatingTermostatsNeedWaterPump() || thermostat_was_bumped)
  {
    if(waterPumpStatus != WATERPUMP_STATUS_ON) {
      updateWaterPumpState(WATERPUMP_STATUS_ON);
    }
  }
  else
  { // ! heatingTermostatsNeedWaterPump()
    if (waterPumpStatus != WATERPUMP_STATUS_OFF) {
      updateWaterPumpState(WATERPUMP_STATUS_OFF);
    }
  }
}
#endif // CONFIG_WATERPUMP_SUPPORT

void disableThermostat(const char * reason)
{
  ESP_LOGI(TAG, "Turning thermostat off, reason: %s", reason);
  thermostatState=THERMOSTAT_STATE_IDLE;

  update_relay_status(CONFIG_MQTT_THERMOSTAT_RELAY_ID, RELAY_STATUS_OFF);
  publish_all_normal_thermostats_action_evt();
#if CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS
  publish_normal_thermostat_notification(thermostatDuration, reason);
#endif // CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS

  thermostatDuration = 0;

  ESP_LOGI(TAG, "thermostat disabled");
}

void enableThermostat(const char * reason)
{
  ESP_LOGI(TAG, "Turning thermostat on, reason: %s", reason);
  thermostatState=THERMOSTAT_STATE_HEATING;
  update_relay_status(CONFIG_MQTT_THERMOSTAT_RELAY_ID, RELAY_STATUS_ON);

  publish_all_normal_thermostats_action_evt();
#if CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS
  publish_normal_thermostat_notification(thermostatDuration, reason);
#endif // CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS

  thermostatDuration = 0;
  ESP_LOGI(TAG, "thermostat enabled");
}

#if CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS
void publish_circuit_thermostat_notification(unsigned int duration)
{
  char data[256];
  memset(data,0,256);

  sprintf(data, "Heating state changed to %s. It was %s for %u minutes",
          heatingState == HEATING_STATE_ENABLED ? "on"  : "off",
          heatingState == HEATING_STATE_ENABLED ? "off" : "on", duration);

  publish_thermostat_notification_evt(data);
}
#endif // CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS

void enableHeating()
{
  heatingState = HEATING_STATE_ENABLED;
  publish_all_circuit_thermostats_action_evt();
#if CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS
  publish_circuit_thermostat_notification(heatingDuration);
#endif // CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS

  heatingDuration = 0;
  ESP_LOGI(TAG, "heating enabled");
}

void disableHeating()
{
  heatingState = HEATING_STATE_IDLE;
  publish_all_circuit_thermostats_action_evt();
#if CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS
  publish_circuit_thermostat_notification(heatingDuration);
#endif // CONFIG_MQTT_THERMOSTAT_ENABLE_NOTIFICATIONS

  heatingDuration = 0;
  ESP_LOGI(TAG, "heating disabled");
}

void dump_data()
{
  ESP_LOGI(TAG, "thermostat state is %d", thermostatState);
  ESP_LOGI(TAG, "heating state is %d", heatingState);
#ifdef CONFIG_WATERPUMP_SUPPORT
  ESP_LOGI(TAG, "waterPump state is %d", getWaterPumpStatus());
#endif // CONFIG_WATERPUMP_SUPPORT
  ESP_LOGI(TAG, "thermostat_bump is %s", thermostat_bump ? "true" : "false");
  ESP_LOGI(TAG, "thermostat_was_bumped is %s", thermostat_was_bumped ? "true" : "false");

  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    ESP_LOGI(TAG, "thermostatMode[%d] is %d", id, thermostatMode[id]);
    ESP_LOGI(TAG, "currentTemperature[%d] is %d", id, currentTemperature[id]);
    ESP_LOGI(TAG, "currentTemperatureFlag[%d] is %d", id, currentTemperatureFlag[id]);
    ESP_LOGI(TAG, "targetTemperature[%d] is %d", id, targetTemperature[id]);
    ESP_LOGI(TAG, "temperatureTolerance[%d] is %d", id, temperatureTolerance[id]);

    if (thermostatType[id] == THERMOSTAT_TYPE_NORMAL) {
      ESP_LOGI(TAG, "thermostatType[%d] is THERMOSTAT_TYPE_NORMAL(%d)", id, thermostatType[id]);
    } else if (thermostatType[id] == THERMOSTAT_TYPE_CIRCUIT) {
      ESP_LOGI(TAG, "thermostatType[%d] is THERMOSTAT_TYPE_CIRCUIT(%d)", id, thermostatType[id]);
    }

    if (thermostatType[id] == THERMOSTAT_TYPE_CIRCUIT) {
      ESP_LOGI(TAG, "currentTemperature_1[%d] is %d", id, currentTemperature_1);
      ESP_LOGI(TAG, "currentTemperature_2[%d] is %d", id, currentTemperature_2);
      ESP_LOGI(TAG, "currentTemperature_3[%d] is %d", id, currentTemperature_3);
      ESP_LOGI(TAG, "currentTemperature_4[%d] is %d", id, currentTemperature_4);
    }
  }
}

bool sensor_is_reporting(char* reason)
{
  bool sensorReporting = false;
  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    if (thermostatType[id] == THERMOSTAT_TYPE_NORMAL && temperatureSensorState(id) != TEMPERATURE_SENSOR_OBSOLETE) {
      sensorReporting = true;
      break;
    }
  }
  if (sensorReporting == false) {
    sprintf(reason, "No live sensor is reporting");
  }
  return sensorReporting;
}

bool heating()
{
  if (circuitThermostatId == -1)
    return false;
  bool heating = false;
  if ((temperatureSensorState(circuitThermostatId) != TEMPERATURE_SENSOR_OBSOLETE) && thermostatMode[circuitThermostatId] == THERMOSTAT_MODE_HEAT) {
    if (currentTemperature_4 < currentTemperature_3 &&
        currentTemperature_3 < currentTemperature_2 &&
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
  if ((temperatureSensorState(circuitThermostatId) != TEMPERATURE_SENSOR_OBSOLETE) && thermostatMode[circuitThermostatId] == THERMOSTAT_MODE_HEAT) {
    if (currentTemperature_4 >= currentTemperature_3 &&
        currentTemperature_3 >= currentTemperature_2 &&
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
  if ((temperatureSensorState(circuitThermostatId) != TEMPERATURE_SENSOR_OBSOLETE) && thermostatMode[circuitThermostatId] == THERMOSTAT_MODE_HEAT)
    return  (currentTemperature[circuitThermostatId] <= targetTemperature[circuitThermostatId]);
  else
    return true;
}

bool circuitTooHot(char * reason)
{
  ESP_LOGI(TAG, "checking circuit %d to hot", circuitThermostatId);
  if (circuitThermostatId == -1)
    return false;
  if ((temperatureSensorState(circuitThermostatId) != TEMPERATURE_SENSOR_OBSOLETE) && thermostatMode[circuitThermostatId] == THERMOSTAT_MODE_HEAT) {
    if (currentTemperature[circuitThermostatId] >= temperatureTolerance[circuitThermostatId]) {
      sprintf(reason, "Circuit is too hot");
      return true;
    };
  }
  return false;
}

bool tooHot(char* reason)
{
  bool tooHot = true;
  char tstr[64];
  bool reasonUpdated = false;

  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    if ((temperatureSensorState(id) != TEMPERATURE_SENSOR_OFFLINE) && thermostatMode[id] == THERMOSTAT_MODE_HEAT) {
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
    if ((temperatureSensorState(id) != TEMPERATURE_SENSOR_OFFLINE) && thermostatMode[id] == THERMOSTAT_MODE_HEAT) {
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

bool update_heating(char* reason){
  bool heatingToggledOff = false;
  if ((heatingState == HEATING_STATE_IDLE) && heating()) {
    enableHeating();
  } else if ((heatingState == HEATING_STATE_ENABLED) && not_heating()) {
    disableHeating();
    sprintf(reason, "Heating is toggled off");
    heatingToggledOff = true;
  }
  return heatingToggledOff;
}
void update_thermostat()
{
  dump_data();

  char reason[256];
  memset(reason,0,256);

  bool heatingToggledOff = update_heating(reason);

  if (thermostatState == THERMOSTAT_STATE_HEATING) {
    if ((!sensor_is_reporting(reason)) || circuitTooHot(reason) || heatingToggledOff) {
      disableThermostat(reason);
      if (thermostat_was_bumped) {
        thermostat_was_bumped = false;
      }
    } else if (thermostat_was_bumped) { // let's have a limit for max bump running
      if (thermostatDuration > 120) { // 2h bump should be more than enough
        disableThermostat("Thermostat bump limit of 2h was reached");
        thermostat_was_bumped = false;
      }
    } else if (tooHot(reason)) { // check thermostats temps if not thermostat_was_bumped
      disableThermostat(reason);
    }
  } else { //   thermostatState == THERMOSTAT_STATE_IDLE
    if (circuitColdEnough()) {
      if (tooCold(reason)) {
        enableThermostat(reason);
      } else if (thermostat_bump){
        enableThermostat("Thermostat was bumped");
        thermostat_was_bumped = true;
      }
    }
  }

  #ifdef CONFIG_WATERPUMP_SUPPORT
  if (thermostatState == THERMOSTAT_STATE_HEATING) {
    update_water_pump_state();
  }
  #endif // CONFIG_WATERPUMP_SUPPORT

  if (thermostat_bump){
    thermostat_bump = false;
  }
}

void vThermostatTimerCallback( TimerHandle_t xTimer )
{
  ESP_LOGI(TAG, "Thermostat timer expired");
  struct ThermostatMessage t;
  t.msgType = THERMOSTAT_LIFE_TICK;
  if (xQueueSend( thermostatQueue,
                  ( void * )&t,
                  QUEUE_TIMEOUT) != pdPASS) {
  }
}

void handle_thermostat_cmd_task(void* pvParameters)
{
  update_thermostat_type();
  update_waterpump_on();


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
            const int prevTempSensorState = temperatureSensorState(id);
            if (prevTempSensorState != TEMPERATURE_SENSOR_OBSOLETE) {
              currentTemperatureFlag[id] -= 1;
            }
            if (temperatureSensorState(id) != prevTempSensorState) {
              publish_thermostat_status_evt(id);
            }

          }
          update_thermostat();
        }

        if (t.msgType == THERMOSTAT_CMD_BUMP) {
          thermostat_bump = true;
        }

        if (t.msgType == THERMOSTAT_CURRENT_TEMPERATURE) {
          ESP_LOGI(TAG, "Update temperature for thermostat %d", t.thermostatId);
          if (t.data.currentTemperature != SHRT_MIN) {
            const int prevTempSensorState = temperatureSensorState(t.thermostatId);
            currentTemperatureFlag[t.thermostatId] = SENSOR_LIFETIME;
            if (temperatureSensorState(t.thermostatId) != prevTempSensorState) {
              publish_thermostat_status_evt(t.thermostatId);
            }
          }

          if (circuitThermostatId == t.thermostatId) {
            if (currentTemperature_4 != currentTemperature_3) {
              currentTemperature_4 = currentTemperature_3;
            }
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
  init_data();
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
