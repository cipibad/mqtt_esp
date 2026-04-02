#ifndef SENSOR_DS18X20_H
#define SENSOR_DS18X20_H

#ifdef CONFIG_DS18X20_SENSOR

#include <ds18x20.h>

#define DS18X20_MAX_SENSORS 8

extern ds18x20_addr_t ds18x20_addrs[DS18X20_MAX_SENSORS];
extern float ds18x20_temps[DS18X20_MAX_SENSORS];
extern int ds18x20_sensor_count;

void ds18x20_sensor_init(void);
void ds18x20_read_and_publish(void);

#endif // CONFIG_DS18X20_SENSOR

#endif // SENSOR_DS18X20_H
