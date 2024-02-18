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
#include "app_nvs.h"

#ifdef CONFIG_NORTH_INTERFACE_MQTT
#include "app_mqtt.h"

extern EventGroupHandle_t mqtt_event_group;
extern const int MQTT_CONNECTED_BIT;

QueueHandle_t mqttQueue;
SemaphoreHandle_t xSemaphore;
#endif // CONFIG_NORTH_INTERFACE_MQTT

#if CONFIG_MQTT_SWITCHES_NB
#include "app_switch.h"
#endif //CONFIG_MQTT_SWITCHES_NB

#ifdef CONFIG_SENSOR_SUPPORT
#include "app_sensors.h"
#endif //CONFIG_SENSOR_SUPPORT

#if CONFIG_MQTT_THERMOSTATS_NB > 0
#include "app_thermostat.h"
QueueHandle_t thermostatQueue;
#endif // CONFIG_MQTT_THERMOSTATS_NB > 0

#include "app_waterpump.h"

#if CONFIG_MQTT_RELAYS_NB
#include "app_relay.h"
QueueHandle_t relayQueue;
#endif//CONFIG_MQTT_RELAYS_NB

#ifdef CONFIG_MQTT_SCHEDULERS
#include "app_scheduler.h"
QueueHandle_t schedulerCfgQueue;
#endif // CONFIG_MQTT_SCHEDULERS

#ifdef CONFIG_MOTION_SENSOR_SUPPORT
#include "app_motion.h"
#endif // CONFIG_MOTION_SENSOR_SUPPORT

#ifdef CONFIG_PRESENCE_AUTOMATION_SUPPORT
#include "app_presence.h"
#endif // CONFIG_PRESENCE_AUTOMATION_SUPPORT

#ifdef CONFIG_MQTT_OTA
#include "app_ota.h"
QueueHandle_t otaQueue;
#endif //CONFIG_MQTT_OTA

#ifdef CONFIG_MQTT_OPS
#include "app_ops.h"
#endif // CONFIG_MQTT_OPS

#ifdef CONFIG_COAP_SERVER_SUPPORT
#include "app_coap_server.h"
#endif // CONFIG_COAP_SERVER_SUPPORT

#ifdef CONFIG_NORTH_INTERFACE_COAP
#include "app_coap_client.h"

QueueHandle_t coapClientQueue;
#endif // CONFIG_NORTH_INTERFACE_COAP

#include "app_smart_config.h"
QueueHandle_t smartconfigQueue;

extern EventGroupHandle_t wifi_event_group;
extern const int WIFI_CONNECTED_BIT;

extern const char * smartconfigTAG;
extern int smartconfigFlag;

static const char *TAG = "MQTT(S?)_MAIN";

void restart_in_3_minutes_task(void *pvParameter)
{
  ESP_LOGI(TAG, "Prepare to esp board in 3 minutes!");
  vTaskDelay((3 * 60 * 1000 - 10 * 1000) / portTICK_PERIOD_MS);
  ESP_LOGI(TAG, "Prepare to restart system in 10 seconds!");
  vTaskDelay(10 * 1000 / portTICK_PERIOD_MS);
  esp_restart();
}

#ifdef CONFIG_STATUS_LED
void blink_task(void *pvParameter)
{
  gpio_pad_select_gpio(CONFIG_STATUS_LED_GPIO);
  gpio_set_direction(CONFIG_STATUS_LED_GPIO, GPIO_MODE_OUTPUT);

  int interval;
  EventBits_t bits;
  while(1) {

    if(smartconfigFlag) {
      interval=150;
    } else {
      interval=250;
      bits = xEventGroupGetBits(wifi_event_group);
      if(bits & WIFI_CONNECTED_BIT) {
        interval=500;
      }
    }
    gpio_set_level(CONFIG_STATUS_LED_GPIO, LED_ON);

#ifdef CONFIG_NORTH_INTERFACE_MQTT
    bits = xEventGroupGetBits(mqtt_event_group);
    while ( bits & MQTT_CONNECTED_BIT ) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      bits = xEventGroupGetBits(mqtt_event_group);
    }
#endif // CONFIG_NORTH_INTERFACE_MQTT

#ifdef CONFIG_NORTH_INTERFACE_COAP
  while(coap_connection_status() == COAP_LAST_MESSAGE_PASSED){
    bits = xEventGroupGetBits(wifi_event_group);
    if( !(wifi_bits & WIFI_CONNECTED_BIT)) {
      break;
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
#endif // CONFIG_NORTH_INTERFACE_COAP

    vTaskDelay(interval / portTICK_PERIOD_MS);
    gpio_set_level(CONFIG_STATUS_LED_GPIO, LED_OFF);
    vTaskDelay(interval / portTICK_PERIOD_MS);
  }
}

#endif // CONFIG_STATUS_LED
void app_main(void)
{
  ESP_LOGI(TAG, "[APP] Startup..");
  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
  ESP_LOGI(TAG, "[APP] Client ID: " CONFIG_CLIENT_ID);

  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  ESP_LOGI(TAG, "[APP] MAC Address: " MACSTR, MAC2STR(mac) );

  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
  esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
  esp_log_level_set("CoAP_client", ESP_LOG_VERBOSE);


  //otherwise will crash due to ESP_LOG in GPIO function
  // that is called in critical section from DS18X20 driver
  esp_log_level_set("gpio", ESP_LOG_WARN);

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

#ifdef CONFIG_NORTH_INTERFACE_MQTT
  mqtt_event_group = xEventGroupCreate();
  mqttQueue = xQueueCreate(1, sizeof(void *) );
  xSemaphore = xSemaphoreCreateMutex();
#endif // CONFIG_NORTH_INTERFACE_MQTT

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

#ifdef CONFIG_NORTH_INTERFACE_COAP
  coapClientQueue = xQueueCreate(4, sizeof(struct CoapMessage) );
#endif // CONFIG_NORTH_INTERFACE_COAP

#ifdef CONFIG_STATUS_LED
  xTaskCreate(blink_task, "blink_task", configMINIMAL_STACK_SIZE * 3, NULL, 3, NULL);
#endif // CONFIG_STATUS_LED

#if CONFIG_MQTT_THERMOSTATS_NB > 0
  read_nvs_thermostat_data();
#endif // CONFIG_MQTT_THERMOSTATS_NB > 0


  smartconfigQueue = xQueueCreate(3, sizeof(struct SmartConfigMessage) );
  err=read_nvs_integer(smartconfigTAG, &smartconfigFlag);
  ESP_ERROR_CHECK( err );

  xTaskCreate(smartconfig_cmd_task, "smartconfig_cmd_task", 4096, (void *)NULL, 5, NULL);

  if (smartconfigFlag) {
    xTaskCreate(restart_in_3_minutes_task, "reboot_in_5_minutes_task", configMINIMAL_STACK_SIZE * 3, NULL, 3, NULL);
    ESP_ERROR_CHECK(write_nvs_integer(smartconfigTAG, ! smartconfigFlag));
  } else {

#ifdef CONFIG_SENSOR_SUPPORT
  #ifdef CONFIG_TARGET_DEVICE_ESP32
    #define SENSORS_READ_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 9)
  #endif //CONFIG_TARGET_DEVICE_ESP32
  #ifdef CONFIG_TARGET_DEVICE_ESP8266
    #define SENSORS_READ_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)
  #endif //CONFIG_TARGET_DEVICE_ESP8266

    xTaskCreate(sensors_read, "sensors_read", SENSORS_READ_TASK_STACK_SIZE, NULL, 10, NULL);
#endif //CONFIG_SENSOR_SUPPORT

#if CONFIG_MQTT_RELAYS_NB
    xTaskCreate(handle_relay_task, "handle_relay_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
#endif //CONFIG_MQTT_RELAYS_NB

#ifdef CONFIG_MQTT_OTA
   xTaskCreate(handle_ota_update_task, "handle_ota_update_task", configMINIMAL_STACK_SIZE * 7, NULL, 5, NULL);
#endif //CONFIG_MQTT_OTA

#if CONFIG_MQTT_THERMOSTATS_NB > 0
  #ifdef CONFIG_TARGET_DEVICE_ESP32
    #define THERMOSTAT_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 9)
  #endif //CONFIG_TARGET_DEVICE_ESP32
  #ifdef CONFIG_TARGET_DEVICE_ESP8266
    #define THERMOSTAT_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)
  #endif //CONFIG_TARGET_DEVICE_ESP8266

  xTaskCreate(handle_thermostat_cmd_task, "handle_thermostat_cmd_task", THERMOSTAT_TASK_STACK_SIZE, NULL, 5, NULL);
#endif // CONFIG_MQTT_THERMOSTATS_NB > 0

#ifdef CONFIG_WATERPUMP_SUPPORT
initWaterPump();
#endif // CONFIG_WATERPUMP_SUPPORT

#if CONFIG_MQTT_SWITCHES_NB
    gpio_switch_init(NULL);
#endif //CONFIG_MQTT_SWITCHES_NB

#ifdef CONFIG_MQTT_SCHEDULERS
    xTaskCreate(handle_scheduler, "handle_scheduler", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
#endif // CONFIG_MQTT_SCHEDULERS

#ifdef CONFIG_PRESENCE_AUTOMATION_SUPPORT
    xTaskCreate(app_presence_task, "app_presence", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
#endif // CONFIG_PRESENCE_AUTOMATION_SUPPORT

#ifdef CONFIG_MOTION_SENSOR_SUPPORT
    xTaskCreate(app_motion_task, "app_motion", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
#endif // CONFIG_MOTION_SENSOR_SUPPORT

    wifi_init();

#ifdef CONFIG_COAP_SERVER_SUPPORT
    xTaskCreate(coap_server_thread, "coap_server", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
#endif // CONFIG_COAP_SERVER_SUPPORT

#ifdef CONFIG_NORTH_INTERFACE_COAP
    xTaskCreate(coap_client_thread, "coap_client", configMINIMAL_STACK_SIZE * 4, NULL, 5, NULL);
#endif // CONFIG_NORTH_INTERFACE_COAP

#ifdef CONFIG_NORTH_INTERFACE_MQTT
    xTaskCreate(handle_mqtt_sub_pub, "handle_mqtt_sub_pub", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
    mqtt_init_and_start();
#endif // CONFIG_NORTH_INTERFACE_MQTT

#ifdef CONFIG_MQTT_OPS
  #ifdef CONFIG_TARGET_DEVICE_ESP32
    #define OPS_PUB_TASK_SIZE (configMINIMAL_STACK_SIZE * 4)
  #endif //CONFIG_TARGET_DEVICE_ESP32
  #ifdef CONFIG_TARGET_DEVICE_ESP8266
    #define OPS_PUB_TASK_SIZE (configMINIMAL_STACK_SIZE * 2)
  #endif //CONFIG_TARGET_DEVICE_ESP8266

    xTaskCreate(ops_pub_task, "ops_pub_task", OPS_PUB_TASK_SIZE, NULL, 5, NULL);
#endif // CONFIG_MQTT_OPS
  }
}
