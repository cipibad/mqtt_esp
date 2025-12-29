#include "esp_system.h"
#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC

#include <limits.h>
#include <string.h>

#include "driver/adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_main.h"
#include "app_sensors_soil_adc.h"
#include "app_publish_data.h"
#include "app_logging.h"

static short soil_moisture = SHRT_MIN;

void soil_moisture_init(void)
{
  adc_config_t adc_config;
  adc_config.mode = ADC_READ_TOUT_MODE;
  adc_config.clk_div = 8;
  ESP_ERROR_CHECK(adc_init(&adc_config));
}

void soil_moisture_read(void)
{
  uint16_t soil_moisture_data = 0;
  if (ESP_OK == adc_read(&soil_moisture_data)) {
    soil_moisture = 1023 - soil_moisture_data;
    LOGI(LOG_MODULE_SOIL_MOISTURE, "adc read: %d", soil_moisture);
  } else {
    LOGE(LOG_MODULE_SOIL_MOISTURE, "Could not read data from adc");
  }
}

static void publish_soil_moisture_adc(void)
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/moisture/adc";

  if (soil_moisture == SHRT_MIN) {
    LOGE(LOG_MODULE_SENSOR, "Invalid sensor value, skipping publish for topic: %s", topic);
    return;
  }

  publish_data_to_thermostat(topic, soil_moisture * 10);

  char data[16];
  memset(data,0,16);
  sprintf(data, "%d.%d", soil_moisture, abs(soil_moisture % 10));
  publish_non_persistent_data(topic, data);
}

static void publish_soil_moisture_adc_ha(void)
{
  const char * topic = CONFIG_HOME_NAME "/" CONFIG_ROOM_NAME "/sensor";
  char payload[128];

  sprintf(payload, "{\"source\":\"soil_moisture\",\"moisture\":%d.%01d}",
          soil_moisture, abs(soil_moisture % 10));

  publish_non_persistent_data(topic, payload);
}

void soil_moisture_publish(void)
{
  publish_soil_moisture_adc();
  vTaskDelay(50 / portTICK_PERIOD_MS);
  publish_soil_moisture_adc_ha();
  vTaskDelay(50 / portTICK_PERIOD_MS);
}

void soil_moisture_publish_ha(void)
{
  publish_soil_moisture_adc_ha();
  vTaskDelay(50 / portTICK_PERIOD_MS);
}

#endif // CONFIG_SOIL_MOISTURE_SENSOR_ADC
