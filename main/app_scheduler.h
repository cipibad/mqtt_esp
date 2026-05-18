#ifndef APP_SCHEDULER_H
#define APP_SCHEDULER_H

void start_scheduler_timer(void);
void handle_scheduler(void* pvParameters);

#define MAX_SCHEDULER_NB 2

//FIXME basic structure only

enum SchedulerCfgMessageType {
  SCHEDULER_CMD_ACTION,
  SCHEDULER_CMD_DOW,
  SCHEDULER_CMD_TIME,
  SCHEDULER_CMD_STATUS,
  SCHEDULER_CMD_TRIGGER,
};

enum SchedulerAction {
  SCHEDULER_ACTION_UNSET,
  SCHEDULER_ACTION_RELAY_ON,
  SCHEDULER_ACTION_RELAY_OFF,
  SCHEDULER_ACTION_WATER_TEMP_LOW,
  SCHEDULER_ACTION_WATER_TEMP_HIGH,
  SCHEDULER_ACTION_OW_ON,
  SCHEDULER_ACTION_OW_OFF,
};

#define SCHEDULER_TIME_DOW_SUNDAY     1
#define SCHEDULER_TIME_DOW_MONDAY     2
#define SCHEDULER_TIME_DOW_TUESDAY    4
#define SCHEDULER_TIME_DOW_WEDNESDAY  8
#define SCHEDULER_TIME_DOW_THURSDAY  16
#define SCHEDULER_TIME_DOW_FRIDAY    32
#define SCHEDULER_TIME_DOW_SATURDAY  64

struct SchedulerTime {
  short hour;
  short minute;
};

enum SchedulerStatus {
  SCHEDULER_STATUS_UNSET,
  SCHEDULER_STATUS_ON,
  SCHEDULER_STATUS_OFF
};

union SchedulerCfgData {
  enum SchedulerAction action;
  short dow;
  struct SchedulerTime time;
  enum SchedulerStatus status;
  time_t now;
};

struct SchedulerCfgMessage {
  enum SchedulerCfgMessageType msgType;
  unsigned char schedulerId;
  union SchedulerCfgData data;
};

#define SCHEDULE_TIMEOUT 30
#define SCHEDULE_QUEUE_TIMEOUT (SCHEDULE_TIMEOUT * 1000 / portTICK_PERIOD_MS)

void publish_schedulers_data();

#endif /* APP_SCHEDULER_H */
