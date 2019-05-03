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

bool heatEnabled = false;
int targetTemperature=23*10; //30 degrees
int targetTemperatureSensibility=5; //0.5 degrees

const char * targetTemperatureTAG="targetTemp";
const char * targetTemperatureSensibilityTAG="tgtTempSens";

extern int32_t wtemperature;
extern EventGroupHandle_t mqtt_event_group;
extern const int MQTT_INIT_FINISHED_BIT;
extern const int MQTT_PUBLISHED_BIT;
extern QueueHandle_t thermostatQueue;


static const char *TAG = "APP_THERMOSTAT";

void publish_thermostat_data(esp_mqtt_client_handle_t client)
{
  if (xEventGroupGetBits(mqtt_event_group) & MQTT_INIT_FINISHED_BIT)
    {
      const char * connect_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/thermostat";
      char data[256];
      memset(data,0,256);

      sprintf(data, "{\"targetTemperature\":%02f, \"targetTemperatureSensibility\":%02f}", targetTemperature/10., targetTemperatureSensibility/10.);
      xEventGroupClearBits(mqtt_event_group, MQTT_PUBLISHED_BIT);
      int msg_id = esp_mqtt_client_publish(client, connect_topic, data,strlen(data), 1, 0);
      if (msg_id > 0) {
        ESP_LOGI(TAG, "sent publish thermostat data successful, msg_id=%d", msg_id);
        EventBits_t bits = xEventGroupWaitBits(mqtt_event_group, MQTT_PUBLISHED_BIT, false, true, MQTT_FLAG_TIMEOUT);
        if (bits & MQTT_PUBLISHED_BIT) {
          ESP_LOGI(TAG, "publish ack received, msg_id=%d", msg_id);
        } else {
          ESP_LOGW(TAG, "publish ack not received, msg_id=%d", msg_id);
        }
      } else {
        ESP_LOGI(TAG, "failed to publish thermostat, msg_id=%d", msg_id);
      }
    }
}

void updateHeatingState(bool heatEnabled, esp_mqtt_client_handle_t client)
{
  if (heatEnabled)
    {
      update_relay_state(CONFIG_MQTT_THERMOSTAT_RELAY_ID,1, client);
    }
  else
    {
      update_relay_state(CONFIG_MQTT_THERMOSTAT_RELAY_ID,0, client);
    }

  ESP_LOGI(TAG, "heat state updated to %d", heatEnabled);
}


void update_thermostat(esp_mqtt_client_handle_t client)
{

  ESP_LOGI(TAG, "heat state is %d", heatEnabled);
  ESP_LOGI(TAG, "wtemperature is %d", wtemperature);
  ESP_LOGI(TAG, "targetTemperature is %d", targetTemperature);
  ESP_LOGI(TAG, "targetTemperatureSensibility is %d", targetTemperatureSensibility);
  if ( wtemperature == 0 )
    {
      ESP_LOGI(TAG, "sensor is not reporting, no thermostat handling");
      if (heatEnabled==true) {
        ESP_LOGI(TAG, "stop heating as sensor is not reporting");
        heatEnabled=false;
        updateHeatingState(heatEnabled, client);
      }
      return;
    }

  if (heatEnabled==true && wtemperature > targetTemperature + targetTemperatureSensibility)
    {
      heatEnabled=false;
      updateHeatingState(heatEnabled, client);
    }


  if (heatEnabled==false && wtemperature < targetTemperature - targetTemperatureSensibility)
    {
      heatEnabled=true;
      updateHeatingState(heatEnabled, client);
    }

}

void handle_thermostat_cmd_task(void* pvParameters)
{
  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;
  struct ThermostatMessage t;
  while(1) {
    if( xQueueReceive( thermostatQueue, &t , portMAX_DELAY) )
      {
        if (t.targetTemperature && targetTemperature != t.targetTemperature * 10) {
          targetTemperature=t.targetTemperature*10;
          esp_err_t err = write_nvs_integer(targetTemperatureTAG, targetTemperature);
          ESP_ERROR_CHECK( err );
          update_thermostat(client);
        }
        if (t.targetTemperatureSensibility && targetTemperatureSensibility != t.targetTemperatureSensibility * 10) {
          targetTemperatureSensibility=t.targetTemperatureSensibility*10;
          esp_err_t err = write_nvs_integer(targetTemperatureSensibilityTAG, targetTemperatureSensibility);
          ESP_ERROR_CHECK( err );
          update_thermostat(client);
        }
      }
  }
}
#endif // CONFIG_MQTT_THERMOSTAT
