#ifndef APP_RELAY_H
#define APP_RELAY_H

#include "MQTTClient.h"


void relays_init(void);
void publish_relay_data(MQTTClient* pClient);
int handle_relay_cmd(MQTTClient* pEvent);

#endif /* APP_RELAY_H */
