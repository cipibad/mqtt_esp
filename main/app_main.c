#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

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

#if CONFIG_MQTT_THERMOSTATS_NB > 0
#include "app_thermostat.h"
QueueHandle_t thermostatQueue;
#endif // CONFIG_MQTT_THERMOSTATS_NB > 0

#if CONFIG_MQTT_RELAYS_NB
#include "app_relay.h"
QueueHandle_t relayQueue;
#endif//CONFIG_MQTT_RELAYS_NB

#ifdef CONFIG_MQTT_SCHEDULERS
#include "app_scheduler.h"
QueueHandle_t schedulerCfgQueue;
#endif // CONFIG_MQTT_SCHEDULERS

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

SemaphoreHandle_t xSemaphore;

static const char *TAG = "MQTT(S?)_MAIN";

void reboot_in_5_minutes_task(void *pvParameter)
{
  ESP_LOGI(TAG, "Prepare to restart system in 5 minutes!");
  vTaskDelay((1000 * 60 * 5 - 10000) / portTICK_PERIOD_MS);
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


  //otherwise will crash due to ESP_LOG in GPIO function
  // that is called in critical section from DS18X20 driver
  esp_log_level_set("gpio", ESP_LOG_WARN);

  mqtt_event_group = xEventGroupCreate();
  wifi_event_group = xEventGroupCreate();

#if CONFIG_MQTT_THERMOSTATS_NB > 0
  thermostatQueue = xQueueCreate(3, sizeof(struct ThermostatMessage) );
#endif // CONFIG_MQTT_THERMOSTATS_NB > 0

#if CONFIG_MQTT_RELAYS_NB
  relayQueue = xQueueCreate(32, sizeof(struct RelayMessage) );
#endif //CONFIG_MQTT_RELAYS_NB

#ifdef CONFIG_MQTT_SCHEDULERS
  schedulerCfgQueue = xQueueCreate(8, sizeof(struct SchedulerCfgMessage) );
#endif // CONFIG_MQTT_SCHEDULERS


#ifdef CONFIG_MQTT_OTA
  otaQueue = xQueueCreate(1, sizeof(struct OtaMessage) );
#endif //CONFIG_MQTT_OTA
  mqttQueue = xQueueCreate(1, sizeof(void *) );
  xSemaphore = xSemaphoreCreateMutex();

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

#if CONFIG_MQTT_RELAYS_NB
  relays_init();
#endif // CONFIG_MQTT_RELAYS_NB

#if CONFIG_MQTT_THERMOSTATS_NB > 0
  read_nvs_thermostat_data();
#endif // CONFIG_MQTT_THERMOSTATS_NB > 0


  smartconfigQueue = xQueueCreate(3, sizeof(struct SmartConfigMessage) );
  err=read_nvs_integer(smartconfigTAG, &smartconfigFlag);
  ESP_ERROR_CHECK( err );

  xTaskCreate(smartconfig_cmd_task, "smartconfig_cmd_task", 4096, (void *)NULL, 5, NULL);

  if (smartconfigFlag) {
    xTaskCreate(reboot_in_5_minutes_task, "reboot_in_5_minutes_task", configMINIMAL_STACK_SIZE * 3, NULL, 3, NULL);
    ESP_ERROR_CHECK(write_nvs_integer(smartconfigTAG, ! smartconfigFlag));
  } else {

#ifdef CONFIG_MQTT_SENSOR
    xTaskCreate(sensors_read, "sensors_read", configMINIMAL_STACK_SIZE * 9, NULL, 10, NULL);
#endif //CONFIG_MQTT_SENSOR


#if CONFIG_MQTT_RELAYS_NB
    xTaskCreate(handle_relay_task, "handle_relay_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
#endif //CONFIG_MQTT_RELAYS_NB

#if CONFIG_MQTT_SWITCHES_NB
    gpio_switch_init(NULL);
#endif //CONFIG_MQTT_SWITCHES_NB

#ifdef CONFIG_MQTT_OTA
   xTaskCreate(handle_ota_update_task, "handle_ota_update_task", configMINIMAL_STACK_SIZE * 7, NULL, 5, NULL);
#endif //CONFIG_MQTT_OTA

#if CONFIG_MQTT_THERMOSTATS_NB > 0
  xTaskCreate(handle_thermostat_cmd_task, "handle_thermostat_cmd_task", configMINIMAL_STACK_SIZE * 9, NULL, 5, NULL);
#endif // CONFIG_MQTT_THERMOSTATS_NB > 0
    xTaskCreate(handle_mqtt_sub_pub, "handle_mqtt_sub_pub", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);

    wifi_init();
    mqtt_init_and_start();

#ifdef CONFIG_MQTT_OPS
    xTaskCreate(ops_pub_task, "ops_pub_task", configMINIMAL_STACK_SIZE * 2, NULL, 5, NULL);
#endif // CONFIG_MQTT_OPS

#ifdef CONFIG_MQTT_SCHEDULERS
    xTaskCreate(handle_scheduler, "handle_scheduler", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
#endif // CONFIG_MQTT_SCHEDULERS

  }
}
