#include "esp_system.h"
#ifdef CONFIG_MQTT_THERMOSTAT

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include <string.h>

#include "app_main.h"
#include "app_relay.h"
#include "app_thermostat.h"
#include "app_nvs.h"
#include "app_mqtt.h"

bool thermostatEnabled = false;
bool heatingEnabled = false;
unsigned int thermostatDuration = 0;
unsigned int heatingDuration = 0;

int circuitTargetTemperature=23*10; //30 degrees
int waterTargetTemperature=23*10; //30 degrees
int waterTemperatureSensibility=5; //0.5 degrees

int room0TargetTemperature=22*10;
int room0TemperatureSensibility=2;

enum ThermostatMode thermostatMode=TERMOSTAT_MODE_OFF;

const char * circuitTargetTemperatureTAG="ctgtTemp";
const char * waterTargetTemperatureTAG="wTargetTemp";
const char * waterTemperatureSensibilityTAG="wTempSens";
const char * room0TargetTemperatureTAG="r0targetTemp";
const char * room0TemperatureSensibilityTAG="r0TempSens";
const char * thermostatModeTAG="thermMode";

unsigned int room0Temperature = 0;
unsigned int waterTemperature = 0;
unsigned int circuitTemperature = 0;

unsigned int room0TemperatureFlag = SENSOR_LIFETIME;
unsigned int waterTemperatureFlag = SENSOR_LIFETIME;
unsigned int circuitTemperatureFlag = SENSOR_LIFETIME;

unsigned int circuitTemperature_1 = 0;
unsigned int circuitTemperature_2 = 0;
unsigned int circuitTemperature_3 = 0;

extern QueueHandle_t thermostatQueue;


static const char *TAG = "APP_THERMOSTAT";

void publish_thermostat_cfg()
{
  const char * topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/thermostat/cfg";

  char data[256];
  memset(data,0,256);

  sprintf(data, "{\"circuitTargetTemperature\":%d.%01d, \"waterTargetTemperature\":%d.%01d, \"waterTemperatureSensibility\":%d.%01d, \"room0TargetTemperature\":%d.%01d, \"room0TemperatureSensibility\":%d.%01d, \"thermostatMode\":%d}",
          circuitTargetTemperature/10, circuitTargetTemperature%10,
          waterTargetTemperature/10, waterTargetTemperature%10,
          waterTemperatureSensibility/10, waterTemperatureSensibility%10,
          room0TargetTemperature/10,room0TargetTemperature%10,
          room0TemperatureSensibility/10, room0TemperatureSensibility%10,
          thermostatMode);
  mqtt_publish_data(topic, data, QOS_1, RETAIN);
}

void publish_thermostat_state(const char* reason, unsigned int duration)
{
  const char * topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/thermostat/state";
  char data[256];
  memset(data,0,256);

  if(!reason) {
    reason = "";
  }
  sprintf(data, "{\"thermostatState\":%d, \"heatingState\":%d, \"reason\":\"%s\", \"duration\":%u}",
          thermostatEnabled, heatingEnabled, reason, duration);

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

void update_thermostat()
{
  bool heatingToggledOff = false;
  ESP_LOGI(TAG, "heatingEnabled state is %d", heatingEnabled);
  ESP_LOGI(TAG, "circuitTemperature_n is %d", circuitTemperature);
  ESP_LOGI(TAG, "circuitTemperature_n_1 is %d", circuitTemperature_1);
  ESP_LOGI(TAG, "circuitTemperature_n_2 is %d", circuitTemperature_2);
  ESP_LOGI(TAG, "circuitTemperature_n_3 is %d", circuitTemperature_3);
  ESP_LOGI(TAG, "circuitTargetTemperature is %d", circuitTargetTemperature);
  ESP_LOGI(TAG, "thermostatMode is %d", thermostatMode);
  ESP_LOGI(TAG, "thermostat state is %d", thermostatEnabled);
  ESP_LOGI(TAG, "waterTemperature is %d", waterTemperature);
  ESP_LOGI(TAG, "waterTargetTemperature is %d", waterTargetTemperature);
  ESP_LOGI(TAG, "waterTemperatureSensibility is %d", waterTemperatureSensibility);
  ESP_LOGI(TAG, "room0Temperature is %d", room0Temperature);
  ESP_LOGI(TAG, "room0TargetTemperature is %d", room0TargetTemperature);
  ESP_LOGI(TAG, "room0TemperatureSensibility is %d", room0TemperatureSensibility);

  if ( waterTemperatureFlag == 0 && room0TemperatureFlag == 0 ) {
    ESP_LOGI(TAG, "no live sensor is reporting => no thermostat handling");
    if (thermostatEnabled==true) {
      ESP_LOGI(TAG, "stop thermostat as no live sensor is reporting");
      disableThermostat("Thermostat was disabled as no live sensor is reporting");
    }
    return;
  }

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

  bool waterTooHot = ((waterTemperatureFlag > 0) && thermostatMode & BIT_WATER_SENSOR) ?
    (waterTemperature > (waterTargetTemperature + waterTemperatureSensibility)) : true;

  bool roomTooHot = ((room0TemperatureFlag > 0) && thermostatMode & BIT_ROOM_SENSOR) ?
    (room0Temperature > (room0TargetTemperature + room0TemperatureSensibility)) : true;

  bool waterTooCold = ((waterTemperatureFlag > 0) && thermostatMode & BIT_WATER_SENSOR)?
    (waterTemperature < (waterTargetTemperature - waterTemperatureSensibility)) : false;

  bool roomTooCold = ((room0TemperatureFlag > 0) && thermostatMode & BIT_ROOM_SENSOR) ?
    (room0Temperature < (room0TargetTemperature - room0TemperatureSensibility)) : false;

  bool circuitColdEnough = (circuitTemperatureFlag > 0) ? (circuitTemperature < circuitTargetTemperature) : true;

  if (thermostatEnabled &&
      (heatingToggledOff || (roomTooHot && waterTooHot))) {
    const char * reason = heatingToggledOff ?
      ((roomTooHot && waterTooHot) ? "Thermostat was disabled because heating stopped and water and rooms are too hot" : "Thermostat was disabled because heating stopped") :
      ((roomTooHot && waterTooHot) ? "Thermostat was disabled because water and rooms are too hot" : "should never print" );
    ESP_LOGI(TAG, "%s", reason);
    disableThermostat(reason);
  }

  if (!thermostatEnabled &&
      (waterTooCold || roomTooCold) && circuitColdEnough && !(thermostatMode & BIT_HOLD_OFF)) {
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
          if (t.data.cfgData.circuitTargetTemperature && circuitTargetTemperature != t.data.cfgData.circuitTargetTemperature) {
            circuitTargetTemperature=t.data.cfgData.circuitTargetTemperature;
            esp_err_t err = write_nvs_integer(circuitTargetTemperatureTAG, circuitTargetTemperature);
            ESP_ERROR_CHECK( err );
            updated = true;
          }
          if (t.data.cfgData.waterTargetTemperature && waterTargetTemperature != t.data.cfgData.waterTargetTemperature) {
            waterTargetTemperature=t.data.cfgData.waterTargetTemperature;
            esp_err_t err = write_nvs_integer(waterTargetTemperatureTAG, waterTargetTemperature);
            ESP_ERROR_CHECK( err );
            updated = true;
          }
          if (t.data.cfgData.waterTemperatureSensibility && waterTemperatureSensibility != t.data.cfgData.waterTemperatureSensibility) {
            waterTemperatureSensibility=t.data.cfgData.waterTemperatureSensibility;
            esp_err_t err = write_nvs_integer(waterTemperatureSensibilityTAG, waterTemperatureSensibility);
            ESP_ERROR_CHECK( err );
            updated = true;
          }
          if (t.data.cfgData.room0TargetTemperature && room0TargetTemperature != t.data.cfgData.room0TargetTemperature) {
            room0TargetTemperature=t.data.cfgData.room0TargetTemperature;
            esp_err_t err = write_nvs_integer(room0TargetTemperatureTAG, room0TargetTemperature);
            ESP_ERROR_CHECK( err );
            updated = true;
          }
          if (t.data.cfgData.room0TemperatureSensibility && room0TemperatureSensibility != t.data.cfgData.room0TemperatureSensibility) {
            room0TemperatureSensibility=t.data.cfgData.room0TemperatureSensibility;
            esp_err_t err = write_nvs_integer(room0TemperatureSensibilityTAG, room0TemperatureSensibility);
            ESP_ERROR_CHECK( err );
            updated = true;
          }
          if (t.data.cfgData.thermostatMode && thermostatMode != t.data.cfgData.thermostatMode) {
            thermostatMode=t.data.cfgData.thermostatMode;
            esp_err_t err = write_nvs_integer(thermostatModeTAG, thermostatMode);
            ESP_ERROR_CHECK( err );
            updated = true;
          }

          if (updated) {
            publish_thermostat_cfg();
          }
        }
        if (t.msgType == THERMOSTAT_SENSORS_MSG) {
          waterTemperature = t.data.sensorsData.wtemperature;
          if (waterTemperature > 0) {
            waterTemperatureFlag = SENSOR_LIFETIME;
          }
          circuitTemperature_3 = circuitTemperature_2;
          circuitTemperature_2 = circuitTemperature_1;
          circuitTemperature_1 = circuitTemperature;
          circuitTemperature = t.data.sensorsData.ctemperature;
          if (circuitTemperature > 0) {
            circuitTemperatureFlag = SENSOR_LIFETIME;
          }
        }
        if (t.msgType == THERMOSTAT_ROOM_0_MSG) {
          room0Temperature = t.data.roomData.temperature;
          if (room0Temperature > 0) {
            room0TemperatureFlag = SENSOR_LIFETIME;
          }
        }
        if (t.msgType == THERMOSTAT_LIFE_TICK) {
          thermostatDuration += 1;
          heatingDuration += 1;

          if (room0TemperatureFlag > 0)
            room0TemperatureFlag -= 1;
          if (waterTemperatureFlag > 0)
            waterTemperatureFlag -= 1;
          if (circuitTemperatureFlag > 0)
            circuitTemperatureFlag -= 1;
          ESP_LOGI(TAG, "room0TemperatureFlag: %d, waterTemperatureFlag: %d, circuitTemperatureFlag: %d",
                   room0TemperatureFlag, waterTemperatureFlag, circuitTemperatureFlag);
          update_thermostat();
        }
      }
  }
}

void read_nvs_thermostat_data()
{
  esp_err_t err=read_nvs_integer(circuitTargetTemperatureTAG, &circuitTargetTemperature);
  ESP_ERROR_CHECK( err );

  err=read_nvs_integer(waterTargetTemperatureTAG, &waterTargetTemperature);
  ESP_ERROR_CHECK( err );

  err=read_nvs_integer(waterTemperatureSensibilityTAG, &waterTemperatureSensibility);
  ESP_ERROR_CHECK( err );

  err=read_nvs_integer(room0TargetTemperatureTAG, &room0TargetTemperature);
  ESP_ERROR_CHECK( err );

  err=read_nvs_integer(room0TemperatureSensibilityTAG, &room0TemperatureSensibility);
  ESP_ERROR_CHECK( err );

  err=read_nvs_integer(thermostatModeTAG, (int*) &thermostatMode);
  ESP_ERROR_CHECK( err );
}
#endif // CONFIG_MQTT_THERMOSTAT
