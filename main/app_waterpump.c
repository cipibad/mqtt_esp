#include "app_main.h"
#include "app_waterpump.h"
#include "app_relay.h"

#include "esp_log.h"

#include "driver/gpio.h"


#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

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


void openValveTimerCallback( TimerHandle_t xTimer )
{
  const char *pcTimerName = pcTimerGetTimerName( xTimer );
  ESP_LOGI(TAG, "timer %s expired", pcTimerName);
  updateMotorControlPinStatus(CONFIG_WATERPUMP_VALVE_OPEN_GPIO, &valveOnPinStatus, GPIO_STATUS_OFF);
  update_relay_status(CONFIG_WATERPUMP_RELAY_GPIO, RELAY_STATUS_ON);
  waterPumpStatus = WATERPUMP_STATUS_ON;
  ESP_LOGI(TAG, "waterpump is now enabled");
}

void closeValveTimerCallback( TimerHandle_t xTimer )
{
  const char *pcTimerName = pcTimerGetTimerName( xTimer );
  ESP_LOGI(TAG, "timer %s expired", pcTimerName);
  updateMotorControlPinStatus(CONFIG_WATERPUMP_VALVE_CLOSE_GPIO, &valveOffPinStatus, GPIO_STATUS_ON);

  waterPumpStatus = WATERPUMP_STATUS_OFF;
  ESP_LOGI(TAG, "waterpump is now disabled");
}

void initWaterPump()
{
    // we assume no init for relay

    waterPumpStatus = WATERPUMP_STATUS_OFF;
    valveOnTimer = xTimerCreate( "valveOnTimer",           /* Text name. */
                      pdMS_TO_TICKS(15*1000),  /* Period. */
                      pdFALSE,                /* Autoreload. */
                      (void *)1,                  /* ID. */
                      openValveTimerCallback );  /* Callback function. */
    valveOffTimer = xTimerCreate( "valveOffTimer",           /* Text name. */
                      pdMS_TO_TICKS(15*1000),  /* Period. */
                      pdFALSE,                /* Autoreload. */
                      (void *)2,                  /* ID. */
                      openValveTimerCallback );  /* Callback function. */

    initMotorControlPin(CONFIG_WATERPUMP_VALVE_OPEN_GPIO, &valveOnPinStatus);
    initMotorControlPin(CONFIG_WATERPUMP_VALVE_CLOSE_GPIO, &valveOffPinStatus);
}

void enableWaterPump()
{
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

    if (valveOnTimer == NULL) {
      ESP_LOGE(TAG, "No valveOnTimer found");
      return;
    }
    if( xTimerStart( valveOnTimer, 0 ) != pdPASS )
    {
      ESP_LOGE(TAG, "Cannot start valveOnTimer");
      return;
    }
    waterPumpStatus = WATERPUMP_STATUS_OFF_ON_TRANSITION;
    updateMotorControlPinStatus(CONFIG_WATERPUMP_VALVE_OPEN_GPIO, &valveOnPinStatus, GPIO_STATUS_ON);
    ESP_LOGI(TAG, "waterpump enabling is on-going");

}

void disableWaterPump()
{
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
    if (valveOffTimer == NULL) {
      ESP_LOGE(TAG, "No valveOffTimer found");
      return;
    }
    if( xTimerStart( valveOffTimer, 0 ) != pdPASS )
    {
      ESP_LOGE(TAG, "Cannot start valveOffTimer");
      return;
    }
    update_relay_status(CONFIG_WATERPUMP_RELAY_GPIO, RELAY_STATUS_OFF);
    updateMotorControlPinStatus(CONFIG_WATERPUMP_VALVE_CLOSE_GPIO, &valveOffPinStatus, GPIO_STATUS_ON);
}
