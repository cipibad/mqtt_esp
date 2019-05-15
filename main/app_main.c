#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "rom/gpio.h"

#include "app_main.h"

#include "app_wifi.h"
#include "app_mqtt.h"
#include "app_nvs.h"

#if CONFIG_MQTT_SWITCHES_NB
#include "app_switch.h"
#endif //CONFIG_MQTT_SWITCHES_NB

#ifdef CONFIG_MQTT_SENSOR
#include "app_sensors.h"
#endif //CONFIG_MQTT_SENSOR

#ifdef CONFIG_MQTT_THERMOSTAT
#include "app_thermostat.h"
QueueHandle_t thermostatQueue;
extern int targetTemperature;
extern int targetTemperatureSensibility;
extern const char * targetTemperatureTAG;
extern const char * targetTemperatureSensibilityTAG;
#endif // CONFIG_MQTT_THERMOSTAT

#if CONFIG_MQTT_RELAYS_NB
#include "app_relay.h"
QueueHandle_t relayQueue;
#endif//CONFIG_MQTT_RELAYS_NB

#ifdef CONFIG_MQTT_OTA
#include "app_ota.h"
QueueHandle_t otaQueue;
#endif //CONFIG_MQTT_OTA

#ifdef CONFIG_MQTT_OPS
#include "app_ops.h"
#endif // CONFIG_MQTT_OPS

#include "app_smart_config.h"
QueueHandle_t smartconfigQueue;

extern EventGroupHandle_t wifi_event_group;
extern const int WIFI_CONNECTED_BIT;

extern EventGroupHandle_t mqtt_event_group;
extern const int MQTT_CONNECTED_BIT;

extern const char * smartconfigTAG;
extern int smartconfigFlag;

QueueHandle_t mqttQueue;

static const char *TAG = "MQTT(S?)_MAIN";

void reboot_in_5_minutes_task(void *pvParameter)
{
  ESP_LOGI(TAG, "Prepare to restart system in 5 minutes!");
  vTaskDelay(1000 * 60 * 5 - 10000 / portTICK_PERIOD_MS);
  ESP_LOGI(TAG, "Prepare to restart system in 10 seconds!");
  vTaskDelay(10000 / portTICK_PERIOD_MS);
  esp_restart();

}

void blink_task(void *pvParameter)
{

  gpio_pad_select_gpio(CONFIG_MQTT_STATUS_LED_GPIO);
  gpio_set_direction(CONFIG_MQTT_STATUS_LED_GPIO, GPIO_MODE_OUTPUT);

  int interval;
  while(1) {
    EventBits_t bits;
    if(smartconfigFlag) {
      interval=150;
    } else {
      interval=250;
      bits = xEventGroupGetBits(wifi_event_group);
      if( ( bits & WIFI_CONNECTED_BIT ) != 0 ) {
        interval=500;
      }
    }
    gpio_set_level(CONFIG_MQTT_STATUS_LED_GPIO, LED_ON);

    bits = xEventGroupGetBits(mqtt_event_group);
    while ( bits & MQTT_CONNECTED_BIT ) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      bits = xEventGroupGetBits(mqtt_event_group);
    }
    vTaskDelay(interval / portTICK_PERIOD_MS);
    gpio_set_level(CONFIG_MQTT_STATUS_LED_GPIO, LED_OFF);
    vTaskDelay(interval / portTICK_PERIOD_MS);
  }
}

void app_main(void)
{
  ESP_LOGI(TAG, "[APP] Startup..");
  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
  esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);


  mqtt_event_group = xEventGroupCreate();
  wifi_event_group = xEventGroupCreate();

#ifdef CONFIG_MQTT_THERMOSTAT
  thermostatQueue = xQueueCreate(1, sizeof(struct ThermostatMessage) );
#endif // CONFIG_MQTT_THERMOSTAT
#if CONFIG_MQTT_RELAYS_NB
  relayQueue = xQueueCreate(32, sizeof(struct RelayMessage) );
  relays_init();
#endif //CONFIG_MQTT_RELAYS_NB


#ifdef CONFIG_MQTT_OTA
  otaQueue = xQueueCreate(1, sizeof(struct OtaMessage) );
#endif //CONFIG_MQTT_OTA
  mqttQueue = xQueueCreate(1, sizeof(void *) );

  xTaskCreate(blink_task, "blink_task", configMINIMAL_STACK_SIZE * 3, NULL, 3, NULL);

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK( err );

  ESP_LOGI(TAG, "nvs_flash_init done");

#ifdef CONFIG_MQTT_THERMOSTAT
  err=read_nvs_integer(targetTemperatureTAG, &targetTemperature);
  ESP_ERROR_CHECK( err );

  err=read_nvs_integer(targetTemperatureSensibilityTAG, &targetTemperatureSensibility);
  ESP_ERROR_CHECK( err );
#endif // CONFIG_MQTT_THERMOSTAT


  smartconfigQueue = xQueueCreate(1, sizeof(int) );
  err=read_nvs_integer(smartconfigTAG, &smartconfigFlag);
  ESP_ERROR_CHECK( err );

  xTaskCreate(smartconfig_cmd_task, "smartconfig_cmd_task", configMINIMAL_STACK_SIZE * 3, (void *)NULL, 5, NULL);

  if (smartconfigFlag) {
    xTaskCreate(blink_task, "blink_task", configMINIMAL_STACK_SIZE * 3, NULL, 3, NULL);
    ESP_ERROR_CHECK(write_nvs_integer(smartconfigTAG, ! smartconfigFlag));
  } else {

    esp_mqtt_client_handle_t client = mqtt_init();

#ifdef CONFIG_MQTT_SENSOR
    xTaskCreate(sensors_read, "sensors_read", configMINIMAL_STACK_SIZE * 3, (void *)client, 10, NULL);
#endif //CONFIG_MQTT_SENSOR


#if CONFIG_MQTT_RELAYS_NB
    xTaskCreate(handle_relay_cmd_task, "handle_relay_cmd_task", configMINIMAL_STACK_SIZE * 3, (void *)client, 5, NULL);
#endif //CONFIG_MQTT_RELAYS_NB

#if CONFIG_MQTT_SWITCHES_NB
    gpio_switch_init(NULL);
#endif //CONFIG_MQTT_SWITCHES_NB

#ifdef CONFIG_MQTT_OTA
   xTaskCreate(handle_ota_update_task, "handle_ota_update_task", configMINIMAL_STACK_SIZE * 7, (void *)client, 5, NULL);
#endif //CONFIG_MQTT_OTA

#ifdef CONFIG_MQTT_THERMOSTAT
  xTaskCreate(handle_thermostat_cmd_task, "handle_thermostat_cmd_task", configMINIMAL_STACK_SIZE * 3, (void *)client, 5, NULL);
#endif // CONFIG_MQTT_THERMOSTAT
    xTaskCreate(handle_mqtt_sub_pub, "handle_mqtt_sub_pub", configMINIMAL_STACK_SIZE * 3, (void *)client, 5, NULL);

    wifi_init();
    mqtt_start(client);

#ifdef CONFIG_MQTT_OPS
    xTaskCreate(ops_pub_task, "ops_pub_task", configMINIMAL_STACK_SIZE * 5, (void *)client, 5, NULL);
#endif // CONFIG_MQTT_OPS

  }
}
