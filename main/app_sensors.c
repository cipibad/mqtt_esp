#include "esp_system.h"
#ifdef CONFIG_SENSOR_SUPPORT

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
#include "sensor_common.h"

#include "app_publish_data.h"
extern EventGroupHandle_t mqtt_event_group;
extern const int MQTT_CONNECTED_BIT;

#include "esp_sleep.h"
#include "esp_wifi.h"

#include "sensor_dht22.h"
#include "sensor_ds18x20.h"
#include "sensor_bme280.h"
#include "sensor_bh1750.h"
#include "sensor_soil.h"

#ifdef CONFIG_I2C_SENSOR_SUPPORT
#include "app_i2c.h"
#endif

static const char *TAG = "APP_SENSOR";

void publish_sensors_data(void)
{
#ifndef CONFIG_DEEP_SLEEP_MODE

#ifdef CONFIG_DHT22_SENSOR_SUPPORT
    dht22_read_and_publish();
    vTaskDelay(50 / portTICK_PERIOD_MS);
#endif

#ifdef CONFIG_DS18X20_SENSOR
    ds18x20_read_and_publish();
    vTaskDelay(50 / portTICK_PERIOD_MS);
#endif

#ifdef CONFIG_BME280_SENSOR
    bme280_read_and_publish();
    vTaskDelay(50 / portTICK_PERIOD_MS);
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC
    soil_adc_read_and_publish();
    vTaskDelay(50 / portTICK_PERIOD_MS);
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL
    soil_digital_read_and_publish();
    vTaskDelay(50 / portTICK_PERIOD_MS);
#endif

#ifdef CONFIG_BH1750_SENSOR
    bh1750_read_and_publish();
    vTaskDelay(50 / portTICK_PERIOD_MS);
#endif

#endif // CONFIG_DEEP_SLEEP_MODE
}

void sensors_read(void* pvParameters)
{
#ifdef CONFIG_I2C_SENSOR_SUPPORT
    ESP_ERROR_CHECK(i2c_master_init(CONFIG_I2C_SENSOR_SDA_GPIO, CONFIG_I2C_SENSOR_SCL_GPIO));
#endif

#ifdef CONFIG_BME280_SENSOR
    bme280_sensor_init();
#endif

#ifdef CONFIG_DHT22_SENSOR_SUPPORT
    dht22_sensor_init();
#endif

#ifdef CONFIG_DS18X20_SENSOR
    ds18x20_sensor_init();
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_SWITCH
    soil_switch_init();
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC
    soil_adc_init();
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL
    soil_digital_init();
#endif

    while (1) {
#ifdef CONFIG_DEEP_SLEEP_MODE
#ifdef CONFIG_NORTH_INTERFACE_MQTT
        EventBits_t bits = xEventGroupGetBits(mqtt_event_group);
        while (!(bits & MQTT_CONNECTED_BIT)) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            bits = xEventGroupGetBits(mqtt_event_group);
        }
#endif // CONFIG_NORTH_INTERFACE_MQTT
#endif // CONFIG_DEEP_SLEEP_MODE

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_SWITCH
        soil_switch_on();
#endif

#ifdef CONFIG_DHT22_SENSOR_SUPPORT
        dht22_read_and_publish();
#endif

#ifdef CONFIG_DS18X20_SENSOR
        ds18x20_read_and_publish();
#endif

#ifdef CONFIG_BME280_SENSOR
        bme280_read_and_publish();
#endif

#ifdef CONFIG_BH1750_SENSOR
        bh1750_read_and_publish();
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_ADC
        soil_adc_read_and_publish();
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_DIGITAL
        soil_digital_read_and_publish();
#endif

#ifdef CONFIG_SOIL_MOISTURE_SENSOR_SWITCH
        soil_switch_off();
#endif

#ifdef CONFIG_DEEP_SLEEP_MODE
#ifdef CONFIG_NORTH_INTERFACE_MQTT
        bits = xEventGroupGetBits(mqtt_event_group);
        if (bits & MQTT_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Going deepsleep");
            esp_deep_sleep_set_rf_option(2);
            esp_wifi_stop();
            esp_deep_sleep(CONFIG_DEEP_SLEEP_MODE_PERIOD * 1000 * 1000);
        } else {
            continue;
        }
#endif // CONFIG_NORTH_INTERFACE_MQTT
#else
        vTaskDelay(CONFIG_SENSOR_READING_INTERVAL * 1000 / portTICK_PERIOD_MS);
#endif // CONFIG_DEEP_SLEEP_MODE
    }
}

#endif // CONFIG_SENSOR_SUPPORT
