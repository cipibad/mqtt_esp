#include "esp_system.h"
#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC
#include "driver/adc.h"
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "driver/gpio.h"

#include "sensor_common.h"
#include "sensor_soil.h"
#include "app_main.h"
#include "app_publish_data.h"

static const char *TAG = "SENSOR_SOIL";

static sensor_error_state_t soil_adc_error_state = {
    .health = SENSOR_HEALTH_UNKNOWN, 
    .last_published_health = SENSOR_HEALTH_UNKNOWN
};

short soil_moisture = SHRT_MIN;

static void publish_soil_moisture_adc(void)
{
    const char *topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/moisture/adc";
    publish_sensor_data(topic, soil_moisture * 10);
}

void soil_adc_init(void)
{
    adc_config_t adc_config;
    adc_config.mode = ADC_READ_TOUT_MODE;
    adc_config.clk_div = 8;
    ESP_ERROR_CHECK(adc_init(&adc_config));
}

void soil_adc_read_and_publish(void)
{
    uint16_t soil_moisture_data = 0;
    if (ESP_OK == adc_read(&soil_moisture_data)) {
        soil_moisture = 1023 - soil_moisture_data;
        ESP_LOGI(TAG, "adc read: %d", soil_moisture);
        sensor_record_success(&soil_adc_error_state);
        publish_soil_moisture_adc();
    } else {
        sensor_record_error(&soil_adc_error_state);
        ESP_LOGE(TAG, "Could not read data from adc");
    }
    sensor_publish_health_if_changed(&soil_adc_error_state, "adc");
}

#endif // CONFIG_SOIL_MOISTURE_SENSOR_ADC

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL

#include <string.h>
#include "esp_log.h"
#include "driver/gpio.h"

#include "sensor_common.h"
#include "sensor_soil.h"
#include "app_main.h"
#include "app_publish_data.h"

static const char *TAG = "SENSOR_SOIL_DIGITAL";

short soil_moisture_threshold = SHRT_MIN;

static void publish_soil_moisture_th(void)
{
    const char *topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/moisture/th";
    publish_sensor_data(topic, soil_moisture_threshold * 10);
}

void soil_digital_init(void)
{
    gpio_set_direction(CONFIG_SOIL_MOISTURE_SENSOR_GPIO, GPIO_MODE_INPUT);
}

void soil_digital_read_and_publish(void)
{
    soil_moisture_threshold = 1 - gpio_get_level(CONFIG_SOIL_MOISTURE_SENSOR_GPIO);
    ESP_LOGI(TAG, "Soil moisture threshold %s", soil_moisture_threshold ? "high" : "low");
    publish_soil_moisture_th();
}

#endif // CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_SWITCH

#include "driver/gpio.h"
#include "sensor_soil.h"

void soil_switch_init(void)
{
    gpio_set_direction(CONFIG_SOIL_MOISTURE_SENSOR_SWITCH_GPIO, GPIO_MODE_OUTPUT);
}

void soil_switch_on(void)
{
    gpio_set_level(CONFIG_SOIL_MOISTURE_SENSOR_SWITCH_GPIO, 1);
}

void soil_switch_off(void)
{
    gpio_set_level(CONFIG_SOIL_MOISTURE_SENSOR_SWITCH_GPIO, 0);
}

#endif // CONFIG_SOIL_MOISTURE_SENSOR_SWITCH
