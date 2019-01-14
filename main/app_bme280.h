#ifndef APP_BME280_H
#define APP_BME280_H

/* esp_err_t i2c_master_init(void); */
/* void task_bme280_normal_mode(void *ignore); */

s8 BME280_I2C_bus_write(u8 dev_addr, u8 reg_address, u8 *data, u8 data_len);
s8 BME280_I2C_bus_read(u8 dev_addr, u8 reg_address, u8 *data, u8 data_len);
void BME280_delay_msek(u32 msek);
esp_err_t BME280_I2C_init(struct bme280_t *bme280, const int sda_pin, const int scl_pin);
esp_err_t bme_read_data(int32_t *temperature, int32_t *pressure, int32_t *humidity);

#endif /* APP_BME280_H */
