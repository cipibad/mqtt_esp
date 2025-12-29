#include "esp_system.h"
#ifdef CONFIG_SENSOR_SUPPORT

#include <limits.h>
#include <string.h>

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
extern EventGroupHandle_t mqtt_event_group;
extern const int MQTT_CONNECTED_BIT;

#include "esp_sleep.h"
#include "esp_wifi.h"
#include "app_logging.h"


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

#ifdef CONFIG_I2C_SENSOR_SUPPORT
#include "app_i2c.h"

#ifdef CONFIG_BME280_SENSOR
#include "bme280.h"
#include "app_bme280.h"
int32_t bme280_pressure;
int32_t bme280_temperature;
int32_t bme280_humidity;
#endif //CONFIG_BME280_SENSOR

#ifdef CONFIG_BH1750_SENSOR
#include "app_bh1750.h"
uint16_t illuminance;
#endif // CONFIG_BH1750_SENSOR

#endif // CONFIG_I2C_SENSOR_SUPPORT

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC
#include "driver/adc.h"
short soil_moisture = SHRT_MIN;
#endif // CONFIG_SOIL_MOISTURE_SENSOR_ADC

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL
short soil_moisture_threshold = SHRT_MIN;
#endif // CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL


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

#ifdef CONFIG_MQTT_THERMOSTATS_NB4_SENSOR_TYPE_LOCAL
  if (strncmp(topic, CONFIG_MQTT_THERMOSTATS_NB4_LOCAL_SENSOR_TOPIC, strlen(CONFIG_MQTT_THERMOSTATS_NB4_LOCAL_SENSOR_TOPIC)) == 0) {
    thermostat_publish_local_data(4, value);
  }
#endif //CONFIG_MQTT_THERMOSTATS_NB4_SENSOR_TYPE_MQTT

#ifdef CONFIG_MQTT_THERMOSTATS_NB5_SENSOR_TYPE_LOCAL
  if (strncmp(topic, CONFIG_MQTT_THERMOSTATS_NB5_LOCAL_SENSOR_TOPIC, strlen(CONFIG_MQTT_THERMOSTATS_NB5_LOCAL_SENSOR_TOPIC)) == 0) {
    thermostat_publish_local_data(5, value);
  }
#endif //CONFIG_MQTT_THERMOSTATS_NB5_SENSOR_TYPE_MQTT

}

void publish_sensor_data(const char * topic, int value)
{
  if (value == SHRT_MIN) {
    LOGE(LOG_MODULE_SENSOR, "Invalid sensor value, skipping publish for topic: %s", topic);
    return;
  }

  publish_data_to_thermostat(topic, value);

  char data[16];
  memset(data,0,16);
  sprintf(data, "%d.%d", value / 10, abs(value % 10));
  publish_non_persistent_data(topic, data);
}

#ifdef CONFIG_DHT22_SENSOR_SUPPORT
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

  publish_sensor_data(topic, bme280_pressure/10);
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


#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC
void publish_soil_moisture_adc()
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/moisture/adc";
  publish_sensor_data(topic, soil_moisture * 10);
}
#endif // CONFIG_SOIL_MOISTURE_SENSOR_ADC

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL
void publish_soil_moisture_th()
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/moisture/th";
  publish_sensor_data(topic, soil_moisture_threshold * 10);
}
#endif // CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL

#ifdef CONFIG_BH1750_SENSOR
void publish_bh1750_data()
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/illuminance/bh1750";

  if (illuminance == 0) {
    LOGE(LOG_MODULE_BH1750, "Invalid illuminance value (0), skipping publish");
    return;
  }

  char data[16];
  memset(data,0,16);
  sprintf(data, "%d.%02d", illuminance / 100, abs(illuminance % 100));
  publish_non_persistent_data(topic, data);

#ifdef CONFIG_PRESENCE_AUTOMATION_SUPPORT
  extern EventGroupHandle_t presenceEventGroup;
  extern const int PRESENCE_NEW_DATA_BIT;
  if( presenceEventGroup != NULL ) {
    xEventGroupSetBits (presenceEventGroup, PRESENCE_NEW_DATA_BIT);
  }
#endif // CONFIG_PRESENCE_AUTOMATION_SUPPORT
}
#endif // CONFIG_BH1750_SENSOR

#ifdef CONFIG_BME280_SENSOR
void publish_bme280_ha_data()
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

#ifdef CONFIG_DHT22_SENSOR_SUPPORT
void publish_dht22_ha_data()
{
  const char * topic = CONFIG_HOME_NAME "/" CONFIG_ROOM_NAME "/sensor";
  char payload[128];

  sprintf(payload, "{\"source\":\"dht22\",\"temperature\":%d.%02d,\"humidity\":%d.%02d}",
          dht22_mean_temperature / 10, abs(dht22_mean_temperature % 10),
          dht22_mean_humidity / 10, abs(dht22_mean_humidity % 10));

  publish_non_persistent_data(topic, payload);
}
#endif // CONFIG_DHT22_SENSOR_SUPPORT

#ifdef CONFIG_DS18X20_SENSOR
void publish_ds18x20_ha_data(int sensor_id)
{
  const char * topic = CONFIG_HOME_NAME "/" CONFIG_ROOM_NAME "/sensor";
  char payload[128];
  char addr_str[17];

  sprintf(addr_str, "%016llx", (unsigned long long)addrs[sensor_id]);

  sprintf(payload, "{\"source\":\"ds18x20\",\"address\":\"%s\",\"temperature\":%d.%02d}",
          addr_str,
          (int)(temps[sensor_id] * 10), abs((int)(temps[sensor_id] * 10) % 10));

  publish_non_persistent_data(topic, payload);
}
#endif // CONFIG_DS18X20_SENSOR

#ifdef CONFIG_BH1750_SENSOR
void publish_bh1750_ha_data()
{
  const char * topic = CONFIG_HOME_NAME "/" CONFIG_ROOM_NAME "/sensor";
  char payload[128];

  if (illuminance == 0) {
    return;
  }

  sprintf(payload, "{\"source\":\"bh1750\",\"illuminance\":%d.%02d}",
          illuminance / 100, abs(illuminance % 100));

  publish_non_persistent_data(topic, payload);
}
#endif // CONFIG_BH1750_SENSOR

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC
void publish_soil_moisture_adc_ha_data()
{
  const char * topic = CONFIG_HOME_NAME "/" CONFIG_ROOM_NAME "/sensor";
  char payload[128];

  sprintf(payload, "{\"source\":\"soil_moisture\",\"moisture\":%d.%01d}",
          soil_moisture, abs(soil_moisture % 10));

  publish_non_persistent_data(topic, payload);
}
#endif // CONFIG_SOIL_MOISTURE_SENSOR_ADC

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL
void publish_soil_moisture_th_ha_data()
{
  const char * topic = CONFIG_HOME_NAME "/" CONFIG_ROOM_NAME "/sensor";
  char payload[128];

  sprintf(payload, "{\"source\":\"soil_moisture\",\"moisture\":%d}",
          soil_moisture_threshold);

  publish_non_persistent_data(topic, payload);
}
#endif // CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL

void publish_ha_autoconfig()
{
  char config_topic[128];
  char payload[512];
  char sensors[256];
  int offset = 0;

  memset(sensors, 0, sizeof(sensors));
  memset(payload, 0, sizeof(payload));

  sensors[0] = '\0';
  offset = sprintf(sensors, "[");

#ifdef CONFIG_BME280_SENSOR
  offset += sprintf(sensors + offset, "\"bme280\",");
#endif

#ifdef CONFIG_DHT22_SENSOR_SUPPORT
  offset += sprintf(sensors + offset, "\"dht22\",");
#endif

#ifdef CONFIG_DS18X20_SENSOR
  offset += sprintf(sensors + offset, "\"ds18x20\",");
#endif

#ifdef CONFIG_BH1750_SENSOR
  offset += sprintf(sensors + offset, "\"bh1750\",");
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC
  offset += sprintf(sensors + offset, "\"soil_moisture_adc\",");
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL
  offset += sprintf(sensors + offset, "\"soil_moisture_digital\",");
#endif

  if (offset > 1) {
    sensors[offset - 1] = ']';
    sensors[offset] = '\0';
  } else {
    strcpy(sensors, "[]");
  }

  sprintf(config_topic, "device/%s/evt/config", CONFIG_CLIENT_ID);
  sprintf(payload,
          "{\"device_type\":\"%s\",\"client_id\":\"%s\",\"home_name\":\"%s\",\"room_name\":\"%s\",\"topic\":\"%s/%s/sensor\",\"sensors\":%s}",
          CONFIG_DEVICE_TYPE,
          CONFIG_CLIENT_ID,
          CONFIG_HOME_NAME,
          CONFIG_ROOM_NAME,
          CONFIG_HOME_NAME,
          CONFIG_ROOM_NAME,
          sensors);

  publish_non_persistent_data(config_topic, payload);
}

void publish_sensors_data()
{
  publish_ha_autoconfig();

#ifndef CONFIG_DEEP_SLEEP_MODE

#ifdef CONFIG_DHT22_SENSOR_SUPPORT
  publish_dht22_data();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  publish_dht22_ha_data();
  vTaskDelay(50 / portTICK_PERIOD_MS);
#endif // CONFIG_DHT22_SENSOR_SUPPORT

#ifdef CONFIG_DS18X20_SENSOR
  publish_ds18x20_data();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  for(int i=0; i<sensor_count; i++) {
    publish_ds18x20_ha_data(i);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
#endif // CONFIG_DS18X20_SENSOR

#ifdef CONFIG_BME280_SENSOR
  publish_bme280_data();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  publish_bme280_ha_data();
  vTaskDelay(50 / portTICK_PERIOD_MS);
#endif // CONFIG_BME280_SENSOR

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC
  publish_soil_moisture_adc();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  publish_soil_moisture_adc_ha_data();
  vTaskDelay(50 / portTICK_PERIOD_MS);
#endif // CONFIG_SOIL_MOISTURE_SENSOR_ADC

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL
  publish_soil_moisture_th();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  publish_soil_moisture_th_ha_data();
  vTaskDelay(50 / portTICK_PERIOD_MS);
#endif // CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL

#ifdef CONFIG_BH1750_SENSOR
    publish_bh1750_data();
    vTaskDelay(50 / portTICK_PERIOD_MS);
    publish_bh1750_ha_data();
    vTaskDelay(50 / portTICK_PERIOD_MS);
#endif // CONFIG_BH1750_SENSOR

#endif // CONFIG_DEEP_SLEEP_MODE
}

void sensors_read(void* pvParameters)
{

#ifdef CONFIG_I2C_SENSOR_SUPPORT
ESP_ERROR_CHECK(i2c_master_init(CONFIG_I2C_SENSOR_SDA_GPIO, CONFIG_I2C_SENSOR_SCL_GPIO));

#ifdef CONFIG_BME280_SENSOR
  //Don't forget to connect SDO to Vio too

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
#endif //CONFIG_BME280_SENSOR

#endif // CONFIG_I2C_SENSOR_SUPPORT

#ifdef CONFIG_DHT22_SENSOR_SUPPORT
  gpio_pad_select_gpio(CONFIG_DHT22_SENSOR_GPIO);
  gpio_set_direction(CONFIG_DHT22_SENSOR_GPIO, GPIO_MODE_OUTPUT_OD);
  gpio_set_level(CONFIG_DHT22_SENSOR_GPIO, 1);
#endif //CONFIG_DHT22_SENSOR_SUPPORT

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_SWITCH
  gpio_set_direction(CONFIG_SOIL_MOISTURE_SENSOR_SWITCH_GPIO, GPIO_MODE_OUTPUT);
#endif // CONFIG_SOIL_MOISTURE_SENSOR_SWITCH

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC
  // 1. init adc
  adc_config_t adc_config;

  // Depend on menuconfig->Component config->PHY->vdd33_const value
  // When measuring system voltage(ADC_READ_VDD_MODE), vdd33_const must be set to 255.
  adc_config.mode = ADC_READ_TOUT_MODE;
  adc_config.clk_div = 8; // ADC sample collection clock = 80MHz/clk_div = 10MHz
  ESP_ERROR_CHECK(adc_init(&adc_config));
#endif // CONFIG_SOIL_MOISTURE_SENSOR_ADC
#ifdef CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL
  gpio_set_direction(CONFIG_SOIL_MOISTURE_SENSOR_GPIO, GPIO_MODE_INPUT);
#endif // CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL

  while (1)
    {
#ifdef CONFIG_DEEP_SLEEP_MODE
#ifdef CONFIG_NORTH_INTERFACE_MQTT
  EventBits_t bits = xEventGroupGetBits(mqtt_event_group);
  while ( !(bits & MQTT_CONNECTED_BIT) ) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    bits = xEventGroupGetBits(mqtt_event_group);
  }
#endif // CONFIG_NORTH_INTERFACE_MQTT
#endif // CONFIG_DEEP_SLEEP_MODE

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

          LOGI(LOG_MODULE_DHT22, "Humidity: %d.%d%% Temp: %d.%dC",
                   dht22_mean_humidity/10, abs(dht22_mean_humidity%10) ,
                   dht22_mean_temperature/10, abs(dht22_mean_temperature%10));
          publish_dht22_data();
        }
      else
        {
          LOGE(LOG_MODULE_DHT22, "Could not read data from DHT sensor");
        }
#endif //CONFIG_DHT22_SENSOR_SUPPORT

#ifdef CONFIG_DS18X20_SENSOR
       sensor_count = ds18x20_scan_devices(SENSOR_GPIO, addrs, MAX_SENSORS);
      if (sensor_count < 1) {
        LOGW(LOG_MODULE_DS18X20, "No sensors detected");
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
            LOGI(LOG_MODULE_DS18X20, "Sensor %s reports %d.%dC", addr, temp_c/10, abs(temp_c%10));
          }
        publish_ds18x20_data();

      }
#endif // CONFIG_DS18X20_SENSOR

#ifdef CONFIG_BME280_SENSOR
      if (bme_read_data(&bme280_temperature, &bme280_pressure, &bme280_humidity) == ESP_OK)
        {
          LOGI(LOG_MODULE_BME280, "Temp: %d.%02d degC, Pressure: %d.%02d mmHg , Humidity: %d.%03d %%rH, ",
            bme280_temperature / 100, bme280_temperature % 100,
            bme280_pressure / 100, bme280_pressure % 100,
            bme280_humidity / 1000, bme280_humidity % 1000);
          publish_bme280_data();
        }
       else
        {
          LOGE(LOG_MODULE_BME280, "Could not read data from BME sensor");
        }
#endif //CONFIG_BME280_SENSOR

#ifdef CONFIG_BH1750_SENSOR
       esp_err_t ret = i2c_master_BH1750_read(&illuminance);
       if (ret == ESP_ERR_TIMEOUT) {
           LOGE(LOG_MODULE_BH1750, "I2C Timeout");
       } else if (ret == ESP_OK) {
           LOGI(LOG_MODULE_BH1750, "Illuminance: %d.%02d lx", illuminance/100, illuminance%100);
           publish_bh1750_data();
       } else {
           LOGW(LOG_MODULE_BH1750, "%s: No ack, sensor not connected...skip...", esp_err_to_name(ret));
   }
#endif // CONFIG_BH1750_SENSOR

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_SWITCH
  gpio_set_level(CONFIG_SOIL_MOISTURE_SENSOR_SWITCH_GPIO, 1);
#endif // CONFIG_SOIL_MOISTURE_SENSOR_SWITCH

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC
    uint16_t soil_moisture_data = 0;
    if (ESP_OK == adc_read(&soil_moisture_data)) {
        soil_moisture = 1023 - soil_moisture_data;
        LOGI(LOG_MODULE_SOIL_MOISTURE, "adc read: %d", soil_moisture);
        publish_soil_moisture_adc();
    } else {
      LOGE(LOG_MODULE_SOIL_MOISTURE, "Could not read data from adc");
    }
#endif // CONFIG_SOIL_MOISTURE_SENSOR_ADC

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL
     soil_moisture_threshold = 1 - gpio_get_level(CONFIG_SOIL_MOISTURE_SENSOR_GPIO);
    LOGI(LOG_MODULE_SOIL_MOISTURE, "Soil moisture threshold %s", soil_moisture_threshold ? "high" : "low");
    publish_soil_moisture_th();
#endif // CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_SWITCH
  gpio_set_level(CONFIG_SOIL_MOISTURE_SENSOR_SWITCH_GPIO, 0);
#endif // CONFIG_SOIL_MOISTURE_SENSOR_SWITCH

#ifdef CONFIG_DEEP_SLEEP_MODE
#ifdef CONFIG_NORTH_INTERFACE_MQTT
  bits = xEventGroupGetBits(mqtt_event_group);
  if (bits & MQTT_CONNECTED_BIT) {
    LOGI(LOG_MODULE_SYSTEM, "Going deepsleep");
    esp_deep_sleep_set_rf_option(2);
    esp_wifi_stop();
    esp_deep_sleep(CONFIG_DEEP_SLEEP_MODE_PERIOD * 1000 * 1000);
  } else {
    continue;
  }
#endif // CONFIG_NORTH_INTERFACE_MQTT
#endif // CONFIG_DEEP_SLEEP_MODE

      vTaskDelay(CONFIG_SENSOR_READING_INTERVAL * 1000 / portTICK_PERIOD_MS);
    }
}

#endif // CONFIG_SENSOR_SUPPORT
