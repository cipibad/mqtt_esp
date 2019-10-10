#ifndef APP_SCHEDULER_H
#define APP_SCHEDULER_H

#include "app_relay.h"

void start_scheduler_timer(void);
void handle_scheduler(void* pvParameters);

#define RELAY_ACTION 1
#define TRIGGER_ACTION 255
#define MAX_SCHEDULER_NB 8
//FIXME basic structure only

union Data {
  struct RelayCmdMessage relayData;
};

struct SchedulerCfgMessage
{
  unsigned char schedulerId;
  unsigned int timestamp;
  unsigned char actionId;
  union Data data;
};


#endif /* APP_SCHEDULER_H */
