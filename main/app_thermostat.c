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
bool heatingEnabled2 = false;

int columnTargetTemperature=23*10; //30 degrees
int waterTargetTemperature=23*10; //30 degrees
int waterTemperatureSensibility=5; //0.5 degrees

int room0TargetTemperature=22*10;
int room0TemperatureSensibility=2;

const char * columnTargetTemperatureTAG="ctgtTemp";
const char * waterTargetTemperatureTAG="wTargetTemp";
const char * waterTemperatureSensibilityTAG="wTempSens";
const char * room0TargetTemperatureTAG="r0targetTemp";
const char * room0TemperatureSensibilityTAG="r0TempSens";

int32_t room0Temperature = 0;
int32_t wtemperature = 0;
int32_t ctemperature = 0;
int32_t ctemperature_1 = 0;
int32_t ctemperature_2 = 0;
int32_t ctemperature_3 = 0;
extern EventGroupHandle_t mqtt_event_group;
extern const int MQTT_INIT_FINISHED_BIT;
extern const int MQTT_PUBLISHED_BIT;
extern QueueHandle_t thermostatQueue;


static const char *TAG = "APP_THERMOSTAT";

void publish_thermostat_cfg()
{

  const char * topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/thermostat/cfg";

  char data[256];
  memset(data,0,256);

  sprintf(data, "{\"columnTargetTemperature\":%d.%01d, \"waterTargetTemperature\":%d.%01d, \"waterTemperatureSensibility\":%d.%01d, \"room0TargetTemperature\":%d.%01d, \"room0TemperatureSensibility\":%d.%01d}",
          columnTargetTemperature/10, columnTargetTemperature%10,
          waterTargetTemperature/10, waterTargetTemperature%10,
          waterTemperatureSensibility/10, waterTemperatureSensibility%10,
          room0TargetTemperature/10,room0TargetTemperature%10,
          room0TemperatureSensibility/10, room0TemperatureSensibility%10);
  xEventGroupClearBits(mqtt_event_group, MQTT_PUBLISHED_BIT);

  mqtt_publish_data(topic, data, QOS_1, RETAIN);
}

void publish_thermostat_state()
{
  const char * topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/thermostat/state";
  char data[256];
  memset(data,0,256);

  sprintf(data, "{\"thermostatState\":%d, \"heatingState\":%d, \"heatingState2\":%d}",
          thermostatEnabled, heatingEnabled, heatingEnabled2);

  mqtt_publish_data(topic, data, QOS_1, RETAIN);

}

void publish_thermostat_data()
{
  publish_thermostat_state();
  publish_thermostat_cfg();
}


void disableThermostat()
{
  publish_thermostat_state();
  thermostatEnabled=false;
  update_relay_state(CONFIG_MQTT_THERMOSTAT_RELAY_ID, 0);
  publish_thermostat_state();
  ESP_LOGI(TAG, "thermostat disabled");
}

void enableThermostat()
{
  publish_thermostat_state();
  thermostatEnabled=true;
  update_relay_state(CONFIG_MQTT_THERMOSTAT_RELAY_ID, 1);
  publish_thermostat_state();
  ESP_LOGI(TAG, "thermostat enabled");
}


void update_thermostat()
{
  ESP_LOGI(TAG, "heatingEnabled state is %d", heatingEnabled);
  ESP_LOGI(TAG, "heatingEnabled2 state is %d", heatingEnabled2);
  ESP_LOGI(TAG, "ctemperature_n is %d", ctemperature);
  ESP_LOGI(TAG, "ctemperature_n_1 is %d", ctemperature_1);
  ESP_LOGI(TAG, "ctemperature_n_2 is %d", ctemperature_2);
  ESP_LOGI(TAG, "ctemperature_n_3 is %d", ctemperature_3);
  ESP_LOGI(TAG, "columnTargetTemperature is %d", columnTargetTemperature);
  ESP_LOGI(TAG, "thermostat state is %d", thermostatEnabled);
  ESP_LOGI(TAG, "wtemperature is %d", wtemperature);
  ESP_LOGI(TAG, "waterTargetTemperature is %d", waterTargetTemperature);
  ESP_LOGI(TAG, "waterTemperatureSensibility is %d", waterTemperatureSensibility);
  ESP_LOGI(TAG, "room0Temperature is %d", room0Temperature);
  ESP_LOGI(TAG, "room0TargetTemperature is %d", room0TargetTemperature);
  ESP_LOGI(TAG, "room0TemperatureSensibility is %d", room0TemperatureSensibility);
  if ( wtemperature == 0 && room0Temperature == 0 )
    {
      ESP_LOGI(TAG, "no sensor is reporting => no thermostat handling");
      if (thermostatEnabled==true) {
        ESP_LOGI(TAG, "stop thermostat as no sensor is reporting");
        disableThermostat();
      }
      return;
    }

  if (thermostatEnabled==false &&
      (
       (wtemperature > 0 &&
        wtemperature < waterTargetTemperature - waterTemperatureSensibility) ||
       (room0Temperature > 0 &&
        room0Temperature < room0TargetTemperature - room0TemperatureSensibility)
       )&&
      ctemperature < columnTargetTemperature)
    {
      enableThermostat();
    }

  if (ctemperature_2 && ctemperature_1 && ctemperature) {//three consecutive valid readings with 2 difference
    if (thermostatEnabled && !heatingEnabled
        && ((ctemperature_2 + 1) < ctemperature_1
            && ctemperature_1 < (ctemperature - 1))){ //water is heating 1 3 5
      publish_thermostat_state();
      heatingEnabled = true;
      ESP_LOGI(TAG, "heating enabled");
      publish_thermostat_state();
    }

    if (heatingEnabled
        && ( (ctemperature_2 + 1) >= ctemperature_1
             || ctemperature_1 >= (ctemperature - 1 ))) { //heating is disabled   1 3 4
      publish_thermostat_state();
      heatingEnabled = false;
      ESP_LOGI(TAG, "heating disabled");
      publish_thermostat_state();
    }
  }


  if (ctemperature_3 && ctemperature_2 && ctemperature_1 && ctemperature) {//four consecutive valid readings
    if (thermostatEnabled && !heatingEnabled2
        && ( ctemperature_3 < ctemperature_2
             && ctemperature_2 < ctemperature_1
             && ctemperature_1 < ctemperature )){ //water is heating 1 2 3 4
      publish_thermostat_state();
      heatingEnabled2 = true;
      ESP_LOGI(TAG, "heating2 enabled");
      publish_thermostat_state();
    }

    if (heatingEnabled2
        && ( ctemperature_3 >= ctemperature_2
             || ctemperature_2 >= ctemperature_1
             || ctemperature_1 >= ctemperature )) { //heating is disabled   5 4 3 2
      publish_thermostat_state();
      heatingEnabled2 = false;
      ESP_LOGI(TAG, "heating2 disabled");
      publish_thermostat_state();
      ESP_LOGI(TAG, "thermostat disabled due to heating2 disabled");
      disableThermostat();
    }
  }


  ctemperature_3 = ctemperature_2;
  ctemperature_2 = ctemperature_1;
  ctemperature_1 = ctemperature;
}

void handle_thermostat_cmd_task(void* pvParameters)
{
  struct ThermostatMessage t;
  while(1) {
    if( xQueueReceive( thermostatQueue, &t , portMAX_DELAY) )
      {
        if (t.msgType == THERMOSTAT_CFG_MSG) {
          bool updated = false;
          if (t.data.cfgData.columnTargetTemperature && columnTargetTemperature != t.data.cfgData.columnTargetTemperature) {
            columnTargetTemperature=t.data.cfgData.columnTargetTemperature;
            esp_err_t err = write_nvs_integer(columnTargetTemperatureTAG, columnTargetTemperature);
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
          if (updated) {
            update_thermostat();
            publish_thermostat_cfg();
          }
        }
        if (t.msgType == THERMOSTAT_SENSORS_MSG) {
          wtemperature = t.data.sensorsData.wtemperature;
          ctemperature = t.data.sensorsData.ctemperature;
          update_thermostat();
        }
        if (t.msgType == THERMOSTAT_ROOM_0_MSG) {
          room0Temperature = t.data.roomData.temperature;
          update_thermostat();
        }
      }
  }
}

void read_nvs_thermostat_data()
{
  esp_err_t err=read_nvs_integer(columnTargetTemperatureTAG, &columnTargetTemperature);
  ESP_ERROR_CHECK( err );

  err=read_nvs_integer(waterTargetTemperatureTAG, &waterTargetTemperature);
  ESP_ERROR_CHECK( err );

  err=read_nvs_integer(waterTemperatureSensibilityTAG, &waterTemperatureSensibility);
  ESP_ERROR_CHECK( err );

  err=read_nvs_integer(room0TargetTemperatureTAG, &room0TargetTemperature);
  ESP_ERROR_CHECK( err );

  err=read_nvs_integer(room0TemperatureSensibilityTAG, &room0TemperatureSensibility);
  ESP_ERROR_CHECK( err );
}
#endif // CONFIG_MQTT_THERMOSTAT
