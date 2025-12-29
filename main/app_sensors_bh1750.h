#ifndef APP_SENSORS_BH1750_H
#define APP_SENSORS_BH1750_H

#include "app_sensors.h"

void bh1750_init(void);
void bh1750_read(void);
void bh1750_publish(void);
void bh1750_publish_ha(void);

#endif // APP_SENSORS_BH1750_H
