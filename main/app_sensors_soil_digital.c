#include "esp_system.h"
#ifdef CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL

#include <limits.h>
#include <string.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_main.h"
#include "app_sensors_soil_digital.h"
#include "app_publish_data.h"
#include "app_logging.h"

static short soil_moisture_digital_threshold = SHRT_MIN;

void soil_moisture_digital_digital_digital_init(void)
{
  gpio_set_direction(CONFIG_SOIL_MOISTURE_SENSOR_GPIO, GPIO_MODE_INPUT);
}

void soil_moisture_digital_digital_read(void)
{
  soil_moisture_digital_threshold = 1 - gpio_get_level(CONFIG_SOIL_MOISTURE_SENSOR_GPIO);
  LOGI(LOG_MODULE_SOIL_MOISTURE, "Soil moisture threshold %s",
         soil_moisture_digital_threshold ? "high" : "low");
}

static void publish_soil_moisture_digital_th(void)
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/moisture/th";

  publish_data_to_thermostat(topic, soil_moisture_digital_threshold * 10);

  char data[16];
  memset(data,0,16);
  sprintf(data, "%d", soil_moisture_digital_threshold);
  publish_non_persistent_data(topic, data);
}

static void publish_soil_moisture_digital_th_ha(void)
{
  const char * topic = CONFIG_HOME_NAME "/" CONFIG_ROOM_NAME "/sensor";
  char payload[128];

  sprintf(payload, "{\"source\":\"soil_moisture\",\"moisture\":%d}",
          soil_moisture_digital_threshold);

  publish_non_persistent_data(topic, payload);
}

void soil_moisture_digital_digital_publish(void)
{
  publish_soil_moisture_digital_th();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  publish_soil_moisture_digital_th_ha();
  vTaskDelay(50 / portTICK_PERIOD_MS);
}

void soil_moisture_digital_digital_publish_ha(void)
{
  publish_soil_moisture_digital_th_ha();
  vTaskDelay(50 / portTICK_PERIOD_MS);
}

#endif // CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL
