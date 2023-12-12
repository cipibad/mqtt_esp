#include "esp_system.h"
#ifdef CONFIG_BH1750_SENSOR

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"

#include "driver/i2c.h"

#include "app_bh1750.h"

#define I2C_MASTER_SCL_IO           5                /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           4               /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0        /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0                /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                /*!< I2C master do not need buffer */

#define WRITE_BIT                           I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT                            I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN                        0x1              /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS                       0x0              /*!< I2C master will not check ack from slave */
#define ACK_VAL                             0x0              /*!< I2C ack value */
#define NACK_VAL                            0x1              /*!< I2C nack value */
#define LAST_NACK_VAL                       0x2              /*!< I2C last_nack value */

            // default 0x5C if BH1750_I2C_ADDRESS_High
            // default 0x23 if BH1750_I2C_ADDRESS_LOW

#define BH1750_SENSOR_ADDR 0x23   /*!< slave address for BH1750 sensor */

            // default 0x10 if BH1750_CONTINU_H_RESOLUTION
            // default 0x11 if BH1750_CONTINU_H_RESOLUTION2
            // default 0x13 if BH1750_CONTINU_L_RESOLUTION
            // default 0x20 if BH1750_ONETIME_H_RESOLUTION
            // default 0x21 if BH1750_ONETIME_H_RESOLUTION2
            // default 0x23 if BH1750_ONETIME_L_RESOLUTION
#define BH1750_CMD_START 0x23   /*!< Operation mode */

/**
 * @brief i2c master initialization
 */
esp_err_t i2c_master_BH1750_init()
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = 1;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = 1;
    conf.clk_stretch_tick = 300; // 300 ticks, Clock stretch is about 210us, you can make changes according to the actual situation.
    #ifdef CONFIG_TARGET_DEVICE_ESP32
    ESP_ERROR_CHECK(i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0));
    #else //CONFIG_TARGET_DEVICE_ESP32
    ESP_ERROR_CHECK(i2c_driver_install(i2c_master_port, conf.mode));
    #endif //CONFIG_TARGET_DEVICE_ESP32
    ESP_ERROR_CHECK(i2c_param_config(i2c_master_port, &conf));
    return ESP_OK;
    return ESP_OK;
}


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
    i2c_master_write_byte(cmd, BH1750_CMD_START, ACK_CHECK_EN);
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