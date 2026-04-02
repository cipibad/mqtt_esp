#ifndef SENSOR_DHT22_H
#define SENSOR_DHT22_H

#ifdef CONFIG_DHT22_SENSOR_SUPPORT

extern short dht22_mean_temperature;
extern short dht22_mean_humidity;

void dht22_sensor_init(void);
void dht22_read_and_publish(void);

#endif // CONFIG_DHT22_SENSOR_SUPPORT

#endif // SENSOR_DHT22_H
