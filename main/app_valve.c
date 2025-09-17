#include "esp_system.h"
#ifdef CONFIG_VALVE_SUPPORT
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/queue.h"

#include "app_valve.h"
#include "app_publish_data.h"
#include "app_relay.h"
#ifdef CONFIG_AT_SERVER
#include "app_at.h"
#endif // CONFIG_AT_SERVER

#include <string.h>

#define OPERATION_TIMEOUT 15

static const char *TAG = "VALVE";
extern QueueHandle_t valveQueue;

static TimerHandle_t valveOpenTimer;
static TimerHandle_t valveCloseTimer;

valve_status_t valveStatus;

void publish_valve_status()
{
  const char* valve_status_topic = CONFIG_DEVICE_TYPE"/"CONFIG_CLIENT_ID"/evt/status/valve";
  char data[16];

  memset(data,0,16);
  switch (valveStatus)
  {
  case VALVE_STATUS_INIT:
    ESP_LOGW(TAG, "no notification in idle status");
    break;
  case VALVE_STATUS_CLOSED:
    strcat(data, "closed");
    break;
  case VALVE_STATUS_CLOSED_OPEN_TRANSITION:
    strcat(data, "closed->open");
    break;
  case VALVE_STATUS_OPEN:
    strcat(data, "open");
    break;
  case VALVE_STATUS_OPEN_CLOSED_TRANSITION:
    strcat(data, "open->closed");
    break;
  default:
    ESP_LOGE(TAG, "no notification in unknown status %d", valveStatus);
    break;
  }
  if (strlen(data) > 0) {
    publish_persistent_data(valve_status_topic, data);
  }
}

void openValveTimerCallback( TimerHandle_t xTimer )
{
  const char *pcTimerName = pcTimerGetTimerName( xTimer );
  ESP_LOGI(TAG, "timer %s expired", pcTimerName);
  update_relay_status(CONFIG_VALVE_OPEN_RELAY_ID, RELAY_STATUS_OFF);
  valveStatus = VALVE_STATUS_OPEN;
  publish_valve_status();
  ESP_LOGI(TAG, "valve is now open");
}

void closeValveTimerCallback( TimerHandle_t xTimer )
{
  const char *pcTimerName = pcTimerGetTimerName( xTimer );
  ESP_LOGI(TAG, "timer %s expired", pcTimerName);
  update_relay_status(CONFIG_VALVE_CLOSE_RELAY_ID, RELAY_STATUS_OFF);
  valveStatus = VALVE_STATUS_CLOSED;
  publish_valve_status();
  ESP_LOGI(TAG, "valve is now closed");
}

void openValve()
{
    if (valveOpenTimer == NULL)
    {
      ESP_LOGE(TAG, "No valveOpenTimer found");
      return;
    }
    if( xTimerStart( valveOpenTimer, 0 ) != pdPASS )
    {
      ESP_LOGE(TAG, "Cannot start valveOpenTimer");
      return;
    }
    update_relay_status(CONFIG_VALVE_OPEN_RELAY_ID, RELAY_STATUS_ON);
    valveStatus = VALVE_STATUS_CLOSED_OPEN_TRANSITION;
    publish_valve_status();
    ESP_LOGI(TAG, "valve opening is on-going");
}

void closeValve()
{
    if (valveCloseTimer == NULL)
    {
      ESP_LOGE(TAG, "No valveCloseTimer found");
      return;
    }
    if( xTimerStart( valveCloseTimer, 0 ) != pdPASS )
    {
      ESP_LOGE(TAG, "Cannot start valveCloseTimer");
      return;
    }
    update_relay_status(CONFIG_VALVE_CLOSE_RELAY_ID, RELAY_STATUS_ON);
    valveStatus = VALVE_STATUS_OPEN_CLOSED_TRANSITION;
    publish_valve_status();
    ESP_LOGI(TAG, "valve closing is on-going");
}

void update_valve_status(valve_status_t new_valve_state)
{
  if (new_valve_state == VALVE_STATUS_OPEN) {
    if (valveStatus == VALVE_STATUS_OPEN) {
      ESP_LOGE(TAG, "valve is already open");
      return;
    }
    if (valveStatus == VALVE_STATUS_CLOSED_OPEN_TRANSITION) {
      ESP_LOGE(TAG, "valve opening is on-going");
      return;
    }
    if (valveStatus == VALVE_STATUS_OPEN_CLOSED_TRANSITION) {
      ESP_LOGE(TAG, "valve closing is on-going");
      return;
    }
    openValve();
  } else if (new_valve_state == VALVE_STATUS_CLOSED) {
    if (valveStatus == VALVE_STATUS_CLOSED) {
      ESP_LOGE(TAG, "valve is already closed");
      return;
    }
    if (valveStatus == VALVE_STATUS_CLOSED_OPEN_TRANSITION) {
      ESP_LOGE(TAG, "valve opening is on-going");
      return;
    }
    if (valveStatus == VALVE_STATUS_OPEN_CLOSED_TRANSITION) {
      ESP_LOGE(TAG, "valve closing is on-going");
      return;
    }
    closeValve();
  }
}

void init_valve()
{
    valveStatus = VALVE_STATUS_INIT;
    valveOpenTimer = xTimerCreate( "valveOpenTimer",           /* Text name. */
                      pdMS_TO_TICKS(OPERATION_TIMEOUT*1000),  /* Period. */
                      pdFALSE,                /* Autoreload. */
                      (void *)1,                  /* ID. */
                      openValveTimerCallback );  /* Callback function. */
    valveCloseTimer = xTimerCreate( "valveCloseTimer",           /* Text name. */
                      pdMS_TO_TICKS(OPERATION_TIMEOUT*1000),  /* Period. */
                      pdFALSE,                /* Autoreload. */
                      (void *)2,                  /* ID. */
                      closeValveTimerCallback );  /* Callback function. */

    closeValve();
}

void app_valve_task(void* pvParameters)
{
  ESP_LOGI(TAG, "app_valve_task started");

  if (is_relay_serial_type(CONFIG_VALVE_OPEN_RELAY_ID) || is_relay_serial_type(CONFIG_VALVE_CLOSE_RELAY_ID)) {
    while (!is_serial_interface_online()) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }

  init_valve();

  struct ValveMessage r;
  while(1) {
    if( xQueueReceive( valveQueue, &r , portMAX_DELAY) )
      {
        if (r.msgType == VALVE_CMD_STATUS) {
          update_valve_status(r.data);
          continue;
        }
      }
  }
}
#endif // CONFIG_VALVE_SUPPORT