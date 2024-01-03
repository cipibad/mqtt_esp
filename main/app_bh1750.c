#include "esp_system.h"
#ifdef CONFIG_BH1750_SENSOR

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"

#include "app_i2c.h"
#include "app_bh1750.h"

#define BH1750_SENSOR_ADDR_LOW 0x23
#define BH1750_SENSOR_ADDR_HIGH 0x5C

#define BH1750_SENSOR_ADDR BH1750_SENSOR_ADDR_LOW   /*!< slave address for BH1750 sensor */

#define BH1750_CONTINU_H_RESOLUTION 0x10
#define BH1750_CONTINU_H_RESOLUTION2 0x11
#define BH1750_CONTINU_L_RESOLUTION 0x13
#define BH1750_ONETIME_H_RESOLUTION 0x20
#define BH1750_ONETIME_H_RESOLUTION2 0x21
#define BH1750_ONETIME_L_RESOLUTION 0x23

#define BH1750_SENSOR_OPERATION_MODE BH1750_ONETIME_L_RESOLUTION   /*!< Operation mode */

/**
 * @brief test code to operate on BH1750 sensor
 *
 * 1. set operation mode(e.g One time L-resolution mode)
 * _________________________________________________________________
 * | start | slave_addr + wr_bit + ack | write 1 byte + ack  | stop |
 * --------|---------------------------|---------------------|------|
 * 2. wait more than 24 ms
 * 3. read data
 * ______________________________________________________________________________________
 * | start | slave_addr + rd_bit + ack | read 1 byte + ack  | read 1 byte + nack | stop |
 * --------|---------------------------|--------------------|--------------------|------|
 */
esp_err_t i2c_master_BH1750_read(uint16_t *data)
{
    int ret;
    uint8_t data_h,data_l;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, BH1750_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, BH1750_SENSOR_OPERATION_MODE, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(30 / portTICK_RATE_MS);
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, BH1750_SENSOR_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &data_h, ACK_VAL);
    i2c_master_read_byte(cmd, &data_l, NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    *data = 10 * (data_h << 8 | data_l)/ 12;
    return ret;
}

#endif // CONFIG_BH1750_SENSOR