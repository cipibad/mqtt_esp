#include "esp_system.h"
#ifdef CONFIG_BME280_SENSOR

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"

#include "sensor_common.h"
#include "sensor_bme280.h"
#include "app_main.h"
#include "app_publish_data.h"
#include "app_i2c.h"
#include "bme280.h"
#include "app_bme280.h"

static const char *TAG = "SENSOR_BME280";

static sensor_error_state_t bme280_error_state = {
    .health = SENSOR_HEALTH_UNKNOWN, 
    .last_published_health = SENSOR_HEALTH_UNKNOWN
};

static const sensor_value_range_t bme280_temp_range = {-400, 850};     // -40.0°C to 85.0°C
static const sensor_value_range_t bme280_humidity_range = {0, 1000};   // 0% to 100%

int32_t bme280_pressure;
int32_t bme280_temperature;
int32_t bme280_humidity;

static void publish_bme280_temperature(void)
{
    const char *topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/temperature/bme280";
    publish_sensor_data(topic, bme280_temperature/10);
}

static void publish_bme280_humidity(void)
{
    const char *topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/humidity/bme280";
    publish_sensor_data(topic, bme280_humidity/100);
}

static void publish_bme280_pressure(void)
{
    const char *topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/pressure/bme280";
    publish_sensor_data(topic, bme280_pressure/10);
}

static void publish_bme280_data(void)
{
    publish_bme280_temperature();
    vTaskDelay(50 / portTICK_PERIOD_MS);
    publish_bme280_humidity();
    vTaskDelay(50 / portTICK_PERIOD_MS);
    publish_bme280_pressure();
    vTaskDelay(50 / portTICK_PERIOD_MS);
}

void bme280_sensor_init(void)
{
    struct bme280_t bme280 = {
        .bus_write = BME280_I2C_bus_write,
        .bus_read = BME280_I2C_bus_read,
        .dev_addr = BME280_I2C_ADDRESS1,
        .delay_msec = BME280_delay_msek
    };
    esp_err_t err = BME280_I2C_init(&bme280,
                                    CONFIG_I2C_SENSOR_SDA_GPIO,
                                    CONFIG_I2C_SENSOR_SCL_GPIO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot init bme280 sensor");
    }
}

void bme280_read_and_publish(void)
{
    if (bme_read_data(&bme280_temperature, &bme280_pressure, &bme280_humidity) == ESP_OK) {
        short temp_c = bme280_temperature / 10;
        short humidity_pct = bme280_humidity / 100;
        if (sensor_validate_value(temp_c, &bme280_temp_range) &&
            sensor_validate_value(humidity_pct, &bme280_humidity_range)) {
            ESP_LOGI(TAG, "Temp: %d.%02d degC, Pressure: %d.%02d mmHg , Humidity: %d.%03d %%rH, ",
              bme280_temperature / 100, bme280_temperature % 100,
              bme280_pressure / 100, bme280_pressure % 100,
              bme280_humidity / 1000, bme280_humidity % 1000);
            sensor_record_success(&bme280_error_state);
            publish_bme280_data();
        } else {
            sensor_record_error(&bme280_error_state);
            ESP_LOGW(TAG, "Values out of range, skipping");
        }
    } else {
        sensor_record_error(&bme280_error_state);
        ESP_LOGE(TAG, "Could not read data from BME sensor");
    }
    
    sensor_publish_health_if_changed(&bme280_error_state, "bme280");
}

#endif // CONFIG_BME280_SENSOR
