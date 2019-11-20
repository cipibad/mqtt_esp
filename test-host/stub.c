#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void mqtt_publish_data(const char * topic,
                       const char * data,
                       int qos, int retain)
{
 
}

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


void ESP_ERROR_CHECK(int a)
{
  do {
  }
  while (0);
}

void * thermostatQueue;
