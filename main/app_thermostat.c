#include "esp_system.h"
#include <limits.h>
#include <string.h>
#include <stdlib.h>

#if CONFIG_MQTT_THERMOSTATS_NB > 0

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


enum ThermostatMode thermostatMode[CONFIG_MQTT_THERMOSTATS_NB] = {THERMOSTAT_MODE_UNSET};

const char * thermostatModeTAG[CONFIG_MQTT_THERMOSTATS_NB] = {
  "thermMode0"
#if CONFIG_MQTT_THERMOSTATS_NB > 1
  "thermMode1"
#if CONFIG_MQTT_THERMOSTATS_NB > 2
  "thermMode2"
#if CONFIG_MQTT_THERMOSTATS_NB > 3
  "thermMode3"
#endif //CONFIG_MQTT_THERMOSTATS_NB > 3
#endif //CONFIG_MQTT_THERMOSTATS_NB > 2
#endif //CONFIG_MQTT_THERMOSTATS_NB > 1
};

short targetTemperature[CONFIG_MQTT_THERMOSTATS_NB] = {21*10};

const char * targetTemperatureTAG[CONFIG_MQTT_THERMOSTATS_NB] = {
  "targetTemp0"
#if CONFIG_MQTT_THERMOSTATS_NB > 1
  "targetTemp1"
#if CONFIG_MQTT_THERMOSTATS_NB > 2
  "targetTemp2"
#if CONFIG_MQTT_THERMOSTATS_NB > 3
  "targetTemp3"
#endif //CONFIG_MQTT_THERMOSTATS_NB > 3
#endif //CONFIG_MQTT_THERMOSTATS_NB > 2
#endif //CONFIG_MQTT_THERMOSTATS_NB > 1
};

short temperatureTolerance[CONFIG_MQTT_THERMOSTATS_NB] = {5}; //0.5

const char * temperatureToleranceTAG[CONFIG_MQTT_THERMOSTATS_NB] = {
  "tempToler0"
#if CONFIG_MQTT_THERMOSTATS_NB > 1
  "tempToler1"
#if CONFIG_MQTT_THERMOSTATS_NB > 2
  "tempToler2"
#if CONFIG_MQTT_THERMOSTATS_NB > 3
  "tempToler3"
#endif //CONFIG_MQTT_THERMOSTATS_NB > 3
#endif //CONFIG_MQTT_THERMOSTATS_NB > 2
#endif //CONFIG_MQTT_THERMOSTATS_NB > 1
};

enum ThermostatType thermostatType[CONFIG_MQTT_THERMOSTATS_NB] = {THERMOSTAT_TYPE_NORMAL};

short currentTemperature[CONFIG_MQTT_THERMOSTATS_NB] = {SHRT_MIN};
short currentTemperatureFlag[CONFIG_MQTT_THERMOSTATS_NB] = {0};

short currentTemperature_1 = SHRT_MIN;
short currentTemperature_2 = SHRT_MIN;
short currentTemperature_3 = SHRT_MIN;

extern QueueHandle_t thermostatQueue;

static const char *TAG = "APP_THERMOSTAT";

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

void publish_thermostat_state(const char* reason, unsigned int duration)
{
  const char * topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/thermostat/state";
  char data[256];
  memset(data,0,256);

  char tstr[64];

  sprintf(tstr, "{\"thermostatState\":%d,", thermostatState==THERMOSTAT_STATE_HEATING);
  strcat(data, tstr);

  //FIXME this should print only we have at least one circuit thermostat
  sprintf(tstr, "\"heatingState\":%d,", heatingState == HEATING_STATE_ENABLED);
  strcat(data, tstr);

  if(!reason) {
    reason = "";
  }

  sprintf(tstr, "\"reason\":\"%s\", \"duration\":%u}",
          reason, duration);
  strcat(data, tstr);

  mqtt_publish_data(topic, data, QOS_1, RETAIN);
}

void publish_thermostat_data()
{

  publish_all_thermostats_current_temperature_evt();
  publish_all_thermostats_target_temperature_evt();
  publish_all_thermostats_temperature_tolerance_evt();
  publish_all_thermostats_mode_evt();
  publish_all_thermostats_action_evt();
  
  publish_thermostat_state(NULL, 0);
}


void disableThermostat(const char * reason)
{
  publish_thermostat_state(NULL, 0);

  thermostatState=THERMOSTAT_STATE_IDLE;
  update_relay_status(CONFIG_MQTT_THERMOSTAT_RELAY_ID, RELAY_STATUS_OFF);

  publish_all_thermostats_action_evt();
  publish_thermostat_state(reason, thermostatDuration);

  thermostatDuration = 0;
  ESP_LOGI(TAG, "thermostat disabled");
}

void enableThermostat(const char * reason)
{
  publish_thermostat_state(NULL, 0);

  thermostatState=THERMOSTAT_STATE_HEATING;
  update_relay_status(CONFIG_MQTT_THERMOSTAT_RELAY_ID, RELAY_STATUS_ON);

  publish_all_thermostats_action_evt();
  publish_thermostat_state(reason, thermostatDuration);

  thermostatDuration = 0;
  ESP_LOGI(TAG, "thermostat enabled");
}

void enableHeating()
{
  publish_thermostat_state(NULL, 0);
  heatingState = HEATING_STATE_ENABLED;
  publish_thermostat_state("Heating was enabled", heatingDuration);

  heatingDuration = 0;
  ESP_LOGI(TAG, "heating enabled");
}
void disableHeating()
{
  publish_thermostat_state(NULL, 0);
  heatingState = HEATING_STATE_IDLE;
  publish_thermostat_state("Heating was disabled", heatingDuration);
  heatingDuration = 0;
  ESP_LOGI(TAG, "heating2 disabled");
}

void update_thermostat()
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
  

  bool sensorReporting = false;
  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    if (thermostatType[id] == THERMOSTAT_TYPE_NORMAL && currentTemperatureFlag[id] != 0) {
      sensorReporting = true;
      break;
    }
  }
  
  if (!sensorReporting) {
    ESP_LOGI(TAG, "no live sensor is reporting => no thermostat handling");
    if (thermostatState==THERMOSTAT_STATE_HEATING) {
      ESP_LOGI(TAG, "stop thermostat as no live sensor is reporting");
      disableThermostat("Thermostat was disabled as no live sensor is reporting");
    }
    return;
  }

  bool heating = false;
  bool not_heating = false;
  
  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    if ((currentTemperatureFlag[id] > 0) && thermostatMode[id] == THERMOSTAT_MODE_HEAT) {
      if (thermostatType[id] == THERMOSTAT_TYPE_CIRCUIT) {
        if (currentTemperature[id] != SHRT_MIN &&
            currentTemperature_1 != SHRT_MIN &&
            currentTemperature_2 != SHRT_MIN &&
            currentTemperature_3 != SHRT_MIN) {
          if (currentTemperature_3 < currentTemperature_2 &&
              currentTemperature_2 < currentTemperature_1 &&
              currentTemperature_1 < currentTemperature[id]) {
            heating = true;
          }
          if (currentTemperature_3 >= currentTemperature_2 &&
              currentTemperature_2 >= currentTemperature_1 &&
              currentTemperature_1 >= currentTemperature[id]) {
            not_heating = true;
          }
        }
      }
    }
  }
  
  bool heatingToggledOff = false;

  if ((heatingState == HEATING_STATE_IDLE) && heating) {
    enableHeating();
  } else if ((heatingState == HEATING_STATE_ENABLED) && not_heating) {
    disableHeating();
    heatingToggledOff = true;
  }
  
  char reason[256];
  memset(reason,0,256);

  char tstr[64];

  bool hotEnough = true;
  bool tooCold = false;
  bool circuitColdEnough = true;
    
  for(int id = 0; id < CONFIG_MQTT_THERMOSTATS_NB; id++) {
    if ((currentTemperatureFlag[id] > 0) && thermostatMode[id] == THERMOSTAT_MODE_HEAT) {
      if (thermostatType[id] == THERMOSTAT_TYPE_NORMAL) {
        if (currentTemperature[id] > (targetTemperature[id] + temperatureTolerance[id])) {
          sprintf(tstr, "thermostat[%d] is hot enough, ", id);
          strcat(reason, tstr);
        } else if (currentTemperature[id] < (targetTemperature[id] - temperatureTolerance[id])) {
          sprintf(tstr, "thermostat[%d] is too cold, ", id);
          strcat(reason, tstr);
          tooCold = true;
          hotEnough = false;
        } else{
          sprintf(tstr, "thermostat[%d] is good, ", id);
          strcat(reason, tstr);
          hotEnough = false;
        }
      }
      if (thermostatType[id] == THERMOSTAT_TYPE_CIRCUIT) {
        if (currentTemperature > targetTemperature) {
          circuitColdEnough = false;
        }
      }
    }

    if (thermostatState == THERMOSTAT_STATE_HEATING &&
        (heatingToggledOff || hotEnough)) {
      ESP_LOGI(TAG, "reason: %s", reason);
      disableThermostat(reason);
    }

    if (thermostatState != THERMOSTAT_STATE_HEATING &&
        tooCold && circuitColdEnough) {
      ESP_LOGI(TAG, "reason: %s", reason);
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
#endif // CONFIG_MQTT_THERMOSTATS_NB0_TYPE_CIRCUIT

#ifdef CONFIG_MQTT_THERMOSTATS_NB1_TYPE_CIRCUIT
  thermostatType[1]=THERMOSTAT_TYPE_CIRCUIT;
#endif // CONFIG_MQTT_THERMOSTATS_NB1_TYPE_CIRCUIT

#ifdef CONFIG_MQTT_THERMOSTATS_NB2_TYPE_CIRCUIT
  thermostatType[2]=THERMOSTAT_TYPE_CIRCUIT;
#endif // CONFIG_MQTT_THERMOSTATS_NB2_TYPE_CIRCUIT

#ifdef CONFIG_MQTT_THERMOSTATS_NB3_TYPE_CIRCUIT
  thermostatType[3]=THERMOSTAT_TYPE_CIRCUIT;
#endif // CONFIG_MQTT_THERMOSTATS_NB3_TYPE_CIRCUIT

  //create period read timer
  TimerHandle_t th =
    xTimerCreate( "thermostatSensorsTimer",           /* Text name. */
                  pdMS_TO_TICKS(60000),  /* Period. */
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
          if (t.data.currentTemperature != SHRT_MIN) {
            currentTemperature[t.thermostatId] = SENSOR_LIFETIME;
            if (currentTemperature_3 != currentTemperature_2) {
              currentTemperature_3 = currentTemperature_2;
            }
            if (currentTemperature_2 != currentTemperature_1) {
              currentTemperature_2 = currentTemperature_1;
            }
            if (currentTemperature_1 != currentTemperature[t.thermostatId]) {
              currentTemperature_1 = currentTemperature[t.thermostatId];
            }
            if (currentTemperature[t.thermostatId] != t.data.currentTemperature) {
              currentTemperature[t.thermostatId] = t.data.currentTemperature;
              publish_thermostat_current_temperature_evt(t.thermostatId);
            }
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
