#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include <string.h>

#include "app_esp8266.h"
#include "app_relay.h"
#include "app_thermostat.h"
#include "app_nvs.h"

bool heatEnabled = false;
int targetTemperature=23*10; //30 degrees
int targetTemperatureSensibility=5; //0.5 degrees

const char * targetTemperatureTAG="targetTemp";
const char * targetTemperatureSensibilityTAG="tgtTempSens";

extern float wtemperature;
extern EventGroupHandle_t mqtt_event_group;
extern const int PUBLISHED_BIT;
extern const int INIT_FINISHED_BIT;
extern QueueHandle_t thermostatQueue;


static const char *TAG = "APP_THERMOSTAT";

void publish_thermostat_data(MQTTClient* client)
{
  if (xEventGroupGetBits(mqtt_event_group) & INIT_FINISHED_BIT)
    {
      const char * thermostat_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/thermostat";
      char data[256];
      memset(data,0,256);

      sprintf(data, "{\"targetTemperature\":%02f, \"targetTemperatureSensibility\":%02f}", targetTemperature/10., targetTemperatureSensibility/10.);

      MQTTMessage message;
      message.qos = QOS1;
      message.retained = 1;
      message.payload = data;
      message.payloadlen = strlen(data);

      int rc = MQTTPublish(client, thermostat_topic, &message);
      if (rc == 0) {
        ESP_LOGI(TAG, "sent publish thermostat successful, rc=%d", rc);
      } else {
        ESP_LOGI(TAG, "failed to publish thermostat, rc=%d", rc);
      }
    }
}


void updateHeatingState(bool heatEnabled, MQTTClient* client)
{
  if (heatEnabled)
    {
      update_relay_state(0,1, client);
    }
  else
    {
      update_relay_state(0,0, client);
    }

  ESP_LOGI(TAG, "heat state updated to %d", heatEnabled);
}


void update_thermostat(MQTTClient* client)
{

  ESP_LOGI(TAG, "heat state is %d", heatEnabled);
  ESP_LOGI(TAG, "wtemperature is %f", wtemperature);
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

  if (heatEnabled==true && wtemperature * 10 > targetTemperature + targetTemperatureSensibility)
    {
      heatEnabled=false;
      updateHeatingState(heatEnabled, client);
    }


  if (heatEnabled==false && wtemperature  * 10 < targetTemperature - targetTemperatureSensibility)
    {
      heatEnabled=true;
      updateHeatingState(heatEnabled, client);
    }

}

void handle_thermostat_cmd_task(void* pvParameters)
{
  MQTTClient* client = (MQTTClient*) pvParameters;
  //esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;
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
