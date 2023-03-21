#include "app_waterpump.h"
#include "app_relay.h"

#include "esp_log.h"


#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "APP_WATERPUMP";

TimerHandle_t valveOnTimer;
TimerHandle_t valveOffTimer;

int waterPumpStatus;

// void turnValveOn(){

// }

// void turnValveOff(){

// }

void openValveTimerCallback( TimerHandle_t xTimer )
{
  const char *pcTimerName = pcTimerGetTimerName( xTimer );
  ESP_LOGI(TAG, "timer %s expired", pcTimerName);
         // setGpioOpenValveOff
  update_relay_status(CONFIG_WATERPUMP_RELAY_GPIO, RELAY_STATUS_ON);
  waterPumpStatus = WATERPUMP_STATUS_ON;
  ESP_LOGI(TAG, "waterpump is now enabled");
}

void closeValveTimerCallback( TimerHandle_t xTimer )
{
  const char *pcTimerName = pcTimerGetTimerName( xTimer );
  ESP_LOGI(TAG, "timer %s expired", pcTimerName);
           // setGpioCloseValveOff

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
    // setgpioOpenValveOn

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

    // setGpioCloseValveOn
}
