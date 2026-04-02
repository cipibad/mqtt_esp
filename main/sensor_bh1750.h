#ifndef SENSOR_BH1750_H
#define SENSOR_BH1750_H

#ifdef CONFIG_BH1750_SENSOR

void bh1750_sensor_init(void);
void bh1750_read_and_publish(void);

#endif // CONFIG_BH1750_SENSOR

#endif // SENSOR_BH1750_H
