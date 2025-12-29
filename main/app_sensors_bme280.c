#include <esp_system.h>
#ifdef CONFIG_BME280_SENSOR

#include <limits.h>
#include <string.h>

#include "freertos/FreeRTOS.h"

#include "esp_log.h"

#include "app_main.h"
#include "app_sensors_bme280.h"
#include "app_publish_data.h"
#include "app_logging.h"
#include "bme280.h"
#include "app_bme280.h"
#include "app_i2c.h"

static int32_t bme280_pressure;
static int32_t bme280_temperature;
static int32_t bme280_humidity;

void bme280_init(void)
{
  ESP_ERROR_CHECK(i2c_master_init(CONFIG_I2C_SENSOR_SDA_GPIO, CONFIG_I2C_SENSOR_SCL_GPIO));

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
    LOGE(LOG_MODULE_BME280, "Cannot init bme280 sensor");
  }
}

void bme280_read(void)
{
  if (bme_read_data(&bme280_temperature, &bme280_pressure, &bme280_humidity) == ESP_OK)
    {
      LOGI(LOG_MODULE_BME280, "Temp: %d.%02d degC, Pressure: %d.%02d mmHg , Humidity: %d.%03d %%rH, ",
            bme280_temperature / 100, bme280_temperature % 100,
            bme280_pressure / 100, bme280_pressure % 100,
            bme280_humidity / 1000, bme280_humidity % 1000);
    }
   else
    {
      LOGE(LOG_MODULE_BME280, "Could not read data from BME sensor");
    }
}

static void publish_bme280_temperature(void)
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/temperature/bme280";
  publish_sensor_data(topic, bme280_temperature/10);
}

static void publish_bme280_humidity(void)
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/humidity/bme280";
  publish_sensor_data(topic, bme280_humidity/100);
}

static void publish_bme280_pressure(void)
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/pressure/bme280";
  publish_sensor_data(topic, bme280_pressure/10);
}

void bme280_publish(void)
{
  publish_bme280_temperature();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  publish_bme280_humidity();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  publish_bme280_pressure();
  vTaskDelay(50 / portTICK_PERIOD_MS);
}

void bme280_publish_ha(void)
{
  const char * topic = CONFIG_HOME_NAME "/" CONFIG_ROOM_NAME "/sensor";
  char payload[128];
  
  sprintf(payload, "{\"source\":\"bme280\",\"temperature\":%d.%02d,\"humidity\":%d.%02d,\"pressure\":%d}",
          bme280_temperature / 100, abs(bme280_temperature % 100),
          bme280_humidity / 1000, abs(bme280_humidity % 1000),
          bme280_pressure / 100);
  
  publish_non_persistent_data(topic, payload);
}

#endif // CONFIG_BME280_SENSOR
