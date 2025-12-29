#ifndef APP_SENSORS_H
#define APP_SENSORS_H

void sensors_read(void* pvParameters);
void publish_sensors_data();

#ifdef CONFIG_BME280_SENSOR
void publish_bme280_ha_data();
#endif

#ifdef CONFIG_DHT22_SENSOR_SUPPORT
void publish_dht22_ha_data();
#endif

#ifdef CONFIG_DS18X20_SENSOR
void publish_ds18x20_ha_data(int sensor_id);
#endif

#ifdef CONFIG_BH1750_SENSOR
void publish_bh1750_ha_data();
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC
void publish_soil_moisture_adc_ha_data();
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL
void publish_soil_moisture_th_ha_data();
#endif

void publish_log_message(const char *level, const char *module, const char *message);
void publish_error_log(const char *module, const char *format, ...);
void publish_warning_log(const char *module, const char *format, ...);
void publish_info_log(const char *module, const char *format, ...);

#ifdef CONFIG_MQTT_LOG_LEVEL_DEBUG
void publish_debug_log(const char *module, const char *format, ...);
#endif

#endif /* APP_SENSORS_H */
