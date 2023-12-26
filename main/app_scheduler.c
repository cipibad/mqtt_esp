#include "esp_system.h"
#ifdef CONFIG_MQTT_SCHEDULERS

#include <time.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "lwip/apps/sntp.h"

#include "string.h"

#include "app_main.h"
#include "app_nvs.h"
#include "app_scheduler.h"
#include "app_publish_data.h"

#include "app_relay.h"

#if CONFIG_MQTT_THERMOSTATS_NB > 0
#include "app_thermostat.h"
extern QueueHandle_t thermostatQueue;
#endif // CONFIG_MQTT_THERMOSTATS_NB > 0

static const char *TAG = "SCHEDULER";
extern QueueHandle_t schedulerCfgQueue;
extern QueueHandle_t relayQueue;

const char * schedulerActionTAG[MAX_SCHEDULER_NB] = {
  "schAction0",
  "schAction1",
};

const char * schedulerDowTAG[MAX_SCHEDULER_NB] = {
  "schDow0",
  "schDow1",
};

const char * schedulerTimeTAG[MAX_SCHEDULER_NB] = {
  "schTime0",
  "schTime1",
};

const char * schedulerStatusTAG[MAX_SCHEDULER_NB] = {
  "schStatus0",
  "schStatus1",
};

enum SchedulerAction schedulerAction[MAX_SCHEDULER_NB] = {
  SCHEDULER_ACTION_UNSET,
  SCHEDULER_ACTION_UNSET,
};

short schedulerDow[MAX_SCHEDULER_NB] = {
  0,
  0,
};

struct SchedulerTime schedulerTime[MAX_SCHEDULER_NB] = {
  {0, 0},
  {0, 0},
};

enum SchedulerStatus schedulerStatus[MAX_SCHEDULER_NB] = {
  SCHEDULER_STATUS_UNSET,
  SCHEDULER_STATUS_UNSET,
};

time_t lastNow = 0;


void read_nvs_scheduler_data()
{
  esp_err_t err;

  for(int id = 0; id < MAX_SCHEDULER_NB; id++) {
    err=read_nvs_short(schedulerActionTAG[id], (short*) &schedulerAction[id]);
    ESP_ERROR_CHECK( err );
  }

  for(int id = 0; id < MAX_SCHEDULER_NB; id++) {
    err=read_nvs_short(schedulerDowTAG[id], (short*) &schedulerDow[id]);
    ESP_ERROR_CHECK( err );
  }

  char data[16];
  size_t length;

  for(int id = 0; id < MAX_SCHEDULER_NB; id++) {
    memset(data,0,16);
    length = sizeof(data);

    err=read_nvs_str(schedulerTimeTAG[id], data, &length);
    ESP_ERROR_CHECK( err );

    if(strlen(data) == 0) {
      continue;
    }

    char *token = strtok(data, ":");
    if (!token) {
      ESP_LOGW(TAG, "unhandled scheduler time token 1: %s", data);
      continue;
    }
    schedulerTime[id].hour = atoi(token);

    token = strtok(NULL, ":");
    if (!token) {
      ESP_LOGW(TAG, "unhandled scheduler time token 2: %s", data);
      continue;
    }
    schedulerTime[id].minute = atoi(token);
  }

  for(int id = 0; id < MAX_SCHEDULER_NB; id++) {
    err=read_nvs_short(schedulerStatusTAG[id], (short*) &schedulerStatus[id]);
    ESP_ERROR_CHECK( err );
  }

}
void publish_scheduler_action_evt(int id)
{
  const char * thermostat_topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/action/scheduler";

  char data[16];
  memset(data,0,16);
  sprintf(data, "%s",
          schedulerAction[id] == SCHEDULER_ACTION_UNSET ? "unset" :
          schedulerAction[id] == SCHEDULER_ACTION_RELAY_ON ? "relay_on" :
          schedulerAction[id] == SCHEDULER_ACTION_RELAY_OFF ? "relay_off" :
          schedulerAction[id] == SCHEDULER_ACTION_WATER_TEMP_LOW ? "water_temp_low" :
          schedulerAction[id] == SCHEDULER_ACTION_WATER_TEMP_HIGH ? "water_temp_high" :
          schedulerAction[id] == SCHEDULER_ACTION_OW_ON ? "ow_on" :
          schedulerAction[id] == SCHEDULER_ACTION_OW_OFF ? "ow_off" : "unset");

  char topic[MAX_TOPIC_LEN];
  memset(topic,0,MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", thermostat_topic, id);

  publish_persistent_data(topic, data);
}

void publish_scheduler_dow_evt(int id)
{
  const char * thermostat_topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/dow/scheduler";

  char data[8];
  memset(data,0,8);
  sprintf(data, "%d", schedulerDow[id]);

  char topic[MAX_TOPIC_LEN];
  memset(topic,0,MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", thermostat_topic, id);

  publish_persistent_data(topic, data);
}

void publish_scheduler_time_evt(int id)
{
  const char * thermostat_topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/time/scheduler";

  char data[16];
  memset(data,0,16);
  sprintf(data, "%d:%d", schedulerTime[id].hour, schedulerTime[id].minute);

  char topic[MAX_TOPIC_LEN];
  memset(topic,0,MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", thermostat_topic, id);

  publish_persistent_data(topic, data);
}

void publish_scheduler_status_evt(int id)
{
  const char * thermostat_topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/status/scheduler";

  char data[8];
  memset(data,0,8);
  sprintf(data, "%s",
          schedulerStatus[id] == SCHEDULER_STATUS_UNSET ? "OFF" :
          schedulerStatus[id] == SCHEDULER_STATUS_ON ? "ON" :
          schedulerStatus[id] == SCHEDULER_STATUS_OFF ? "OFF" : "OFF");

  char topic[MAX_TOPIC_LEN];
  memset(topic,0,MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", thermostat_topic, id);

  publish_persistent_data(topic, data);
}

void publish_all_schedulers_action_evt()
{
  for(int id = 0; id < MAX_SCHEDULER_NB; id++) {
    publish_scheduler_action_evt(id);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void publish_all_schedulers_dow_evt()
{
  for(int id = 0; id < MAX_SCHEDULER_NB; id++) {
    publish_scheduler_dow_evt(id);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void publish_all_schedulers_time_evt()
{
  for(int id = 0; id < MAX_SCHEDULER_NB; id++) {
    publish_scheduler_time_evt(id);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void publish_all_schedulers_status_evt()
{
  for(int id = 0; id < MAX_SCHEDULER_NB; id++) {
    publish_scheduler_status_evt(id);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void publish_schedulers_data() {
  publish_all_schedulers_action_evt();
  publish_all_schedulers_dow_evt();
  publish_all_schedulers_time_evt();
  publish_all_schedulers_status_evt();
}

void update_time_from_ntp()
{
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;

    // wait for time to be set
    while (timeinfo.tm_year < (2022 - 1900) && ++retry < retry_count) {
      ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      time(&now);
      localtime_r(&now, &timeinfo);
    }

    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "Current time after ntp update: %s", strftime_buf);
    lastNow = now;

    ESP_LOGI(TAG, "Sleeping %d seconds to execute at exact minute", 60 - timeinfo.tm_sec);
    vTaskDelay((60 - timeinfo.tm_sec) * 1000 / portTICK_PERIOD_MS);
}

void vSchedulerCallback( TimerHandle_t xTimer )
{

  ESP_LOGI(TAG, "timer scheduler expired, checking scheduled actions");


  //trigerring
  struct SchedulerCfgMessage s;
  s.msgType = SCHEDULER_CMD_TRIGGER;
  time(&s.data.now);
  if (xQueueSend(schedulerCfgQueue
                 ,( void * )&s
                 ,SCHEDULE_QUEUE_TIMEOUT) != pdPASS) {
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
                  pdMS_TO_TICKS(60 * 1000),  /* Period. */
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

void executeAction(enum SchedulerAction sa)
{
  if (sa == SCHEDULER_ACTION_RELAY_ON) {
    struct RelayMessage r = {RELAY_CMD_STATUS, 0, RELAY_STATUS_ON};
    if (xQueueSend( relayQueue,
                    ( void * )&r,
                    RELAY_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to relayQueue");
    }
  } else if (sa == SCHEDULER_ACTION_RELAY_OFF) {
    struct RelayMessage r = {RELAY_CMD_STATUS, 0, RELAY_STATUS_OFF};
    if (xQueueSend( relayQueue,
                    ( void * )&r,
                    RELAY_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to relayQueue");
    }
#if CONFIG_MQTT_THERMOSTATS_NB > 0
  } else if (sa == SCHEDULER_ACTION_WATER_TEMP_LOW) {
    struct ThermostatMessage tm = {THERMOSTAT_CMD_TARGET_TEMPERATURE, 2, {{0,0}}};
    tm.data.targetTemperature = 290;
    if (xQueueSend( thermostatQueue
                    ,( void * )&tm
                    ,THERMOSTAT_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to thermostatQueue");
    }
  } else if (sa == SCHEDULER_ACTION_WATER_TEMP_HIGH) {
    struct ThermostatMessage tm = {THERMOSTAT_CMD_TARGET_TEMPERATURE, 2, {{0,0}}};
    tm.data.targetTemperature = 330;
    if (xQueueSend( thermostatQueue
                    ,( void * )&tm
                    ,THERMOSTAT_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to thermostatQueue");
    }
#endif // CONFIG_MQTT_THERMOSTATS_NB > 0

  }
}

void handle_scheduler(void* pvParameters)
{
  ESP_LOGI(TAG, "handle_scheduler task started");

  read_nvs_scheduler_data();
  start_scheduler_timer();

  struct SchedulerCfgMessage tempSchedulerCfg;
  while(1) {
    if( xQueueReceive(schedulerCfgQueue, &tempSchedulerCfg, portMAX_DELAY)) {
      if (tempSchedulerCfg.msgType == SCHEDULER_CMD_TRIGGER) {
        struct tm timeinfo;
        memset(&timeinfo, 0, sizeof(struct tm));

        localtime_r(&tempSchedulerCfg.data.now, &timeinfo);

        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "Current scheduler trigger time is: %s", strftime_buf);

        for(int sid=0; sid <=1; sid++) {
          ESP_LOGI(TAG, "Checking scheduler with id: %d", sid);
          if (schedulerStatus[sid] == SCHEDULER_STATUS_ON) {
            short dow = 1 << timeinfo.tm_wday;
            ESP_LOGI(TAG, "Scheduler dow: %d", schedulerDow[sid]);
            if (schedulerDow[sid] & dow || schedulerDow[sid] == 0) {
              timeinfo.tm_hour = schedulerTime[sid].hour;
              timeinfo.tm_min = schedulerTime[sid].minute;
              timeinfo.tm_sec = 0;
              time_t schedulerTimeNow = mktime(&timeinfo);

              struct tm ti;
              memset(&ti, 0, sizeof(struct tm));
              localtime_r(&schedulerTimeNow, &ti);
              char strfti_buf[64];
              strftime(strfti_buf, sizeof(strfti_buf), "%c", &timeinfo);
              ESP_LOGI(TAG, "Scheduler time is: %s", strfti_buf);
              if (lastNow < schedulerTimeNow && schedulerTimeNow <= tempSchedulerCfg.data.now) {
                executeAction(schedulerAction[sid]);
                if(schedulerDow[sid] == 0) {
                  schedulerStatus[sid] = SCHEDULER_STATUS_OFF;
                  publish_scheduler_status_evt(sid);
                }
              }
            }
          }
        }
        lastNow = tempSchedulerCfg.data.now;
      } else if (tempSchedulerCfg.msgType == SCHEDULER_CMD_ACTION) {
        unsigned char schedulerId = tempSchedulerCfg.schedulerId;
        if (schedulerId > 1) {
          ESP_LOGE(TAG, "Unexpected schedulerId: %d",
            schedulerId);
          continue;
        }
        if (schedulerAction[schedulerId] != tempSchedulerCfg.data.action)
        {
          schedulerAction[schedulerId] = tempSchedulerCfg.data.action;
          esp_err_t err = write_nvs_short(schedulerActionTAG[schedulerId],
                                          schedulerAction[schedulerId]);
          ESP_ERROR_CHECK( err );
        }
        publish_scheduler_action_evt(schedulerId);
      } else if (tempSchedulerCfg.msgType == SCHEDULER_CMD_DOW) {
        unsigned char schedulerId = tempSchedulerCfg.schedulerId;
        if (schedulerId > 1) {
          ESP_LOGE(TAG, "Unexpected schedulerId: %d",
            schedulerId);
          continue;
        }
        if (schedulerDow[schedulerId]  != tempSchedulerCfg.data.dow)
        {
          schedulerDow[schedulerId] = tempSchedulerCfg.data.dow;

          esp_err_t err = write_nvs_short(schedulerDowTAG[schedulerId],
                                          schedulerDow[schedulerId]);
          ESP_ERROR_CHECK( err );
        }
        publish_scheduler_dow_evt(schedulerId);
      } else if (tempSchedulerCfg.msgType == SCHEDULER_CMD_TIME) {
        unsigned char schedulerId = tempSchedulerCfg.schedulerId;
        if (schedulerId > 1) {
          ESP_LOGE(TAG, "Unexpected schedulerId: %d",
            schedulerId);
          continue;
        }
        if (schedulerTime[schedulerId].hour   != tempSchedulerCfg.data.time.hour ||
            schedulerTime[schedulerId].minute != tempSchedulerCfg.data.time.minute)
        {
          schedulerTime[schedulerId] = tempSchedulerCfg.data.time;

          char data[16];
          memset(data,0,16);
          sprintf(data, "%d:%d", schedulerTime[schedulerId].hour, schedulerTime[schedulerId].minute);
          esp_err_t err = write_nvs_str(schedulerTimeTAG[schedulerId],
                                          data);
          ESP_ERROR_CHECK( err );
        }
        publish_scheduler_time_evt(schedulerId);
      } else if (tempSchedulerCfg.msgType == SCHEDULER_CMD_STATUS) {
        unsigned char schedulerId = tempSchedulerCfg.schedulerId;
        if (schedulerId > 1) {
          ESP_LOGE(TAG, "Unexpected schedulerId: %d",
            schedulerId);
          continue;
        }
        if (schedulerStatus[schedulerId] != tempSchedulerCfg.data.status)
        {
          schedulerStatus[schedulerId] = tempSchedulerCfg.data.status;
          esp_err_t err = write_nvs_short(schedulerStatusTAG[schedulerId],
                                          schedulerStatus[schedulerId]);
          ESP_ERROR_CHECK( err );
        }
        publish_scheduler_status_evt(schedulerId);
      } else {
        ESP_LOGE(TAG, "Unknown actionId: %d",
                 tempSchedulerCfg.msgType);
      }
    }
  }
}

#endif //CONFIG_MQTT_SCHEDULERS
