#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "nvs.h"


#include "app_esp8266.h"
#include "app_relay.h"
#include "app_thermostat.h"



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
  /*
  if (xEventGroupGetBits(mqtt_event_group) & INIT_FINISHED_BIT)
    {
      const char * connect_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/thermostat";
      char data[256];
      memset(data,0,256);

      sprintf(data, "{\"targetTemperature\":%02f, \"targetTemperatureSensibility\":%02f}", targetTemperature/10., targetTemperatureSensibility/10.);
      xEventGroupClearBits(mqtt_event_group, PUBLISHED_BIT);
      int msg_id = esp_mqtt_client_publish(client, connect_topic, data,strlen(data), 1, 0);
      if (msg_id > 0) {
        ESP_LOGI(TAG, "sent publish thermostat data successful, msg_id=%d", msg_id);
        EventBits_t bits = xEventGroupWaitBits(mqtt_event_group, PUBLISHED_BIT, false, true, MQTT_FLAG_TIMEOUT);
        if (bits & PUBLISHED_BIT) {
          ESP_LOGI(TAG, "publish ack received, msg_id=%d", msg_id);
        } else {
          ESP_LOGW(TAG, "publish ack not received, msg_id=%d", msg_id);
        }
      } else {
        ESP_LOGI(TAG, "failed to publish thermostat, msg_id=%d", msg_id);
      }
    }
  */
}

esp_err_t write_thermostat_nvs(const char * tag, int value)
{
  nvs_handle my_handle;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    printf("Error (%d) opening NVS handle!\n", err);
  } else {
    printf("Done\n");
    printf("Updating %s in NVS ... ", tag);
    err = nvs_set_i32(my_handle, tag, value);
    printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
    printf("Committing updates in NVS ... ");
    err = nvs_commit(my_handle);
    printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

    // Close
    nvs_close(my_handle);
  }
  return err;
}

esp_err_t read_thermostat_nvs(const char * tag, int * value)
{
  printf("Opening Non-Volatile Storage (NVS) handle... ");
  nvs_handle my_handle;
  esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
  switch (err) {
  case ESP_ERR_NVS_NOT_FOUND:
      printf("The storage is not initialized yet!\n");
      err = ESP_OK;
      break;
  case ESP_OK:
    printf("Done\n");
    printf("Reading %s from NVS ... ", tag);
    err = nvs_get_i32(my_handle, tag, value);
    switch (err) {
    case ESP_OK:
      printf("Done\n");
      printf("%s = %d\n", tag, *value);
      break;
    case ESP_ERR_NVS_NOT_FOUND:
      printf("The value is not initialized yet!\n");
      err = ESP_OK;
      break;
    default :
      printf("Error (%d) reading!\n", err);
    }
    break;
  default:
    printf("Error (%d) opening NVS handle!\n", err);
    break;
  }

  nvs_close(my_handle);
  return err;
}

void updateHeatingState(bool heatEnabled)
{
  if (heatEnabled)
    {
      update_relay_state(0,1);
    }
  else
    {
      update_relay_state(0,0);
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
        updateHeatingState(heatEnabled);
        publish_relay_data(client);

      }
      return;
    }

  if (heatEnabled==true && wtemperature * 10 > targetTemperature + targetTemperatureSensibility)
    {
      heatEnabled=false;
      updateHeatingState(heatEnabled);
      publish_relay_data(client);
    }


  if (heatEnabled==false && wtemperature  * 10 < targetTemperature - targetTemperatureSensibility)
    {
      heatEnabled=true;
      updateHeatingState(heatEnabled);
      publish_relay_data(client);

    }

}

void handle_thermostat_cmd_task(void* pvParameters)
{/*
  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;
  struct ThermostatMessage t;
  while(1) {
    if( xQueueReceive( thermostatQueue, &t , portMAX_DELAY) )
      {
        if (t.targetTemperature && targetTemperature != t.targetTemperature * 10) {
          targetTemperature=t.targetTemperature*10;
          esp_err_t err = write_thermostat_nvs(targetTemperatureTAG, targetTemperature);
          ESP_ERROR_CHECK( err );
          update_thermostat(client);
        }
        if (t.targetTemperatureSensibility && targetTemperatureSensibility != t.targetTemperatureSensibility * 10) {
          targetTemperatureSensibility=t.targetTemperatureSensibility*10;
          esp_err_t err = write_thermostat_nvs(targetTemperatureSensibilityTAG, targetTemperatureSensibility);
          ESP_ERROR_CHECK( err );
          update_thermostat(client);
        }
      }
  }
 */
}
