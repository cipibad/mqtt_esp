#ifndef APP_WATERPUMP_H
#define APP_WATERPUMP_H

#define WATERPUMP_STATUS_OFF                 0
#define WATERPUMP_STATUS_ON                  1
#define WATERPUMP_STATUS_OFF_ON_TRANSITION   2
#define WATERPUMP_STATUS_ON_OFF_TRANSITION   3

#define GPIO_STATUS_OFF   0
#define GPIO_STATUS_ON    1


void initWaterPump();
void enableWaterPump();
void disableWaterPump();

#endif // AP_WATERPUMP_H