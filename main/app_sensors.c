#include "esp_system.h"
#ifdef CONFIG_SENSOR_SUPPORT

#include <limits.h>
#include <string.h>
#include "esp_log.h"

#include "driver/gpio.h"
#include "rom/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "app_main.h"
#include "app_sensors.h"

#if CONFIG_MQTT_THERMOSTATS_NB > 0
#include "app_thermostat.h"
#endif // CONFIG_MQTT_THERMOSTATS_NB > 0

#include "app_publish_data.h"

#ifdef CONFIG_DHT22_SENSOR_SUPPORT
#include "dht.h"

#ifdef CONFIG_DHT22_SENSOR_TYPE_AM2301
#define DHT_SENSOR_TYPE DHT_TYPE_AM2301
#endif // CONFIG_DHT22_SENSOR_TYPE_AM2301

#ifdef CONFIG_DHT22_SENSOR_TYPE_SI7021
#define DHT_SENSOR_TYPE DHT_TYPE_SI7021
#endif // CONFIG_DHT22_SENSOR_TYPE_SI7021

#define MEAN_FACTOR 5

short dht22_mean_temperature = SHRT_MIN;
short dht22_temperature = SHRT_MIN;
short dht22_mean_humidity = SHRT_MIN;
short dht22_humidity = SHRT_MIN;
#endif //CONFIG_DHT22_SENSOR_SUPPORT

#ifdef CONFIG_DS18X20_SENSOR
#include <ds18x20.h>
#define MAX_SENSORS 8
static const gpio_num_t SENSOR_GPIO = CONFIG_DS18X20_SENSOR_GPIO;
ds18x20_addr_t addrs[MAX_SENSORS];
float temps[MAX_SENSORS];
int sensor_count = 0;
#endif // CONFIG_DS18X20_SENSOR


#ifdef CONFIG_BME280_SENSOR
#include "bme280.h"
#include "app_bme280.h"
int32_t bme280_pressure;
int32_t bme280_temperature;
int32_t bme280_humidity;
#endif //CONFIG_BME280_SENSOR

static const char *TAG = "app_sensors";

void publish_data_to_thermostat(const char * topic, int value)
{
#ifdef CONFIG_MQTT_THERMOSTATS_NB0_SENSOR_TYPE_LOCAL
  if (strncmp(topic, CONFIG_MQTT_THERMOSTATS_NB0_LOCAL_SENSOR_TOPIC, strlen(CONFIG_MQTT_THERMOSTATS_NB0_LOCAL_SENSOR_TOPIC)) == 0) {
    thermostat_publish_local_data(0, value);
  }
#endif //CONFIG_MQTT_THERMOSTATS_NB0_SENSOR_TYPE_MQTT

#ifdef CONFIG_MQTT_THERMOSTATS_NB1_SENSOR_TYPE_LOCAL
  if (strncmp(topic, CONFIG_MQTT_THERMOSTATS_NB1_LOCAL_SENSOR_TOPIC, strlen(CONFIG_MQTT_THERMOSTATS_NB1_LOCAL_SENSOR_TOPIC)) == 0) {
    thermostat_publish_local_data(1, value);
  }
#endif //CONFIG_MQTT_THERMOSTATS_NB1_SENSOR_TYPE_MQTT

#ifdef CONFIG_MQTT_THERMOSTATS_NB2_SENSOR_TYPE_LOCAL
  if (strncmp(topic, CONFIG_MQTT_THERMOSTATS_NB2_LOCAL_SENSOR_TOPIC, strlen(CONFIG_MQTT_THERMOSTATS_NB2_LOCAL_SENSOR_TOPIC)) == 0) {
    thermostat_publish_local_data(2, value);
  }
#endif //CONFIG_MQTT_THERMOSTATS_NB2_SENSOR_TYPE_MQTT

#ifdef CONFIG_MQTT_THERMOSTATS_NB3_SENSOR_TYPE_LOCAL
  if (strncmp(topic, CONFIG_MQTT_THERMOSTATS_NB3_LOCAL_SENSOR_TOPIC, strlen(CONFIG_MQTT_THERMOSTATS_NB3_LOCAL_SENSOR_TOPIC)) == 0) {
    thermostat_publish_local_data(3, value);
  }
#endif //CONFIG_MQTT_THERMOSTATS_NB3_SENSOR_TYPE_MQTT

}

void publish_sensor_data(const char * topic, int value)
{

  publish_data_to_thermostat(topic, value);

  char data[16];
  memset(data,0,16);
  sprintf(data, "%d.%d", value / 10, abs(value % 10));
  publish_non_persistent_data(topic, data);
}

#ifdef CONFIG_DHT22_SENSOR_SUPPORT
void publish_dht22_log(const char* log_message)
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/log/dht22";
  publish_non_persistent_data(topic, log_message);

}

void publish_dht22_mean_temperature()
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/temperature/dht22";
  publish_sensor_data(topic, dht22_mean_temperature);
}

void publish_dht22_mean_humidity()
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/humidity/dht22";
  publish_sensor_data(topic, dht22_mean_humidity);
}

void publish_dht22_data()
{
  publish_dht22_mean_temperature();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  publish_dht22_mean_humidity();
  vTaskDelay(50 / portTICK_PERIOD_MS);
}
#endif // CONFIG_DHT22_SENSOR_SUPPORT

#ifdef CONFIG_DS18X20_SENSOR
void publish_ds18x20_temperature(int sensor_id)
{
 const char * temperature_topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/temperature";

  char topic[MAX_TOPIC_LEN];
  memset(topic,0,MAX_TOPIC_LEN);
  sprintf(topic, "%s/%08x%08x", temperature_topic,
          (uint32_t)(addrs[sensor_id] >> 32),
          (uint32_t)addrs[sensor_id]);

  publish_sensor_data(topic, temps[sensor_id] * 10);
}

void publish_ds18x20_data()
{
  for(int i=0; i<sensor_count; i++) {
    publish_ds18x20_temperature(i);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}
#endif // CONFIG_DS18X20_SENSOR

#ifdef CONFIG_BME280_SENSOR
void publish_bme280_temperature()
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/temperature/bme280";
  publish_sensor_data(topic, bme280_temperature/10);
}

void publish_bme280_humidity()
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/humidity/bme280";
  publish_sensor_data(topic, bme280_humidity/100);
}

void publish_bme280_pressure()
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/pressure/bme280";

  publish_sensor_data(topic, bme280_pressure*7.50061683);
}

void publish_bme280_data()
{
  publish_bme280_temperature();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  publish_bme280_humidity();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  publish_bme280_pressure();
  vTaskDelay(50 / portTICK_PERIOD_MS);
}
#endif // CONFIG_BME280_SENSOR

void publish_sensors_data()
{
#ifdef CONFIG_DHT22_SENSOR_SUPPORT
  void publish_dht22_data();
#endif // CONFIG_DHT22_SENSOR_SUPPORT

#ifdef CONFIG_DS18X20_SENSOR
  publish_ds18x20_data();
#endif // CONFIG_DS18X20_SENSOR

#ifdef CONFIG_BME280_SENSOR
  void publish_bme280_data();
#endif // CONFIG_BME280_SENSOR
}

void sensors_read(void* pvParameters)
{

#ifdef CONFIG_BME280_SENSOR
  //Don't forget to connect SDO to Vio too

	struct bme280_t bme280 = {
		.bus_write = BME280_I2C_bus_write,
		.bus_read = BME280_I2C_bus_read,
		.dev_addr = BME280_I2C_ADDRESS2,
		.delay_msec = BME280_delay_msek
	};
	esp_err_t err = BME280_I2C_init(&bme280,
                                  CONFIG_BME280_SENSOR_SDA_GPIO,
                                  CONFIG_BME280_SENSOR_SCL_GPIO);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Cannot init bme280 sensor");
  }
#endif //CONFIG_BME280_SENSOR
#ifdef CONFIG_DHT22_SENSOR_SUPPORT
  gpio_pad_select_gpio(CONFIG_DHT22_SENSOR_GPIO);
  gpio_set_direction(CONFIG_DHT22_SENSOR_GPIO, GPIO_MODE_OUTPUT_OD);
  gpio_set_level(CONFIG_DHT22_SENSOR_GPIO, 1);
#endif //CONFIG_DHT22_SENSOR_SUPPORT

  while (1)
    {
#ifdef CONFIG_DHT22_SENSOR_SUPPORT
      dht22_temperature = SHRT_MIN;
      dht22_humidity = SHRT_MIN;
      if (dht_read_data(DHT_SENSOR_TYPE, CONFIG_DHT22_SENSOR_GPIO, &dht22_humidity, &dht22_temperature) == ESP_OK)
        {
          if (dht22_mean_temperature == SHRT_MIN) {
            dht22_mean_temperature = dht22_temperature;
          } else {
            dht22_mean_temperature = (((CONFIG_DHT22_SENSOR_SMA_FACTOR - 1) * dht22_mean_temperature)
              + dht22_temperature) / CONFIG_DHT22_SENSOR_SMA_FACTOR;
          }

          if (dht22_mean_humidity == SHRT_MIN) {
            dht22_mean_humidity = dht22_humidity;
          } else {
            dht22_mean_humidity = (((CONFIG_DHT22_SENSOR_SMA_FACTOR - 1) * dht22_mean_humidity)
              + dht22_humidity) / CONFIG_DHT22_SENSOR_SMA_FACTOR;
          }

          ESP_LOGI(TAG, "Humidity: %d.%d%% Temp: %d.%dC",
                   dht22_mean_humidity/10, abs(dht22_mean_humidity%10) ,
                   dht22_mean_temperature/10, abs(dht22_mean_temperature%10));
          publish_dht22_data();
        }
      else
        {
          ESP_LOGE(TAG, "Error: Could not read data from DHT sensor");
          publish_dht22_log( "Error: Could not read data from DHT sensor");
        }
#endif //CONFIG_DHT22_SENSOR_SUPPORT

#ifdef CONFIG_DS18X20_SENSOR
      sensor_count = ds18x20_scan_devices(SENSOR_GPIO, addrs, MAX_SENSORS);
      if (sensor_count < 1) {
        ESP_LOGW(TAG, "No sensors detected!\n");
      } else {
        ds18x20_measure_and_read_multi(SENSOR_GPIO, addrs, sensor_count, temps);
        for (int j = 0; j < sensor_count; j++)
          {
            // The ds18x20 address is a 64-bit integer, but newlib-nano
            // printf does not support printing 64-bit values, so we
            // split it up into two 32-bit integers and print them
            // back-to-back to make it look like one big hex number.
            char addr[8+8+1];
            sprintf(addr, "%08x", (uint32_t)(addrs[j] >> 32));
            sprintf(addr + 8, "%08x", (uint32_t)addrs[j]);
            short temp_c = (short)(temps[j] * 10);
            ESP_LOGI(TAG,"Sensor %s reports %d.%dC", addr, temp_c/10, abs(temp_c%10));
          }
        publish_ds18x20_data();

      }
#endif // CONFIG_DS18X20_SENSOR


#ifdef CONFIG_BME280_SENSOR
      if (bme_read_data(&bme280_temperature, &bme280_pressure, &bme280_humidity) == ESP_OK)
        {
          ESP_LOGI(TAG, "Temp: %d.%02dC, Pressure: %d, Humidity: %d.%03d%%, ",  bme280_temperature/100,bme280_temperature%100, bme280_pressure, bme280_humidity/1000, bme280_humidity%1000);
          publish_bme280_data();
        }
      else
        {
          ESP_LOGE(TAG, "Could not read data from BME sensor");
        }
#endif //CONFIG_BME280_SENSOR

      vTaskDelay(CONFIG_SENSOR_READING_INTERVAL * 1000 / portTICK_PERIOD_MS);
    }
}

#endif // CONFIG_SENSOR_SUPPORT
