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

time_t lastNow = 0;

const char* schedulerActionTAG[CONFIG_MQTT_SCHEDULERS_NB];

const char* schedulerDowTAG[CONFIG_MQTT_SCHEDULERS_NB];

const char* schedulerTimeTAG[CONFIG_MQTT_SCHEDULERS_NB];

const char* schedulerStatusTAG[CONFIG_MQTT_SCHEDULERS_NB];

struct SchedulerData scheduler_data[CONFIG_MQTT_SCHEDULERS_NB];

void init_tag(const char* tag_array[], const char* tag_prefix)
{
  char buffer[32];
  for (int i = 0; i < CONFIG_MQTT_SCHEDULERS_NB;  i++) {
    snprintf(buffer, sizeof(buffer), "%s%d", tag_prefix, i);
    const char* tag = strdup(buffer);
    if (tag) {
      tag_array[i] = tag;
    } else {
        ESP_LOGE(TAG, "cannot init memory for tag_array");
    }
  }
}

void init_scheduler_tags()
{
  init_tag(schedulerActionTAG, "schAction");
  init_tag(schedulerDowTAG, "schDow");
  init_tag(schedulerTimeTAG, "schTime");
  init_tag(schedulerStatusTAG, "schStatus");
}

void read_nvs_scheduler_time(const char* tag, struct SchedulerTime* schedulerTime)
{
  char data[16];
  memset(data,0,16);
  size_t length = sizeof(data);

  esp_err_t err=read_nvs_str(tag, data, &length);
  ESP_ERROR_CHECK_WITHOUT_ABORT( err );

  if(strlen(data) == 0) {
    ESP_LOGW(TAG, "empty string read from nvs for time: %s", data);
    return;
  }

  char *token = strtok(data, ":");
  if (!token) {
    ESP_LOGW(TAG, "unhandled scheduler time token 1: %s", data);
    return;
  }
  schedulerTime->hour = atoi(token);

  token = strtok(NULL, ":");
  if (!token) {
    ESP_LOGW(TAG, "unhandled scheduler time token 2: %s", data);
    return;
  }
  schedulerTime->minute = atoi(token);
}

void init_scheduler_data()
{
  esp_err_t err;

  for (int i = 0; i < CONFIG_MQTT_SCHEDULERS_NB;  i++) {
    scheduler_data[i].schedulerAction = SCHEDULER_ACTION_UNSET;
    err=read_nvs_short(schedulerActionTAG[i], (short*) &scheduler_data[i].schedulerAction);
    ESP_ERROR_CHECK_WITHOUT_ABORT( err );

    scheduler_data[i].schedulerDow = 0;
    err=read_nvs_short(schedulerDowTAG[i], (short*) &scheduler_data[i].schedulerDow);
    ESP_ERROR_CHECK_WITHOUT_ABORT( err );

    scheduler_data[i].schedulerStatus = SCHEDULER_STATUS_UNSET;
    err=read_nvs_short(schedulerStatusTAG[i], (short*) &scheduler_data[i].schedulerStatus);
    ESP_ERROR_CHECK_WITHOUT_ABORT( err );

    scheduler_data[i].schedulerTime.hour = 0;
    scheduler_data[i].schedulerTime.minute = 0;
    read_nvs_scheduler_time(schedulerTimeTAG[i], &scheduler_data[i].schedulerTime);
  }
}

void init_scheduler()
{
  init_scheduler_tags();
  init_scheduler_data();
}

void publish_scheduler_action_evt(int id)
{
  const char * thermostat_topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/action/scheduler";

  char data[32];
  memset(data,0,32);
  sprintf(data, "%s",
          scheduler_data[id].schedulerAction == SCHEDULER_ACTION_UNSET ? "unset" :
          scheduler_data[id].schedulerAction == SCHEDULER_ACTION_RELAY_ON ? "relay_on" :
          scheduler_data[id].schedulerAction == SCHEDULER_ACTION_RELAY_OFF ? "relay_off" :
          scheduler_data[id].schedulerAction == SCHEDULER_ACTION_WATER_TEMP_LOW ? "water_temp_low" :
          scheduler_data[id].schedulerAction == SCHEDULER_ACTION_WATER_TEMP_HIGH ? "water_temp_high" :
          scheduler_data[id].schedulerAction == SCHEDULER_ACTION_ROOM_TEMP_SLIGHT_INC ? "room_temp_slight_inc" :
          scheduler_data[id].schedulerAction == SCHEDULER_ACTION_ROOM_TEMP_MODERATE_INC ? "room_temp_moderate_inc" :
          scheduler_data[id].schedulerAction == SCHEDULER_ACTION_ROOM_TEMP_SLIGHT_DEC ? "room_temp_slight_dec" :
          scheduler_data[id].schedulerAction == SCHEDULER_ACTION_ROOM_TEMP_MODERATE_DEC ? "room_temp_moderate_dec" : "unset");

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
  sprintf(data, "%d", scheduler_data[id].schedulerDow);

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
  sprintf(data, "%d:%d", scheduler_data[id].schedulerTime.hour, scheduler_data[id].schedulerTime.minute);

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
          scheduler_data[id].schedulerStatus == SCHEDULER_STATUS_UNSET ? "OFF" :
          scheduler_data[id].schedulerStatus == SCHEDULER_STATUS_ON ? "ON" :
          scheduler_data[id].schedulerStatus == SCHEDULER_STATUS_OFF ? "OFF" : "OFF");

  char topic[MAX_TOPIC_LEN];
  memset(topic,0,MAX_TOPIC_LEN);
  sprintf(topic, "%s/%d", thermostat_topic, id);

  publish_persistent_data(topic, data);
}

void publish_all_schedulers_action_evt()
{
  for(int id = 0; id < CONFIG_MQTT_SCHEDULERS_NB; id++) {
    publish_scheduler_action_evt(id);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void publish_all_schedulers_dow_evt()
{
  for(int id = 0; id < CONFIG_MQTT_SCHEDULERS_NB; id++) {
    publish_scheduler_dow_evt(id);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void publish_all_schedulers_time_evt()
{
  for(int id = 0; id < CONFIG_MQTT_SCHEDULERS_NB; id++) {
    publish_scheduler_time_evt(id);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void publish_all_schedulers_status_evt()
{
  for(int id = 0; id < CONFIG_MQTT_SCHEDULERS_NB; id++) {
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
    struct ThermostatMessage tm;
    memset(&tm, 0, sizeof(struct ThermostatMessage));
    tm.msgType = THERMOSTAT_CMD_WATER_TEMP_LOW;

    if (xQueueSend( thermostatQueue
                    ,( void * )&tm
                    ,THERMOSTAT_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send tm to thermostatQueue");
    }
  } else if (sa == SCHEDULER_ACTION_WATER_TEMP_HIGH) {
    struct ThermostatMessage tm;
    memset(&tm, 0, sizeof(struct ThermostatMessage));
    tm.msgType = THERMOSTAT_CMD_WATER_TEMP_HIGH;

    if (xQueueSend( thermostatQueue
                    ,( void * )&tm
                    ,THERMOSTAT_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send tm to thermostatQueue");
    }
  } else if (sa == SCHEDULER_ACTION_ROOM_TEMP_SLIGHT_INC) {
    struct ThermostatMessage tm;
    memset(&tm, 0, sizeof(struct ThermostatMessage));
    tm.msgType = THERMOSTAT_CMD_ROOM_TEMP_SLIGHT_INC;

    if (xQueueSend( thermostatQueue
                    ,( void * )&tm
                    ,THERMOSTAT_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send tm to thermostatQueue");
    }
  } else if (sa == SCHEDULER_ACTION_ROOM_TEMP_MODERATE_INC) {
    struct ThermostatMessage tm;
    memset(&tm, 0, sizeof(struct ThermostatMessage));
    tm.msgType = THERMOSTAT_CMD_ROOM_TEMP_MODERATE_INC;

    if (xQueueSend( thermostatQueue
                    ,( void * )&tm
                    ,THERMOSTAT_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send tm to thermostatQueue");
    }
  } else if (sa == SCHEDULER_ACTION_ROOM_TEMP_SLIGHT_DEC) {
    struct ThermostatMessage tm;
    memset(&tm, 0, sizeof(struct ThermostatMessage));
    tm.msgType = THERMOSTAT_CMD_ROOM_TEMP_SLIGHT_DEC;

    if (xQueueSend( thermostatQueue
                    ,( void * )&tm
                    ,THERMOSTAT_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send tm to thermostatQueue");
    }
  } else if (sa == SCHEDULER_ACTION_ROOM_TEMP_MODERATE_DEC) {
    struct ThermostatMessage tm;
    memset(&tm, 0, sizeof(struct ThermostatMessage));
    tm.msgType = THERMOSTAT_CMD_ROOM_TEMP_MODERATE_DEC;

    if (xQueueSend( thermostatQueue
                    ,( void * )&tm
                    ,THERMOSTAT_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send tm to thermostatQueue");
    }

#endif // CONFIG_MQTT_THERMOSTATS_NB > 0

  }
}

void handle_scheduler(void* pvParameters)
{
  ESP_LOGI(TAG, "handle_scheduler task started");

  init_scheduler();
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
          if (scheduler_data[sid].schedulerStatus == SCHEDULER_STATUS_ON) {
            short dow = 1 << timeinfo.tm_wday;
            ESP_LOGI(TAG, "Scheduler dow: %d", scheduler_data[sid].schedulerDow);
            if (scheduler_data[sid].schedulerDow & dow || scheduler_data[sid].schedulerDow == 0) {
              timeinfo.tm_hour = scheduler_data[sid].schedulerTime.hour;
              timeinfo.tm_min = scheduler_data[sid].schedulerTime.minute;
              timeinfo.tm_sec = 0;
              time_t schedulerTimeNow = mktime(&timeinfo);

              struct tm ti;
              memset(&ti, 0, sizeof(struct tm));
              localtime_r(&schedulerTimeNow, &ti);
              char strfti_buf[64];
              strftime(strfti_buf, sizeof(strfti_buf), "%c", &timeinfo);
              ESP_LOGI(TAG, "Scheduler time is: %s", strfti_buf);
              if (lastNow < schedulerTimeNow && schedulerTimeNow <= tempSchedulerCfg.data.now) {
                executeAction(scheduler_data[sid].schedulerAction);
                if(scheduler_data[sid].schedulerDow == 0) {
                  scheduler_data[sid].schedulerStatus = SCHEDULER_STATUS_OFF;
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
        if (scheduler_data[schedulerId].schedulerAction != tempSchedulerCfg.data.action)
        {
          scheduler_data[schedulerId].schedulerAction = tempSchedulerCfg.data.action;
          esp_err_t err = write_nvs_short(schedulerActionTAG[schedulerId],
                                          scheduler_data[schedulerId].schedulerAction);
          ESP_ERROR_CHECK_WITHOUT_ABORT( err );
        }
        publish_scheduler_action_evt(schedulerId);
      } else if (tempSchedulerCfg.msgType == SCHEDULER_CMD_DOW) {
        unsigned char schedulerId = tempSchedulerCfg.schedulerId;
        if (schedulerId > 1) {
          ESP_LOGE(TAG, "Unexpected schedulerId: %d",
            schedulerId);
          continue;
        }
        if (scheduler_data[schedulerId].schedulerDow  != tempSchedulerCfg.data.dow)
        {
          scheduler_data[schedulerId].schedulerDow = tempSchedulerCfg.data.dow;

          esp_err_t err = write_nvs_short(schedulerDowTAG[schedulerId],
                                          scheduler_data[schedulerId].schedulerDow);
          ESP_ERROR_CHECK_WITHOUT_ABORT( err );
        }
        publish_scheduler_dow_evt(schedulerId);
      } else if (tempSchedulerCfg.msgType == SCHEDULER_CMD_TIME) {
        unsigned char schedulerId = tempSchedulerCfg.schedulerId;
        if (schedulerId > 1) {
          ESP_LOGE(TAG, "Unexpected schedulerId: %d",
            schedulerId);
          continue;
        }
        if (scheduler_data[schedulerId].schedulerTime.hour   != tempSchedulerCfg.data.time.hour ||
            scheduler_data[schedulerId].schedulerTime.minute != tempSchedulerCfg.data.time.minute)
        {
          scheduler_data[schedulerId].schedulerTime = tempSchedulerCfg.data.time;

          char data[16];
          memset(data,0,16);
          sprintf(data, "%d:%d", scheduler_data[schedulerId].schedulerTime.hour, scheduler_data[schedulerId].schedulerTime.minute);
          esp_err_t err = write_nvs_str(schedulerTimeTAG[schedulerId],
                                          data);
          ESP_ERROR_CHECK_WITHOUT_ABORT( err );
        }
        publish_scheduler_time_evt(schedulerId);
      } else if (tempSchedulerCfg.msgType == SCHEDULER_CMD_STATUS) {
        unsigned char schedulerId = tempSchedulerCfg.schedulerId;
        if (schedulerId > 1) {
          ESP_LOGE(TAG, "Unexpected schedulerId: %d",
            schedulerId);
          continue;
        }
        if (scheduler_data[schedulerId].schedulerStatus != tempSchedulerCfg.data.status)
        {
          scheduler_data[schedulerId].schedulerStatus = tempSchedulerCfg.data.status;
          esp_err_t err = write_nvs_short(schedulerStatusTAG[schedulerId],
                                          scheduler_data[schedulerId].schedulerStatus);
          ESP_ERROR_CHECK_WITHOUT_ABORT( err );
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
