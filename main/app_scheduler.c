#include <time.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "lwip/apps/sntp.h"

#include "app_main.h"
#include "app_scheduler.h"

static const char *TAG = "SCHEDULER";
extern QueueHandle_t schedulerCfgQueue;
extern QueueHandle_t relayCmdQueue;

void update_time_from_ntp()
{
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;

    // wait for time to be set
    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
      ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      time(&now);
      localtime_r(&now, &timeinfo);
    }

    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "Current time after ntp update: %s", strftime_buf);
}

void vSchedulerCallback( TimerHandle_t xTimer )
{

  ESP_LOGI(TAG, "timer scheduler expired, checking scheduled actions");

  time_t now = 0;
  struct tm timeinfo = { 0 };
  time(&now);
  localtime_r(&now, &timeinfo);

  char strftime_buf[64];
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  ESP_LOGI(TAG, "Current time after ntp update: %s", strftime_buf);

  //trigerring
  struct SchedulerCfgMessage s;
  s.actionId = TRIGGER_ACTION;
  if (xQueueSend(schedulerCfgQueue
                 ,( void * )&s
                 ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to scheduleCfgQueue");
  }

}


void start_scheduler_timer()
{
  ESP_LOGI(TAG, "scheduler_task started");

  ESP_LOGI(TAG, "Initializing SNTP");
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, "pool.ntp.org");
  sntp_init();

  update_time_from_ntp();

  TimerHandle_t th =
    xTimerCreate( "schedulerTimer",           /* Text name. */
                  pdMS_TO_TICKS(60000),  /* Period. */
                  pdTRUE,                /* Autoreload. */
                  (void *)0,                  /* No ID. */
                  vSchedulerCallback );  /* Callback function. */
  if( th != NULL ) {
    ESP_LOGI(TAG, "timer is created");
    if (xTimerStart(th, portMAX_DELAY) != pdFALSE){
      ESP_LOGI(TAG, "timer is active");
    }
  }

}


void handle_scheduler(void* pvParameters)
{
  ESP_LOGI(TAG, "handle_scheduler task started");

  struct SchedulerCfgMessage schedulerCfg[MAX_SCHEDULER_NB];
  memset (schedulerCfg, 0, MAX_SCHEDULER_NB * sizeof(schedulerCfg));
  struct SchedulerCfgMessage tempSchedulerCfg;
  while(1) {
    if( xQueueReceive(schedulerCfgQueue, &tempSchedulerCfg , portMAX_DELAY) )
      {
        if (tempSchedulerCfg.actionId == TRIGGER_ACTION) {
          for (int i = 0; i < MAX_SCHEDULER_NB; ++i) {
            ESP_LOGI(TAG, "id: %d, ts: %d, aId: %d",
                     schedulerCfg[i].schedulerId,
                     schedulerCfg[i].timestamp,
                     schedulerCfg[i].actionId);
            if (schedulerCfg[i].actionId == RELAY_ACTION) {
              ESP_LOGI(TAG, "realyId: %d, relayValue: %d",
                       schedulerCfg[i].data.relayData.relayId,
                       schedulerCfg[i].data.relayData.relayValue);
            }

            //FIXME implement solution to make sure it runs only one time
            //FIXME check timestamp
            if (schedulerCfg[i].actionId == RELAY_ACTION) {
              struct RelayCmdMessage r=schedulerCfg[i].data.relayData;
              if (xQueueSend( relayCmdQueue
                              ,( void * )&r
                              ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
                ESP_LOGE(TAG, "Cannot send to relayCmdQueue");
              }
            }
          }
        } else {
          schedulerCfg[tempSchedulerCfg.schedulerId] = tempSchedulerCfg;
        }
      }
  }

}
