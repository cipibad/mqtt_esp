#ifndef APP_SCHEDULER_H
#define APP_SCHEDULER_H

void start_scheduler(void);

//FIXME basic structure only
struct SchedulerCfgMessage
{
  char schedulerTimestamp;
  char schedulerRelayId;
  char schedulerRelayState;
};


#endif /* APP_SCHEDULER_H */
