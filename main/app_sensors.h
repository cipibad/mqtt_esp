#ifndef APP_SENSORS_H
#define APP_SENSORS_H

void sensors_read(void* pvParameters);
#ifdef ESP8266
#include "MQTTClient.h"
void mqtt_publish_sensor_data(MQTTClient* pClient);
#endif //ESP8266

#endif /* APP_SENSORS_H */
