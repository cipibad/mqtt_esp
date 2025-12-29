#ifndef APP_SENSORS_BME280_H
#define APP_SENSORS_BME280_H

#include "app_sensors.h"

void bme280_init(void);
void bme280_read(void);
void bme280_publish(void);
void bme280_publish_ha(void);

#endif // APP_SENSORS_BME280_H
