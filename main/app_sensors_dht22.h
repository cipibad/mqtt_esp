#ifndef APP_SENSORS_DHT22_H
#define APP_SENSORS_DHT22_H

#include <stdint.h>

#include "app_sensors.h"

void dht22_init(void);
void dht22_read(void);
void dht22_publish(void);
void dht22_publish_ha(void);

#endif // APP_SENSORS_DHT22_H
