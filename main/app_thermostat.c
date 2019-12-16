#include "esp_system.h"
#include <limits.h>
#include <string.h>

#ifdef CONFIG_MQTT_THERMOSTAT

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "app_main.h"
#include "app_relay.h"
#include "app_thermostat.h"
#include "app_nvs.h"
#include "app_mqtt.h"

bool thermostatEnabled = false;
unsigned int thermostatDuration = 0;

enum HoldOffMode holdOffMode=HOLD_OFF_DISABLED;

enum ThermostatMode thermostatMode=TERMOSTAT_MODE_OFF;
const char * thermostatModeTAG="thermMode";

#ifdef CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
bool heatingEnabled = false;
unsigned int heatingDuration = 0;

short waterTargetTemperature=23*10; //30 degrees
const char * waterTargetTemperatureTAG="wTargetTemp";

short waterTemperatureSensibility=5; //0.5 degrees
const char * waterTemperatureSensibilityTAG="wTempSens";

unsigned int waterTemperature = 0;
unsigned int waterTemperatureFlag = 0;

short circuitTargetTemperature=23*10; //30 degrees
const char * circuitTargetTemperatureTAG="ctgtTemp";
unsigned int circuitTemperature = 0;

unsigned int circuitTemperatureFlag = 0;

unsigned int circuitTemperature_1 = 0;
unsigned int circuitTemperature_2 = 0;
unsigned int circuitTemperature_3 = 0;

#endif //CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER

#if CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0
short room0TargetTemperature=22*10;
const char * room0TargetTemperatureTAG="r0targetTemp";

short room0TemperatureSensibility=2;
const char * room0TemperatureSensibilityTAG="r0TempSens";

short room0Temperature = SHRT_MIN;
unsigned char room0TemperatureFlag = 0;
#endif //CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0

extern QueueHandle_t thermostatQueue;


static const char *TAG = "APP_THERMOSTAT";

void publish_thermostat_cfg()
{
  const char * topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/thermostat/cfg";

  char data[256];
  memset(data,0,256);

  char tstr[128];
  strcat(data, "{");

#ifdef CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
  sprintf(tstr, "\"circuitTargetTemperature\":%d.%01d,\"waterTargetTemperature\":%d.%01d,\"waterTemperatureSensibility\":%d.%01d",
          circuitTargetTemperature/10, circuitTargetTemperature%10,
          waterTargetTemperature/10, waterTargetTemperature%10,
          waterTemperatureSensibility/10, waterTemperatureSensibility%10)
  strcat(data, tstr);
#endif //CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER

#if CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0
  sprintf(tstr, "\"room0TargetTemperature\":%d.%01d,\"room0TemperatureSensibility\":%d.%01d,",
          room0TargetTemperature/10, room0TargetTemperature%10,
          room0TemperatureSensibility/10, room0TemperatureSensibility%10);
  strcat(data, tstr);
#endif //CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0

  sprintf(tstr, "\"thermostatMode\":%d,\"holdOffMode\":%d}",
          thermostatMode, holdOffMode);
  strcat(data, tstr);
  
  mqtt_publish_data(topic, data, QOS_1, RETAIN);
}

void publish_thermostat_state(const char* reason, unsigned int duration)
{
  const char * topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/thermostat/state";
  char data[256];
  memset(data,0,256);

  char tstr[64];
  strcat(data, "{");

  sprintf(tstr, "{\"thermostatState\":%d,", thermostatEnabled);
  strcat(data, tstr);
  
#ifdef CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
  sprintf(tstr, "\"heatingState\":%d,", heatingEnabled)
  strcat(data, tstr);
#endif //CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER

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
  publish_thermostat_state(NULL, 0);
  publish_thermostat_cfg();
}


void disableThermostat(const char * reason)
{
  publish_thermostat_state(NULL, 0);
  thermostatEnabled=false;
  update_relay_state(CONFIG_MQTT_THERMOSTAT_RELAY_ID, 0);
  publish_thermostat_state(reason, thermostatDuration);
  thermostatDuration = 0;
  ESP_LOGI(TAG, "thermostat disabled");
}

void enableThermostat(const char * reason)
{
  publish_thermostat_state(NULL, 0);
  thermostatEnabled=true;
  update_relay_state(CONFIG_MQTT_THERMOSTAT_RELAY_ID, 1);
  publish_thermostat_state(reason, thermostatDuration);
  thermostatDuration = 0;
  ESP_LOGI(TAG, "thermostat enabled");
}

#ifdef CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
void enableHeating()
{
  publish_thermostat_state(NULL, 0);
  heatingEnabled = true;
  ESP_LOGI(TAG, "heating enabled");
  publish_thermostat_state("Heating was enabled", heatingDuration);
  heatingDuration = 0;
}
void disableHeating()
{
  publish_thermostat_state(NULL, 0);
  heatingEnabled = false;
  ESP_LOGI(TAG, "heating2 disabled");
  publish_thermostat_state("Heating was disabled", heatingDuration);
  heatingDuration = 0;
}

#endif //CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER

void update_thermostat()
{
  bool heatingToggledOff = false;
#ifdef CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
  ESP_LOGI(TAG, "heatingEnabled state is %d", heatingEnabled);
  ESP_LOGI(TAG, "circuitTemperature_n is %d", circuitTemperature);
  ESP_LOGI(TAG, "circuitTemperature_n_1 is %d", circuitTemperature_1);
  ESP_LOGI(TAG, "circuitTemperature_n_2 is %d", circuitTemperature_2);
  ESP_LOGI(TAG, "circuitTemperature_n_3 is %d", circuitTemperature_3);
  ESP_LOGI(TAG, "circuitTargetTemperature is %d", circuitTargetTemperature);
  ESP_LOGI(TAG, "waterTemperature is %d", waterTemperature);
  ESP_LOGI(TAG, "waterTargetTemperature is %d", waterTargetTemperature);
  ESP_LOGI(TAG, "waterTemperatureSensibility is %d", waterTemperatureSensibility);
#endif //CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
  ESP_LOGI(TAG, "thermostatMode is %d", thermostatMode);
  ESP_LOGI(TAG, "thermostat state is %d", thermostatEnabled);
#if CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0
  ESP_LOGI(TAG, "room0Temperature is %d", room0Temperature);
  ESP_LOGI(TAG, "room0TargetTemperature is %d", room0TargetTemperature);
  ESP_LOGI(TAG, "room0TemperatureSensibility is %d", room0TemperatureSensibility);
#endif //CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0

  bool sensorReporting = false;
#ifdef CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
  if (waterTemperatureFlag != 0){
    sensorReporting = true;
  }
#endif //CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
#if CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0
  if (room0TemperatureFlag != 0) {
    sensorReporting = true;
  }
#endif //CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0
  if (!sensorReporting) {
    ESP_LOGI(TAG, "no live sensor is reporting => no thermostat handling");
    if (thermostatEnabled==true) {
      ESP_LOGI(TAG, "stop thermostat as no live sensor is reporting");
      disableThermostat("Thermostat was disabled as no live sensor is reporting");
    }
    return;
  }

#ifdef CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
  if (circuitTemperature_3 && circuitTemperature_2 && circuitTemperature_1 && circuitTemperature) {//four consecutive valid readings
    if (!heatingEnabled
        && ( circuitTemperature_3 < circuitTemperature_2
             && circuitTemperature_2 < circuitTemperature_1
             && circuitTemperature_1 < circuitTemperature )){ //water is heating 1 2 3 4
      enableHeating();
    }

    if (heatingEnabled
        && ( circuitTemperature_3 >= circuitTemperature_2
             && circuitTemperature_2 >= circuitTemperature_1
             && circuitTemperature_1 >= circuitTemperature )) { //heating is disabled   5 4 3 2
      disableHeating();
      heatingToggledOff = true;
    }
  }
#endif //CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
  bool waterHotEnough = true;
  bool waterTooCold = false;
  bool circuitColdEnough = true;
#ifdef CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
  waterHotEnough = ((waterTemperatureFlag > 0) && thermostatMode & BIT_WATER_SENSOR) ?
    (waterTemperature > (waterTargetTemperature + waterTemperatureSensibility)) : true;

   waterTooCold = ((waterTemperatureFlag > 0) && thermostatMode & BIT_WATER_SENSOR)?
    (waterTemperature < (waterTargetTemperature - waterTemperatureSensibility)) : false;

  circuitColdEnough = (circuitTemperatureFlag > 0) ? (circuitTemperature < circuitTargetTemperature) : true;

#endif //CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER

  bool roomHotEnough = true;
  bool roomTooCold = false;
#if CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0
  roomHotEnough = ((room0TemperatureFlag > 0) && thermostatMode & BIT_ROOM_SENSOR) ?
    (room0Temperature > (room0TargetTemperature + room0TemperatureSensibility)) : true;

  roomTooCold = ((room0TemperatureFlag > 0) && thermostatMode & BIT_ROOM_SENSOR) ?
    (room0Temperature < (room0TargetTemperature - room0TemperatureSensibility)) : false;
#endif //CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0


  if (thermostatEnabled &&
      (heatingToggledOff || (roomHotEnough && waterHotEnough))) {
    const char * reason = heatingToggledOff ?
      ((roomHotEnough && waterHotEnough) ? "Thermostat was disabled because heating stopped and water and rooms are too hot" : "Thermostat was disabled because heating stopped") :
      ((roomHotEnough && waterHotEnough) ? "Thermostat was disabled because water and rooms are too hot" : "should never print" );
    ESP_LOGI(TAG, "%s", reason);
    disableThermostat(reason);
  }

  if (!thermostatEnabled &&
      (waterTooCold || roomTooCold) && circuitColdEnough && (holdOffMode != HOLD_OFF_ENABLED)) {
    const char * reason = waterTooCold ?
      (roomTooCold ? "Thermostat was enabled because water and room are too cold" : "Thermostat was enabled because water is too cold") :
      (roomTooCold ? "Thermostat was enabled because room is too cold" : "should never print");
    ESP_LOGI(TAG, "%s", reason);
    enableThermostat(reason);
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

#if CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0
void handle_room_temperature_msg(short newRoomTemperature)
{
  if (newRoomTemperature != SHRT_MIN) {
    if (room0Temperature != newRoomTemperature) {
      room0Temperature = newRoomTemperature;
    }
    room0TemperatureFlag = SENSOR_LIFETIME;
  }
}
#endif //CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0

#ifdef CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
void handle_sensors_msg(short newWaterTemperature, short newCircuitTemperature)
{
  if (newWaterTemperature != SHRT_MIN) {
    if (newWaterTemperature != waterTemperature) {
      waterTemperature = newWaterTemperature;
    }
    waterTemperatureFlag = SENSOR_LIFETIME;
  }

  if (circuitTemperature != SHRT_MIN) {
    circuitTemperature_3 = circuitTemperature_2;
    circuitTemperature_2 = circuitTemperature_1;
    circuitTemperature_1 = circuitTemperature;
    circuitTemperature = newCircuitTemperature;
    circuitTemperatureFlag = SENSOR_LIFETIME;
  }
}
#endif //CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER

void handle_thermostat_cmd_task(void* pvParameters)
{

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
        if (t.msgType == THERMOSTAT_CFG_MSG) {
          bool updated = false;
#ifdef CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
          if (t.data.cfgData.circuitTargetTemperature && circuitTargetTemperature != t.data.cfgData.circuitTargetTemperature) {
            circuitTargetTemperature=t.data.cfgData.circuitTargetTemperature;
            esp_err_t err = write_nvs_short(circuitTargetTemperatureTAG, circuitTargetTemperature);
            ESP_ERROR_CHECK( err );
            updated = true;
          }
          if (t.data.cfgData.waterTargetTemperature && waterTargetTemperature != t.data.cfgData.waterTargetTemperature) {
            waterTargetTemperature=t.data.cfgData.waterTargetTemperature;
            esp_err_t err = write_nvs_short(waterTargetTemperatureTAG, waterTargetTemperature);
            ESP_ERROR_CHECK( err );
            updated = true;
          }
          if (t.data.cfgData.waterTemperatureSensibility && waterTemperatureSensibility != t.data.cfgData.waterTemperatureSensibility) {
            waterTemperatureSensibility=t.data.cfgData.waterTemperatureSensibility;
            esp_err_t err = write_nvs_short(waterTemperatureSensibilityTAG, waterTemperatureSensibility);
            ESP_ERROR_CHECK( err );
            updated = true;
          }
#endif //CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
#if CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0
          if (t.data.cfgData.room0TargetTemperature && room0TargetTemperature != t.data.cfgData.room0TargetTemperature) {
            room0TargetTemperature=t.data.cfgData.room0TargetTemperature;
            esp_err_t err = write_nvs_short(room0TargetTemperatureTAG, room0TargetTemperature);
            ESP_ERROR_CHECK( err );
            updated = true;
          }
          if (t.data.cfgData.room0TemperatureSensibility && room0TemperatureSensibility != t.data.cfgData.room0TemperatureSensibility) {
            room0TemperatureSensibility=t.data.cfgData.room0TemperatureSensibility;
            esp_err_t err = write_nvs_short(room0TemperatureSensibilityTAG, room0TemperatureSensibility);
            ESP_ERROR_CHECK( err );
            updated = true;
          }
#endif //CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0
          if (t.data.cfgData.thermostatMode && thermostatMode != t.data.cfgData.thermostatMode) {
            thermostatMode=t.data.cfgData.thermostatMode;
            esp_err_t err = write_nvs_short(thermostatModeTAG, thermostatMode);
            ESP_ERROR_CHECK( err );
            updated = true;
          }
          if (t.data.cfgData.holdOffMode && holdOffMode != t.data.cfgData.holdOffMode) {
            holdOffMode=t.data.cfgData.holdOffMode;
            updated = true;
          }

          if (updated) {
            publish_thermostat_cfg();
          }
        }
#ifdef CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
        if (t.msgType == THERMOSTAT_SENSORS_MSG) {
          handle_sensors_msg(t.data.sensorsData.wtemperature, t.data.sensorsData.ctemperature);
        }
#endif //CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
#if CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0
        if (t.msgType == THERMOSTAT_ROOM_0_MSG) {
          handle_room_temperature_msg(t.data.roomData.temperature);
        }
#endif //CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0
        if (t.msgType == THERMOSTAT_LIFE_TICK) {
          thermostatDuration += 1;
#ifdef CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
          heatingDuration += 1;
          if (waterTemperatureFlag > 0)
            waterTemperatureFlag -= 1;
          if (circuitTemperatureFlag > 0)
            circuitTemperatureFlag -= 1;
          ESP_LOGI(TAG, "waterTemperatureFlag: %d, circuitTemperatureFlag: %d",
                   waterTemperatureFlag, circuitTemperatureFlag);
#endif //CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER

#if CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0
          if (room0TemperatureFlag > 0)
            room0TemperatureFlag -= 1;

          ESP_LOGI(TAG, "room0TemperatureFlag: %d",
                   room0TemperatureFlag);
#endif //CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0
          update_thermostat();
        }
      }
  }
}

void read_nvs_thermostat_data()
{
  esp_err_t err;
#ifdef CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
  esp_err_t err=read_nvs_short(circuitTargetTemperatureTAG, &circuitTargetTemperature);
  ESP_ERROR_CHECK( err );

  err=read_nvs_short(waterTargetTemperatureTAG, &waterTargetTemperature);
  ESP_ERROR_CHECK( err );

  err=read_nvs_short(waterTemperatureSensibilityTAG, &waterTemperatureSensibility);
  ESP_ERROR_CHECK( err );
#endif //CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER

#if CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0
  err=read_nvs_short(room0TargetTemperatureTAG, &room0TargetTemperature);
  ESP_ERROR_CHECK( err );

  err=read_nvs_short(room0TemperatureSensibilityTAG, &room0TemperatureSensibility);
  ESP_ERROR_CHECK( err );
#endif //CONFIG_MQTT_THERMOSTAT_ROOMS_SENSORS_NB > 0

  err=read_nvs_short(thermostatModeTAG, (short*) &thermostatMode);
  ESP_ERROR_CHECK( err );
}
#endif // CONFIG_MQTT_THERMOSTAT
