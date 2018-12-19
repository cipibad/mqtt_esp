#include "esp_log.h"

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "dht.h"
#include "ds18b20.h"

#include "app_sensors.h"


/* extern EventGroupHandle_t sensors_event_group; */
/* extern const int DHT22; */
/* extern const int DS; */

extern int16_t wtemperature;

extern int16_t temperature;
extern int16_t humidity;

static const char *TAG = "MQTTS_DHT22";

void sensors_read(void* pvParameters)
{
  /* const dht_sensor_type_t sensor_type = DHT_TYPE_DHT22; */
  /* const int dht_gpio = 5; */
  const int DS_PIN = 4;
  ds18b20_init(DS_PIN);

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
  
  while (1)
    {
      //FIXME bug when no sensor
      /* if (dht_read_data(sensor_type, dht_gpio, &humidity, &temperature) == ESP_OK) */
      /*   { */
      /*     /\* xEventGroupSetBits(sensors_event_group, DHT22); *\/ */
      /*     ESP_LOGI(TAG, "Humidity: %d.%d%% Temp: %d.%dC", humidity/10, humidity%10 , temperature/10,temperature%10); */
      /*   } */
      /* else */
      /*   { */
      /*     ESP_LOGE(TAG, "Could not read data from DHT sensor\n"); */
      /*   } */
      //END FIXME

      /* porting */
      /* if (-55. < wtemperature && wtemperature < 125. ) { */
      /*   xEventGroupSetBits(asensors_event_group, DS); */
      /* } */
      if (ds18b20_get_temp(&wtemperature) == ESP_OK)
        {
          /* xEventGroupSetBits(sensors_event_group, DHT22); */
          ESP_LOGI(TAG, "Water temp: %d.%dC", wtemperature/10, wtemperature % 10);
        }
      else
        {
          ESP_LOGE(TAG, "Could not read data from ds18b20 sensor\n");
        }
      vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
