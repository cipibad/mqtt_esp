#ifndef APP_SENSORS_SOIL_ADC_H
#define APP_SENSORS_SOIL_ADC_H

#include "app_sensors.h"

void soil_moisture_adc_init(void);
void soil_moisture_adc_read(void);
void soil_moisture_adc_publish(void);
void soil_moisture_adc_publish_ha(void);

#endif // APP_SENSORS_SOIL_ADC_H
