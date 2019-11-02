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
  ESP_LOGI(TAG, "Current time is: %s", strftime_buf);

  //trigerring
  struct SchedulerCfgMessage s;
  s.actionId = TRIGGER_ACTION;
  s.data.triggerActionData.now = now;
  if (xQueueSend(schedulerCfgQueue
                 ,( void * )&s
                 ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to scheduleCfgQueue");
  }

}


void start_scheduler_timer()
{
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

void log_scheduler(const struct SchedulerCfgMessage *msg)
{
  ESP_LOGI(TAG, "schId: %d, ts: %ld, aId: %d, aState: %d",
           msg->schedulerId,
           msg->timestamp,
           msg->actionId,
           msg->actionState);
  if (msg->actionId == RELAY_ACTION) {
    ESP_LOGI(TAG, "realyId: %d, relayValue: %d",
             msg->data.relayActionData.relayId,
             msg->data.relayActionData.relayValue);
  }
}

void handle_relay_action_trigger(struct SchedulerCfgMessage *msg, int nowMinutes) {
  int schedulerMinutes = msg->timestamp / 60;
  ESP_LOGI(TAG, "schedulerMinutes: %d", schedulerMinutes);

  if (schedulerMinutes == nowMinutes ||
      schedulerMinutes == (nowMinutes - 1)) {
    ESP_LOGI(TAG, "Executing scheduleId: %d",
             msg->schedulerId);
    struct RelayCmdMessage r=msg->data.relayActionData;
    if (xQueueSend( relayCmdQueue,
                    ( void * )&r,
                    MQTT_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to relayCmdQueue");
    }
  }

  if (schedulerMinutes <= nowMinutes) {
    ESP_LOGI(TAG, "Disabling scheduleId: %d",
             msg->schedulerId);
    msg->actionState = ACTION_STATE_DISABLED;
  }
}


void handle_action_trigger(struct SchedulerCfgMessage *schedulerCfg, int nowMinutes)
{
  for (int i = 0; i < MAX_SCHEDULER_NB; ++i) {
    log_scheduler(&schedulerCfg[i]);
    if (schedulerCfg[i].actionId    == RELAY_ACTION &&
        schedulerCfg[i].actionState == ACTION_STATE_ENABLED) {
      handle_relay_action_trigger(&schedulerCfg[i], nowMinutes);
    }
  }
}

void handle_scheduler(void* pvParameters)
{
  ESP_LOGI(TAG, "handle_scheduler task started");

  start_scheduler_timer();

  struct SchedulerCfgMessage schedulerCfg[MAX_SCHEDULER_NB];
  memset (schedulerCfg, 0, MAX_SCHEDULER_NB * sizeof(struct SchedulerCfgMessage));
  struct SchedulerCfgMessage tempSchedulerCfg;
  while(1) {
    if( xQueueReceive(schedulerCfgQueue, &tempSchedulerCfg, portMAX_DELAY)) {
      if (tempSchedulerCfg.actionId == TRIGGER_ACTION) {
        int nowMinutes = tempSchedulerCfg.data.triggerActionData.now / 60;
        ESP_LOGI(TAG, "nowMinutes: %d", nowMinutes);
        handle_action_trigger(schedulerCfg, nowMinutes);
      } else if (tempSchedulerCfg.actionId == ADD_RELAY_ACTION) {
        if (tempSchedulerCfg.schedulerId < MAX_SCHEDULER_NB) {
          ESP_LOGI(TAG, "Updating schedulerId: %d",
                   tempSchedulerCfg.schedulerId);
          schedulerCfg[tempSchedulerCfg.schedulerId] = tempSchedulerCfg;
        } else {
              ESP_LOGE(TAG, "Wrong schedulerId: %d",
                       tempSchedulerCfg.schedulerId);
        }
      } else {
        ESP_LOGE(TAG, "Unknown actionId: %d",
                 tempSchedulerCfg.actionId);
      }
    }
  }
}
