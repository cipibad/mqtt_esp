#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"

#include <string.h> //for memset
#include "MQTTClient.h"

#include "app_esp8266.h"

#include "app_wifi.h"
#include "app_mqtt.h"
#include "app_switch.h"

/* #include "app_sensors.h" */
/* #include "app_thermostat.h" */
#include "app_relay.h"

#include "app_ota.h"

int32_t wtemperature;
int32_t ctemperature;
int16_t pressure;

#define FW_VERSION "0.02"


/* extern int targetTemperature; */
/* extern int targetTemperatureSensibility; */
/* extern const char * targetTemperatureTAG; */
/* extern const char * targetTemperatureSensibilityTAG; */

int16_t connect_reason;
const int boot = 0;
const int mqtt_disconnect = 1;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t wifi_event_group;
EventGroupHandle_t mqtt_event_group;
const int CONNECTED_BIT = BIT0;
const int SUBSCRIBED_BIT = BIT1;
const int PUBLISHED_BIT = BIT2;
const int INIT_FINISHED_BIT = BIT3;

QueueHandle_t relayQueue;
QueueHandle_t otaQueue;
/* QueueHandle_t thermostatQueue; */
QueueHandle_t mqttQueue;

const int MQTT_PUBLISH_RELAYS_BIT = BIT5; //FIXME
const int MQTT_PUBLISH_DHT22_BIT = BIT6; //FIXME

static const char *TAG = "MQTT(S?)_MAIN";


#define BLINK_GPIO GPIO_NUM_13
void blink_task(void *pvParameter)
{
  /* Set the GPIO as a push/pull output */
  gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
  int interval;
  while(1) {

    interval=250;
    EventBits_t bits = xEventGroupGetBits(wifi_event_group);
    if( ( bits & CONNECTED_BIT ) != 0 ) {
      interval=500;
    }
    gpio_set_level(BLINK_GPIO, OFF);
    bits = xEventGroupGetBits(mqtt_event_group);
    while ( bits & CONNECTED_BIT ) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelay(interval / portTICK_PERIOD_MS);
    gpio_set_level(BLINK_GPIO, ON);
    vTaskDelay(interval / portTICK_PERIOD_MS);
  }
}

void app_main(void)
{
  connect_reason=boot;
  ESP_LOGI(TAG, "[APP] Startup..");
  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
  esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

  relays_init();

  mqtt_event_group = xEventGroupCreate();
  wifi_event_group = xEventGroupCreate();

  /* thermostatQueue = xQueueCreate(1, sizeof(struct ThermostatMessage) ); */
  relayQueue = xQueueCreate(1, sizeof(struct RelayMessage) );
  otaQueue = xQueueCreate(1, sizeof(struct OtaMessage) );
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

  /* err=read_thermostat_nvs(targetTemperatureTAG, &targetTemperature); */
  /* ESP_ERROR_CHECK( err ); */

  /* err=read_thermostat_nvs(targetTemperatureSensibilityTAG, &targetTemperatureSensibility); */
  /* ESP_ERROR_CHECK( err ); */

  MQTTClient* client = mqtt_init();

  /* xTaskCreate(sensors_read, "sensors_read", configMINIMAL_STACK_SIZE * 3, (void *)client, 10, NULL); */

  xTaskCreate(handle_relay_cmd_task, "handle_relay_cmd_task", configMINIMAL_STACK_SIZE * 3, (void *)client, 5, NULL);
  xTaskCreate(handle_ota_update_task, "handle_ota_update_task", configMINIMAL_STACK_SIZE * 7, (void *)client, 5, NULL);
  /* xTaskCreate(handle_thermostat_cmd_task, "handle_thermostat_cmd_task", configMINIMAL_STACK_SIZE * 3, (void *)client, 5, NULL); */

  wifi_init();
  gpio_switch_init(NULL);
  mqtt_start(client);

}
