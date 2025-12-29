#include "esp_system.h"
#ifdef CONFIG_DHT22_SENSOR_SUPPORT

#include <limits.h>
#include <string.h>

#include "driver/gpio.h"
#include "rom/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_main.h"
#include "app_sensors_dht22.h"
#include "app_publish_data.h"
#include "app_logging.h"

#include "dht.h"

#ifdef CONFIG_DHT22_SENSOR_TYPE_AM2301
#define DHT_SENSOR_TYPE DHT_TYPE_AM2301
#endif

#ifdef CONFIG_DHT22_SENSOR_TYPE_SI7021
#define DHT_SENSOR_TYPE DHT_TYPE_SI7021
#endif

#define MEAN_FACTOR 5

static short dht22_mean_temperature = SHRT_MIN;
static short dht22_temperature = SHRT_MIN;
static short dht22_mean_humidity = SHRT_MIN;
static short dht22_humidity = SHRT_MIN;

void dht22_init(void)
{
  gpio_pad_select_gpio(CONFIG_DHT22_SENSOR_GPIO);
  gpio_set_direction(CONFIG_DHT22_SENSOR_GPIO, GPIO_MODE_OUTPUT_OD);
  gpio_set_level(CONFIG_DHT22_SENSOR_GPIO, 1);
}

void dht22_read(void)
{
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

      LOGI(LOG_MODULE_DHT22, "Humidity: %d.%d%% Temp: %d.%dC",
                   dht22_mean_humidity/10, abs(dht22_mean_humidity%10) ,
                   dht22_mean_temperature/10, abs(dht22_mean_temperature%10));
    }
  else
    {
      LOGE(LOG_MODULE_DHT22, "Could not read data from DHT sensor");
    }
}

static void publish_dht22_mean_temperature(void)
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/temperature/dht22";
  if (dht22_mean_temperature == SHRT_MIN) {
    LOGE(LOG_MODULE_SENSOR, "Invalid sensor value, skipping publish for topic: %s", topic);
    return;
  }
  
  publish_data_to_thermostat(topic, dht22_mean_temperature);
  
  char data[16];
  memset(data,0,16);
  sprintf(data, "%d.%d", dht22_mean_temperature / 10, abs(dht22_mean_temperature % 10));
  publish_non_persistent_data(topic, data);
}

static void publish_dht22_mean_humidity(void)
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/humidity/dht22";
  if (dht22_mean_humidity == SHRT_MIN) {
    LOGE(LOG_MODULE_SENSOR, "Invalid sensor value, skipping publish for topic: %s", topic);
    return;
  }
  
  publish_data_to_thermostat(topic, dht22_mean_humidity);
  
  char data[16];
  memset(data,0,16);
  sprintf(data, "%d.%d", dht22_mean_humidity / 10, abs(dht22_mean_humidity % 10));
  publish_non_persistent_data(topic, data);
}

void dht22_publish(void)
{
  publish_dht22_mean_temperature();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  publish_dht22_mean_humidity();
  vTaskDelay(50 / portTICK_PERIOD_MS);
}

void dht22_publish_ha(void)
{
  const char * topic = CONFIG_HOME_NAME "/" CONFIG_ROOM_NAME "/sensor";
  char payload[128];
  
  sprintf(payload, "{\"source\":\"dht22\",\"temperature\":%d.%02d,\"humidity\":%d.%02d}",
          dht22_mean_temperature / 10, abs(dht22_mean_temperature % 10),
          dht22_mean_humidity / 10, abs(dht22_mean_humidity % 10));
  
  publish_non_persistent_data(topic, payload);
}

#endif // CONFIG_DHT22_SENSOR_SUPPORT
