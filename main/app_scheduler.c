#include <time.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "lwip/apps/sntp.h"

#include "app_scheduler.h"

static const char *TAG = "SCHEDULER";

struct SchedulerCfgMessage schedulerCfg;

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

  // FIXME do something if schedulerCfg.timestamp reached
  //we should be called each 60 seconds, but we should have defense in case we are called after 61 seconds (probably set a global last-run)
  //FIXME
  /* int id = (int)pvTimerGetTimerID( xTimer ); */
  /* ESP_LOGI(TAG, "timer %d expired, sending stop msg", id); */
  /* struct RelayCmdMessage r={id, 0}; */
  /* if (xQueueSend( relayCmdQueue */
  /*                 ,( void * )&r */
  /*                 ,MQTT_QUEUE_TIMEOUT) != pdPASS) { */
  /*   ESP_LOGE(TAG, "Cannot send to relayCmdQueue"); */
  /* } */
}


void start_scheduler()
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
