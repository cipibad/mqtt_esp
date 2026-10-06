#ifndef APP_WATERPUMP_H
#define APP_WATERPUMP_H

#define WATERPUMP_STATUS_INIT                 0
#define WATERPUMP_STATUS_OFF                  1
#define WATERPUMP_STATUS_OFF_ON_TRANSITION    2
#define WATERPUMP_STATUS_ON                   3
#define WATERPUMP_STATUS_ON_OFF_TRANSITION    4

#define GPIO_STATUS_OFF   0
#define GPIO_STATUS_ON    1

extern int waterPumpStatus;

void initWaterPump();
static inline int getWaterPumpStatus(){
      return waterPumpStatus;
}
void updateWaterPumpState(int);
void publish_waterpump_status();
#if CONFIG_WATERPUMP_ENABLE_NOTIFICATIONS
void publish_waterpump_notification_evt(const char* msg);
#endif // CONFIG_WATERPUMP_ENABLE_NOTIFICATIONS

#endif // AP_WATERPUMP_H