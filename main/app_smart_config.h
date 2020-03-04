#ifndef APP_SMART_CONFIG_H
#define APP_SMART_CONFIG_H

struct SmartConfigMessage
{
    int ticks;
    unsigned char relayId;
};

void smartconfig_cmd_task(void* pvParameters);

#endif /* APP_SMART_CONFIG_H */
