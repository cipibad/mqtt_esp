#include <string.h>
#include "esp_log.h"

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef CONFIG_MQTT_SENSOR_DHT22
#include "dht.h"
#endif //CONFIG_MQTT_SENSOR_DHT22

#include "ds18b20.h"

#include "app_esp8266.h"

#include "bme280.h"
#include "app_bme280.h"
#include "app_sensors.h"
/* #include "app_thermostat.h" */


extern EventGroupHandle_t mqtt_event_group;
extern const int INIT_FINISHED_BIT;
extern const int PUBLISHED_BIT;

int32_t wtemperature;
int32_t ctemperature;
int16_t pressure;

#ifdef CONFIG_MQTT_SENSOR_DHT22
int16_t dht22_temperature;
int16_t dht22_humidity;
#endif //CONFIG_MQTT_SENSOR_DHT22

static const char *TAG = "app_sensors";

void sensors_read(void* pvParameters)
{
  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;

  /* const int DS_PIN = 4; */
  /* const int sda_pin = 5; //D1 */
  /* const int scl_pin = 4; //D2 */
  /* const int sda_pin = 4; //D3 */
  /* const int scl_pin = 5; //D4 */

  /* ds18b20_init(DS_PIN); */

  /* gpio_config_t io_conf; */
  /* //disable interrupt */
  /* io_conf.intr_type = GPIO_INTR_DISABLE; */
  /* //set as output mode */
  /* io_conf.mode = GPIO_MODE_OUTPUT; */
  /* //disable pull-down mode */
  /* io_conf.pull_down_en = 0; */
  /* //disable pull-up mode */
  /* io_conf.pull_up_en = 0; */
  
  /* //bit mask of the pins that you want to set,e.g.GPIO15/16 */
  /* io_conf.pin_bit_mask = 0; */
  /* io_conf.pin_bit_mask |= (1ULL << dht_gpio) ; */
  /* //configure GPIO with the given settings */
  /* gpio_config(&io_conf); */

  /* i2c_master_init(SDA_PIN, SCL_PIN); */
  /* bme280_normal_mode_init(); */

	/* struct bme280_t bme280 = { */
	/* 	.bus_write = BME280_I2C_bus_write, */
	/* 	.bus_read = BME280_I2C_bus_read, */
	/* 	.dev_addr = BME280_I2C_ADDRESS2, */
	/* 	.delay_msec = BME280_delay_msek */
	/* }; */
	/* ESP_ERROR_CHECK(BME280_I2C_init(&bme280, sda_pin, scl_pin)); */
  while (1)
    {
      //FIXME bug when no sensor
#ifdef CONFIG_MQTT_SENSOR_DHT22
      if (dht_read_data(DHT_TYPE_DHT22, CONFIG_MQTT_SENSOR_DHT22_GPIO, &dht22_humidity, &dht22_temperature) == ESP_OK)
        {
          ESP_LOGI(TAG, "Humidity: %d.%d%% Temp: %d.%dC",
                   dht22_humidity/10, dht22_humidity%10 ,
                   dht22_temperature/10, dht22_temperature%10);
        }
      else
        {
          ESP_LOGE(TAG, "Could not read data from DHT sensor");
        }
#endif //CONFIG_MQTT_SENSOR_DHT22
      /* END FIXME */

      /* porting */
      /* if (-55. < wtemperature && wtemperature < 125. ) { */
      /*   xEventGroupSetBits(asensors_event_group, DS); */
      /* } */

      /* if (ds18b20_get_temp(&wtemperature) == ESP_OK) */
      /*   { */
      /*     /\* xEventGroupSetBits(sensors_event_group, DHT22); *\/ */
      /*     ESP_LOGI(TAG, "Water temp: %d.%dC", wtemperature/10, wtemperature % 10); */
      /*   } */
      /* else */
      /*   { */
      /*     ESP_LOGE(TAG, "Could not read data from ds18b20 sensor\n"); */
      /*   } */

      /* if (bme_read_data(&temperature, &pressure, &humidity) == ESP_OK) */
      /*   { */
      /*     /\* xEventGroupSetBits(sensors_event_group, DHT22); *\/ */
      /*     ESP_LOGI(TAG, "Temp: %d.%02dC, Pressure: %d, Humidity: %d.%03d%%, ",  temperature/100,temperature%100, pressure, humidity/1000, humidity%1000); */
      /*   } */
      /* else */
      /*   { */
      /*     ESP_LOGE(TAG, "Could not read data from DHT sensor\n"); */
      /*   } */

      /* update_thermostat(pclient); */
      publish_sensors_data(client);
      vTaskDelay(60000 / portTICK_PERIOD_MS);
      //vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void publish_sensors_data(esp_mqtt_client_handle_t client)
{
    if (xEventGroupGetBits(mqtt_event_group) & INIT_FINISHED_BIT)
    {
      const char * sensors_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/sensors";
      ESP_LOGI(TAG, "starting mqtt_publish_sensor_data");

      char data[256];
      memset(data,0,256);
      sprintf(data, "{\"wtemperature\":%d.%d, \"ctemperature\":%d.%d",
              wtemperature / 10, wtemperature % 10,
              ctemperature / 10, ctemperature % 10);

#ifdef CONFIG_MQTT_SENSOR_DHT22
      char tstr[64];
      sprintf(tstr, ", \"humidity\":%d.%d, \"temperature\":%d.%d",
              dht22_humidity / 10, dht22_humidity % 10,
              dht22_temperature / 10, dht22_temperature % 10
              );
        strcat(data, tstr);
#endif //CONFIG_MQTT_SENSOR_DHT22

      strcat(data, "}");

      xEventGroupClearBits(mqtt_event_group, PUBLISHED_BIT);
      int msg_id = esp_mqtt_client_publish(client, sensors_topic, data,strlen(data), 1, 0);
      if (msg_id > 0) {
        ESP_LOGI(TAG, "sent publish temp successful, msg_id=%d", msg_id);
        EventBits_t bits = xEventGroupWaitBits(mqtt_event_group, PUBLISHED_BIT, false, true, MQTT_FLAG_TIMEOUT);
        if (bits & PUBLISHED_BIT) {
          ESP_LOGI(TAG, "publish ack received, msg_id=%d", msg_id);
        } else {
          ESP_LOGW(TAG, "publish ack not received, msg_id=%d", msg_id);
        }
      } else {
        ESP_LOGI(TAG, "failed to publish temp, msg_id=%d", msg_id);
      }
    } else {
      ESP_LOGW(TAG, "skip publish sensor data as mqtt init not finished");

    }
}

