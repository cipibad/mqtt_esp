#include <string.h>
#include "esp_log.h"

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "dht.h"
#include "ds18b20.h"

#include "app_esp8266.h"

#include "bme280.h"
#include "app_bme280.h"
#include "app_sensors.h"
#include "app_thermostat.h"


extern EventGroupHandle_t mqtt_event_group;
extern const int INIT_FINISHED_BIT;

extern int32_t wtemperature;
extern int32_t ctemperature;
extern int16_t pressure;

int16_t temperature;
int16_t humidity;

static const char *TAG = "app_sensors";

void sensors_read(void* pvParameters)
{
  MQTTClient* pclient = (MQTTClient*) pvParameters;

  const dht_sensor_type_t sensor_type = DHT_TYPE_DHT22;
  const int dht_gpio = 5; //D1
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
      ESP_LOGI(TAG, "start sensor reading loop");
      if (dht_read_data(sensor_type, dht_gpio, &humidity, &temperature) == ESP_OK)
        {
          ESP_LOGI(TAG, "Humidity: %d.%d%% Temp: %d.%dC", humidity/10, humidity%10 , temperature/10,temperature%10);
        }
      else
        {
          ESP_LOGE(TAG, "Could not read data from DHT sensor\n");
        }
      ESP_LOGI(TAG, "finished sensor reading");
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

      ESP_LOGI(TAG, "checking thermostat update");
      update_thermostat(pclient);
      ESP_LOGI(TAG, "publishing sensor data");
      publish_sensor_data(pclient);
      ESP_LOGI(TAG, "loop end, waiting 60 seconds");
      vTaskDelay(60000 / portTICK_PERIOD_MS);
      //vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void publish_sensor_data(MQTTClient* pclient)
{
    if (xEventGroupGetBits(mqtt_event_group) & INIT_FINISHED_BIT)
    {
      const char * sensors_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/sensors";
      ESP_LOGI(TAG, "starting mqtt_publish_sensor_data");

      char data[256];
      memset(data,0,256);
      sprintf(data, "{\"counter\":%d, \"humidity\":%d.%d, \"temperature\":%d.%d, \"wtemperature\":%d.%d, \"ctemperature\":%d.%d}",0,

              humidity / 10, humidity % 10,
              temperature / 10, temperature % 10,
              wtemperature / 10, wtemperature % 10,
              ctemperature / 10, ctemperature % 10);

      MQTTMessage message;
      message.qos = QOS1;
      message.retained = 1;
      message.payload = data;
      message.payloadlen = strlen(data);


      int rc = MQTTPublish(pclient, sensors_topic, &message);
      if (rc == 0) {
        ESP_LOGI(TAG, "sent publish sensors successful, rc=%d", rc);
      } else {
        ESP_LOGI(TAG, "failed to publish relay, rc=%d", rc);
      }

    }
}
