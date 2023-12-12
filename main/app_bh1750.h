#ifndef APP_BH1750_H
#define APP_BH1750_H

esp_err_t i2c_master_BH1750_init();
esp_err_t i2c_master_BH1750_read(uint16_t *data);

#endif /* APP_BH1750_H */
