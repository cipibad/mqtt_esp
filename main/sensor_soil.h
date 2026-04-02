#ifndef SENSOR_SOIL_H
#define SENSOR_SOIL_H

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC
void soil_adc_init(void);
void soil_adc_read_and_publish(void);
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL
void soil_digital_init(void);
void soil_digital_read_and_publish(void);
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_SWITCH
void soil_switch_init(void);
void soil_switch_on(void);
void soil_switch_off(void);
#endif

#endif // SENSOR_SOIL_H
