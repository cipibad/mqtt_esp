#include "app_main.h"
#include "app_waterpump.h"
#include "app_relay.h"

#include "app_publish_data.h"

#include "esp_log.h"

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <string.h>

static const char *TAG = "APP_WATERPUMP";

TimerHandle_t valveOnTimer;
TimerHandle_t valveOffTimer;

int waterPumpStatus;

int valveOnPinStatus;
int valveOffPinStatus;

void initMotorControlPin(int pin, int* status)
{
    *status = GPIO_LOW;
    gpio_pad_select_gpio(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, *status);
}

void updateMotorControlPinStatus(int pin, int* status, int value)
{
  ESP_LOGI(TAG, "updateMotorControlPin: pin: %d, status: %d, value: %d", pin, *status, value);
  if (value != (*status == GPIO_HIGH)) {
    if (value == GPIO_STATUS_ON) {
      *status = GPIO_HIGH;
      ESP_LOGI(TAG, "enabling GPIO %d", pin);
    }
    if (value == GPIO_STATUS_OFF) {
      *status = GPIO_LOW;
      ESP_LOGI(TAG, "disabling GPIO %d", pin);
    }
    gpio_set_level(pin, *status);
  }
}

void publish_waterpump_status()
{
  const char* waterpump_status_topic = CONFIG_DEVICE_TYPE"/"CONFIG_CLIENT_ID"/evt/status/waterpump";
  char data[16];

  memset(data,0,16);
  switch (waterPumpStatus)
  {
  case WATERPUMP_STATUS_INIT:
    ESP_LOGW(TAG, "no notification in idle status");
    break;
  case WATERPUMP_STATUS_OFF:
    strcat(data, "off");
    break;
  case WATERPUMP_STATUS_OFF_ON_TRANSITION:
    strcat(data, "off->on");
    break;
  case WATERPUMP_STATUS_ON:
    strcat(data, "on");
    break;
  case WATERPUMP_STATUS_ON_OFF_TRANSITION:
    strcat(data, "on->off");
    break;
  default:
    ESP_LOGE(TAG, "no notification in unknown status %d", waterPumpStatus);
    break;
  }

  if (strlen(data) > 0) {
    publish_persistent_data(waterpump_status_topic, data);
  }
}

#if CONFIG_WATERPUMP_ENABLE_NOTIFICATIONS
void publish_waterpump_notification_evt(const char* msg)
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/notification/waterpump";
  publish_non_persistent_data(topic, msg);
}
#endif // CONFIG_WATERPUMP_ENABLE_NOTIFICATIONS

void openValveTimerCallback( TimerHandle_t xTimer )
{
  const char *pcTimerName = pcTimerGetTimerName( xTimer );
  ESP_LOGI(TAG, "timer %s expired", pcTimerName);
  updateMotorControlPinStatus(CONFIG_WATERPUMP_VALVE_OPEN_GPIO, &valveOnPinStatus, GPIO_STATUS_OFF);
  update_relay_status(CONFIG_WATERPUMP_RELAY_ID, RELAY_STATUS_ON);
  waterPumpStatus = WATERPUMP_STATUS_ON;
  publish_waterpump_status();
  ESP_LOGI(TAG, "waterpump is now enabled");
  #if CONFIG_WATERPUMP_ENABLE_NOTIFICATIONS
  publish_waterpump_notification_evt("Waterpump is now enabled");
  #endif // CONFIG_WATERPUMP_ENABLE_NOTIFICATIONS

}

void closeValveTimerCallback( TimerHandle_t xTimer )
{
  const char *pcTimerName = pcTimerGetTimerName( xTimer );
  ESP_LOGI(TAG, "timer %s expired", pcTimerName);
  updateMotorControlPinStatus(CONFIG_WATERPUMP_VALVE_CLOSE_GPIO, &valveOffPinStatus, GPIO_STATUS_OFF);

  waterPumpStatus = WATERPUMP_STATUS_OFF;
  publish_waterpump_status();
  ESP_LOGI(TAG, "waterpump is now disabled");
  #if CONFIG_WATERPUMP_ENABLE_NOTIFICATIONS
  publish_waterpump_notification_evt("Waterpump is now disabled");
  #endif // CONFIG_WATERPUMP_ENABLE_NOTIFICATIONS

}

void enableWaterPump()
{
    if (valveOnTimer == NULL) {
      ESP_LOGE(TAG, "No valveOnTimer found");
      return;
    }
    if( xTimerStart( valveOnTimer, 0 ) != pdPASS )
    {
      ESP_LOGE(TAG, "Cannot start valveOnTimer");
      return;
    }
    updateMotorControlPinStatus(CONFIG_WATERPUMP_VALVE_OPEN_GPIO, &valveOnPinStatus, GPIO_STATUS_ON);
    waterPumpStatus = WATERPUMP_STATUS_OFF_ON_TRANSITION;
    publish_waterpump_status();
    ESP_LOGI(TAG, "waterpump enabling is on-going");

}

void disableWaterPump()
{
    if (valveOffTimer == NULL)
    {
      ESP_LOGE(TAG, "No valveOffTimer found");
      return;
    }
    if( xTimerStart( valveOffTimer, 0 ) != pdPASS )
    {
      ESP_LOGE(TAG, "Cannot start valveOffTimer");
      return;
    }
    update_relay_status(CONFIG_WATERPUMP_RELAY_ID, RELAY_STATUS_OFF);
    updateMotorControlPinStatus(CONFIG_WATERPUMP_VALVE_CLOSE_GPIO, &valveOffPinStatus, GPIO_STATUS_ON);
    waterPumpStatus = WATERPUMP_STATUS_ON_OFF_TRANSITION;
    publish_waterpump_status();
    ESP_LOGI(TAG, "waterpump disabling is on-going");
}

void updateWaterPumpState(int new_waterpump_state)
{
  if (new_waterpump_state == WATERPUMP_STATUS_ON) {
    if (waterPumpStatus == WATERPUMP_STATUS_ON) {
      ESP_LOGE(TAG, "waterpump is already enabled");
      return;
    }
    if (waterPumpStatus == WATERPUMP_STATUS_OFF_ON_TRANSITION) {
      ESP_LOGE(TAG, "waterpump enabling is on-going");
      return;
    }
    if (waterPumpStatus == WATERPUMP_STATUS_ON_OFF_TRANSITION) {
      ESP_LOGE(TAG, "waterpump disabling is on-going");
      return;
    }
    enableWaterPump();
  } else if (new_waterpump_state == WATERPUMP_STATUS_OFF) {
    if (waterPumpStatus == WATERPUMP_STATUS_OFF) {
      ESP_LOGE(TAG, "waterpump is already disabled");
      return;
    }
    if (waterPumpStatus == WATERPUMP_STATUS_OFF_ON_TRANSITION) {
      ESP_LOGE(TAG, "waterpump enabling is on-going");
      return;
    }
    if (waterPumpStatus == WATERPUMP_STATUS_ON_OFF_TRANSITION) {
      ESP_LOGE(TAG, "waterpump disabling is on-going");
      return;
    }
    disableWaterPump();
  }
}

void initWaterPump()
{
    // we assume no init for relay

    waterPumpStatus = WATERPUMP_STATUS_INIT;
    valveOnTimer = xTimerCreate( "valveOnTimer",           /* Text name. */
                      pdMS_TO_TICKS(15*1000),  /* Period. */
                      pdFALSE,                /* Autoreload. */
                      (void *)1,                  /* ID. */
                      openValveTimerCallback );  /* Callback function. */
    valveOffTimer = xTimerCreate( "valveOffTimer",           /* Text name. */
                      pdMS_TO_TICKS(15*1000),  /* Period. */
                      pdFALSE,                /* Autoreload. */
                      (void *)2,                  /* ID. */
                      closeValveTimerCallback );  /* Callback function. */

    initMotorControlPin(CONFIG_WATERPUMP_VALVE_OPEN_GPIO, &valveOnPinStatus);
    initMotorControlPin(CONFIG_WATERPUMP_VALVE_CLOSE_GPIO, &valveOffPinStatus);
    disableWaterPump();
}
