#ifndef APP_SENSORS_DS18X20_H
#define APP_SENSORS_DS18X20_H

#include <stdint.h>

#include "app_sensors.h"

void ds18x20_init(void);
void ds18x20_read(void);
void ds18x20_publish(void);
void ds18x20_publish_ha(int sensor_id);
int ds18x20_get_sensor_count(void);

#endif // APP_SENSORS_DS18X20_H

