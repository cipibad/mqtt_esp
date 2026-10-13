#include "mqtt_client.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"


SemaphoreHandle_t xSemaphore;
QueueHandle_t publishQueue;

void update_relay_status(int id, char value)
{}

int xQueueSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait)
{}

QueueHandle_t xQueueCreate( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize )
{}

void xTaskCreate( TaskFunction_t pvTaskCode, const char * const pcName,
                  unsigned int usStackDepth, void * const pvParameters,
                  unsigned int uxPriority, TaskHandle_t * const pxCreatedTask )
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

BaseType_t xQueueSemaphoreTake( QueueHandle_t xQueue, TickType_t xTicksToWait )
{}

BaseType_t xQueueGenericSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition )
{}

esp_err_t write_nvs_integer(const char * tag, int value)
{}
esp_err_t read_nvs_integer(const char * tag, int * value)
{}

esp_err_t write_nvs_short(const char * tag, short value)
{}
esp_err_t read_nvs_short(const char * tag, short * value)
{}

esp_err_t write_nvs_str(const char * tag, char * value)
{}
esp_err_t read_nvs_str(const char * tag, char * value, size_t * length)
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
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client, esp_mqtt_event_id_t event, esp_event_handler_t event_handler, void *event_handler_arg)
{}
esp_mqtt_client_connection_state_t esp_mqtt_client_get_state(esp_mqtt_client_handle_t client)
{
  return MQTT_CLIENT_STATE_CONNECTED;
}

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
void * _binary_cert_bundle_pem_start;

void publish_persistent_data(const char * topic, const char * data)
{}
void publish_non_persistent_data(const char * topic, const char * data)
{}
void system_restart()
{}
void * schedulerCfgQueue;
void publish_schedulers_data()
{}

#ifdef CONFIG_WATERPUMP_SUPPORT
#include "app_waterpump.h"
int waterPumpStatus = WATERPUMP_STATUS_INIT;
void updateWaterPumpState(int new_state) {
    if (new_state == WATERPUMP_STATUS_OFF && waterPumpStatus == WATERPUMP_STATUS_ON) {
        waterPumpStatus = WATERPUMP_STATUS_ON_OFF_TRANSITION;
    }
}
void publish_waterpump_status() {}
#endif
