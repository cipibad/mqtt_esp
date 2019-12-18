#include "mqtt_client.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"


void update_relay_state(int id, char value)
{
}

int xQueueSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait)
{}

TimerHandle_t xTimerCreate(	const char * const pcTimerName,
								const TickType_t xTimerPeriodInTicks,
								const UBaseType_t uxAutoReload,
								void * const pvTimerID,
                            TimerCallbackFunction_t pxCallbackFunction )
{}

BaseType_t xTimerStart( TimerHandle_t xTimer, const TickType_t xTicksToWait )
{}

BaseType_t xQueueReceive( QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait)
{}

esp_err_t write_nvs_integer(const char * tag, int value)
{}
esp_err_t read_nvs_integer(const char * tag, int * value)
{}

esp_err_t write_nvs_short(const char * tag, short value)
{}
esp_err_t read_nvs_short(const char * tag, short * value)
{}


void ESP_ERROR_CHECK(int a)
{
  do {
  }
  while (0);
}

EventBits_t xEventGroupGetBits( EventGroupHandle_t xEventGroup )
{}
EventBits_t xEventGroupClearBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear )
{}
EventBits_t xEventGroupWaitBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToWaitFor, const BaseType_t xClearOnExit, const BaseType_t xWaitForAllBits, TickType_t xTicksToWait )
{}
EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet )
{}


int esp_mqtt_client_publish(esp_mqtt_client_handle_t client, const char *topic, const char *data, int len, int qos, int retain)
{}
esp_err_t esp_mqtt_client_subscribe(esp_mqtt_client_handle_t client, const char *topic, int qos)
{}
esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *config)
{}
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client)
{}

void esp_wifi_stop()
{}
void esp_wifi_start()
{}

int esp_get_free_heap_size()
{}


void vTaskDelay(int a)
{}

int esp_reset_reason()
{}

void * thermostatQueue;
void * mqttQueue;
void * _binary_mqtt_iot_cipex_ro_pem_start;
