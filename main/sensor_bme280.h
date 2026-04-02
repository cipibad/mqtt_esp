#ifndef SENSOR_BME280_H
#define SENSOR_BME280_H

#ifdef CONFIG_BME280_SENSOR

void bme280_sensor_init(void);
void bme280_read_and_publish(void);

#endif // CONFIG_BME280_SENSOR

#endif // SENSOR_BME280_H
