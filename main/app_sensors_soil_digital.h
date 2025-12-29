#ifndef APP_SENSORS_SOIL_DIGITAL_H
#define APP_SENSORS_SOIL_DIGITAL_H

#include "app_sensors.h"

void soil_moisture_init(void);
void soil_moisture_read(void);
void soil_moisture_publish(void);
void soil_moisture_publish_ha(void);

#endif // APP_SENSORS_SOIL_DIGITAL_H
