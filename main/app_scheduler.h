#ifndef APP_SCHEDULER_H
#define APP_SCHEDULER_H

#include "app_relay.h"

void start_scheduler_timer(void);
void handle_scheduler(void* pvParameters);

#define RELAY_ACTION 1
#define ADD_RELAY_ACTION 1
#define TRIGGER_ACTION 255
#define MAX_SCHEDULER_NB 8

#define ACTION_STATE_DISABLED 0
#define ACTION_STATE_ENABLED 1

//FIXME basic structure only

union Data {
  struct RelayCmdMessage relayActionData;
  struct TriggerData {time_t now;} triggerActionData;
};

struct SchedulerCfgMessage
{
  unsigned char schedulerId;
  time_t timestamp;
  unsigned char actionId;
  unsigned char actionState;
  union Data data;
};


#endif /* APP_SCHEDULER_H */
