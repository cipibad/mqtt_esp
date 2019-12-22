#include "esp_system.h"
#ifdef CONFIG_MQTT_SENSOR

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

#include "app_mqtt.h"

#ifdef CONFIG_MQTT_SENSOR_DHT22
#include "dht.h"
short dht22_temperature = SHRT_MIN;
short dht22_humidity = SHRT_MIN;
#endif //CONFIG_MQTT_SENSOR_DHT22

#ifdef CONFIG_MQTT_SENSOR_DS18X20
#include <ds18x20.h>
#define MAX_SENSORS 8
static const gpio_num_t SENSOR_GPIO = CONFIG_MQTT_SENSOR_DS18X20_GPIO;
ds18x20_addr_t addrs[MAX_SENSORS];
float temps[MAX_SENSORS];
short wtemperature = SHRT_MIN;
short ctemperature = SHRT_MIN;
#endif // CONFIG_MQTT_SENSOR_DS18X20


#ifdef CONFIG_MQTT_SENSOR_BME280
#include "bme280.h"
#include "app_bme280.h"
int32_t bme280_pressure;
int32_t bme280_temperature;
int32_t bme280_humidity;
#endif //CONFIG_MQTT_SENSOR_BME280

#ifdef CONFIG_MQTT_THERMOSTAT
#include "app_thermostat.h"
extern QueueHandle_t thermostatQueue;
#endif // CONFIG_MQTT_THERMOSTAT

static const char *TAG = "app_sensors";

void sensors_read(void* pvParameters)
{

#ifdef CONFIG_MQTT_SENSOR_BME280
  //Don't forget to connect SDO to Vio too

	struct bme280_t bme280 = {
		.bus_write = BME280_I2C_bus_write,
		.bus_read = BME280_I2C_bus_read,
		.dev_addr = BME280_I2C_ADDRESS2,
		.delay_msec = BME280_delay_msek
	};
	esp_err_t err = BME280_I2C_init(&bme280,
                                  CONFIG_MQTT_SENSOR_BME280_SDA_GPIO,
                                  CONFIG_MQTT_SENSOR_BME280_SCL_GPIO);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Cannot init bme280 sensor");
  }
#endif //CONFIG_MQTT_SENSOR_BME280
#ifdef CONFIG_MQTT_SENSOR_DHT22
  gpio_pad_select_gpio(CONFIG_MQTT_SENSOR_DHT22_GPIO);
  gpio_set_direction(CONFIG_MQTT_SENSOR_DHT22_GPIO, GPIO_MODE_OUTPUT_OD);
  gpio_set_level(CONFIG_MQTT_SENSOR_DHT22_GPIO, 1);
#endif //CONFIG_MQTT_SENSOR_DHT22

  while (1)
    {
#ifdef CONFIG_MQTT_SENSOR_DHT22
      dht22_temperature = SHRT_MIN;
      dht22_humidity = SHRT_MIN;
      if (dht_read_data(DHT_TYPE_AM2301, CONFIG_MQTT_SENSOR_DHT22_GPIO, &dht22_humidity, &dht22_temperature) == ESP_OK)
        {
          ESP_LOGI(TAG, "Humidity: %d.%d%% Temp: %d.%dC",
                   dht22_humidity/10, abs(dht22_humidity%10) ,
                   dht22_temperature/10, abs(dht22_temperature%10));
        }
      else
        {
          ESP_LOGE(TAG, "Could not read data from DHT sensor");
        }
#endif //CONFIG_MQTT_SENSOR_DHT22

#ifdef CONFIG_MQTT_SENSOR_DS18X20
      wtemperature=SHRT_MIN;
      ctemperature=SHRT_MIN;

      int sensor_count = ds18x20_scan_devices(SENSOR_GPIO, addrs, MAX_SENSORS);
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

#ifdef CONFIG_MQTT_THERMOSTAT
            if (strcmp(addr, CONFIG_MQTT_THERMOSTAT_DS18X20_SENSOR_ADDRESS) == 0) {
              wtemperature = temp_c;
            }
            else {
              ctemperature = temp_c;
            }
#else
            ctemperature = temp_c;
#endif // CONFIG_MQTT_THERMOSTAT
          }

      }
#endif // CONFIG_MQTT_SENSOR_DS18X20


#ifdef CONFIG_MQTT_SENSOR_BME280
      if (bme_read_data(&bme280_temperature, &bme280_pressure, &bme280_humidity) == ESP_OK)
        {
          ESP_LOGI(TAG, "Temp: %d.%02dC, Pressure: %d, Humidity: %d.%03d%%, ",  bme280_temperature/100,bme280_temperature%100, bme280_pressure, bme280_humidity/1000, bme280_humidity%1000);
        }
      else
        {
          ESP_LOGE(TAG, "Could not read data from BME sensor\n");
        }
#endif //CONFIG_MQTT_SENSOR_BME280

#ifdef CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
      struct ThermostatSensorsMessage t = {wtemperature, ctemperature};
      struct ThermostatMessage tm;
      memset(&tm, 0, sizeof(struct ThermostatMessage));
      tm.msgType = THERMOSTAT_SENSORS_MSG;
      tm.data.sensorsData = t;
      if (xQueueSend( thermostatQueue
                      ,( void * )&tm
                      ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
        ESP_LOGE(TAG, "Cannot send to thermostatQueue");
      }
#endif // CONFIG_MQTT_THERMOSTAT_HEATING_OPTIMIZER
      publish_sensors_data();
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void publish_sensors_data()
{
  const char * topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/sensors";
  ESP_LOGI(TAG, "starting mqtt_publish_sensor_data");

  char data[256];
  memset(data,0,256);
  char tstr[64];
  strcat(data, "{");

#ifdef CONFIG_MQTT_SENSOR_DHT22
  sprintf(tstr, "\"humidity\":%d.%d,\"temperature\":%d.%d,",
          dht22_humidity / 10, abs(dht22_humidity % 10),
          dht22_temperature / 10, abs(dht22_temperature % 10)
          );
  strcat(data, tstr);
#endif //CONFIG_MQTT_SENSOR_DHT22

#ifdef CONFIG_MQTT_SENSOR_DS18X20
  sprintf(tstr, "\"wtemperature\":%d.%d,\"ctemperature\":%d.%d,",
          wtemperature / 10, abs(wtemperature % 10),
          ctemperature / 10, abs(ctemperature % 10));
  strcat(data, tstr);
#endif // CONFIG_MQTT_SENSOR_DS18X20

#ifdef CONFIG_MQTT_SENSOR_BME280
  ESP_LOGI(TAG, "Temp: %d.%02dC, Pressure: %d, Humidity: %d.%03d%%, ", bme280_temperature/100,bme280_temperature%100, bme280_pressure, bme280_humidity/1000, bme280_humidity%1000);
  sprintf(tstr, "\"humidity\":%d.%d,\"temperature\":%d.%d,\"pressure\":%d,",
          bme280_humidity/1000, abs(bme280_humidity%1000),
          bme280_temperature/100, abs(bme280_temperature%100),
          (int)(bme280_pressure*0.750061683)
          );
  strcat(data, tstr);
#endif //CONFIG_MQTT_SENSOR_BME280
  data[strlen(data)-1] = 0;
  strcat(data, "}");

  mqtt_publish_data(topic, data, QOS_0, NO_RETAIN);

}

#endif // CONFIG_MQTT_SENSOR
