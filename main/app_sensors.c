#include "esp_system.h"
#ifdef CONFIG_SENSOR_SUPPORT

#include <limits.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

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
#include "app_sensors_dht22.h"
#endif

#ifdef CONFIG_DS18X20_SENSOR
#include "app_sensors_ds18x20.h"
#endif

#ifdef CONFIG_BME280_SENSOR
#include "app_sensors_bme280.h"
#endif

#ifdef CONFIG_BH1750_SENSOR
#include "app_sensors_bh1750.h"
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC
#include "app_sensors_soil_adc.h"
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL
#include "app_sensors_soil_digital.h"
#endif


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
  dht22_publish();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  dht22_publish_ha();
  vTaskDelay(50 / portTICK_PERIOD_MS);
#endif // CONFIG_DHT22_SENSOR_SUPPORT

#ifdef CONFIG_DS18X20_SENSOR
  ds18x20_publish();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  for (int i = 0; i < ds18x20_get_sensor_count(); i++) {
    ds18x20_publish_ha(i);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
#endif // CONFIG_DS18X20_SENSOR

#ifdef CONFIG_BME280_SENSOR
  bme280_publish();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  bme280_publish_ha();
  vTaskDelay(50 / portTICK_PERIOD_MS);
#endif // CONFIG_BME280_SENSOR

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC
  soil_moisture_publish();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  soil_moisture_publish_ha();
  vTaskDelay(50 / portTICK_PERIOD_MS);
#endif // CONFIG_SOIL_MOISTURE_SENSOR_ADC

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL
  soil_moisture_publish();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  soil_moisture_publish_ha();
  vTaskDelay(50 / portTICK_PERIOD_MS);
#endif // CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL

#ifdef CONFIG_BH1750_SENSOR
  bh1750_publish();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  bh1750_publish_ha();
  vTaskDelay(50 / portTICK_PERIOD_MS);
#endif // CONFIG_BH1750_SENSOR

#endif // CONFIG_DEEP_SLEEP_MODE
}

void sensors_read(void* pvParameters)
{

#ifdef CONFIG_BME280_SENSOR
  bme280_init();
#endif

#ifdef CONFIG_DHT22_SENSOR_SUPPORT
  dht22_init();
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC
  soil_moisture_init();
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL
  soil_moisture_init();
#endif

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
      dht22_read();
#endif //CONFIG_DHT22_SENSOR_SUPPORT

#ifdef CONFIG_DS18X20_SENSOR
      ds18x20_read();
#endif // CONFIG_DS18X20_SENSOR

#ifdef CONFIG_BME280_SENSOR
      bme280_read();
#endif //CONFIG_BME280_SENSOR

#ifdef CONFIG_BH1750_SENSOR
      bh1750_read();
#endif // CONFIG_BH1750_SENSOR

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC
      soil_moisture_read();
#endif // CONFIG_SOIL_MOISTURE_SENSOR_ADC

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL
      soil_moisture_read();
#endif // CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL

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
