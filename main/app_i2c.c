#include "esp_system.h"
#ifdef CONFIG_I2C_SENSOR_SUPPORT

#include "app_i2c.h"
/**
 * @brief i2c master initialization
 */
esp_err_t i2c_master_init()
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = CONFIG_I2C_SENSOR_SDA_GPIO;
    conf.sda_pullup_en = 1;
    conf.scl_io_num = CONFIG_I2C_SENSOR_SCL_GPIO;
    conf.scl_pullup_en = 1;
    conf.clk_stretch_tick = CONFIG_I2C_SENSOR_CLK_STRETCH_TICK; // 300 ticks, Clock stretch is about 210us, you can make changes according to the actual situation.
    #ifdef CONFIG_TARGET_DEVICE_ESP32
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0));
    #else //CONFIG_TARGET_DEVICE_ESP32
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_driver_install(i2c_master_port, conf.mode));
    #endif //CONFIG_TARGET_DEVICE_ESP32
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_param_config(i2c_master_port, &conf));
    return ESP_OK;
}

#endif // CONFIG_I2C_SENSOR_SUPPORT
