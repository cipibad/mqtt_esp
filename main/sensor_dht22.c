#include "esp_system.h"
#ifdef CONFIG_DHT22_SENSOR_SUPPORT

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "rom/gpio.h"

#include "sensor_common.h"
#include "sensor_dht22.h"
#include "app_main.h"
#include "app_publish_data.h"
#include "dht.h"

#ifdef CONFIG_DHT22_SENSOR_TYPE_AM2301
#define DHT_SENSOR_TYPE DHT_TYPE_AM2301
#endif

#ifdef CONFIG_DHT22_SENSOR_TYPE_SI7021
#define DHT_SENSOR_TYPE DHT_TYPE_SI7021
#endif

static const char *TAG = "SENSOR_DHT22";

static sensor_error_state_t dht22_error_state = {
    .health = SENSOR_HEALTH_UNKNOWN, 
    .last_published_health = SENSOR_HEALTH_UNKNOWN
};

static const sensor_value_range_t dht22_temp_range = {-400, 800};    // -40.0°C to 80.0°C
static const sensor_value_range_t dht22_humidity_range = {0, 1000};  // 0% to 100%

short dht22_mean_temperature = SHRT_MIN;
static short dht22_temperature = SHRT_MIN;
short dht22_mean_humidity = SHRT_MIN;
static short dht22_humidity = SHRT_MIN;

void publish_dht22_log(const char* log_message)
{
    const char *topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/log/dht22";
    publish_non_persistent_data(topic, log_message);
}

static void publish_dht22_mean_temperature(void)
{
    const char *topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/temperature/dht22";
    publish_sensor_data(topic, dht22_mean_temperature);
}

static void publish_dht22_mean_humidity(void)
{
    const char *topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/humidity/dht22";
    publish_sensor_data(topic, dht22_mean_humidity);
}

static void publish_dht22_data(void)
{
    publish_dht22_mean_temperature();
    vTaskDelay(50 / portTICK_PERIOD_MS);
    publish_dht22_mean_humidity();
    vTaskDelay(50 / portTICK_PERIOD_MS);
}

void dht22_sensor_init(void)
{
    gpio_pad_select_gpio(CONFIG_DHT22_SENSOR_GPIO);
    gpio_set_direction(CONFIG_DHT22_SENSOR_GPIO, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(CONFIG_DHT22_SENSOR_GPIO, 1);
}

void dht22_read_and_publish(void)
{
    dht22_temperature = SHRT_MIN;
    dht22_humidity = SHRT_MIN;
    
    if (dht_read_data(DHT_SENSOR_TYPE, CONFIG_DHT22_SENSOR_GPIO, &dht22_humidity, &dht22_temperature) == ESP_OK) {
        if (sensor_validate_value(dht22_temperature, &dht22_temp_range) &&
            sensor_validate_value(dht22_humidity, &dht22_humidity_range)) {
            
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

            ESP_LOGI(TAG, "Humidity: %d.%d%% Temp: %d.%dC",
                     dht22_mean_humidity/10, abs(dht22_mean_humidity%10),
                     dht22_mean_temperature/10, abs(dht22_mean_temperature%10));
            sensor_record_success(&dht22_error_state);
            publish_dht22_data();
        } else {
            sensor_record_error(&dht22_error_state);
            ESP_LOGW(TAG, "Values out of range, skipping");
        }
    } else {
        sensor_record_error(&dht22_error_state);
        ESP_LOGE(TAG, "Error: Could not read data from DHT sensor");
        if (sensor_should_publish_error(&dht22_error_state)) {
            publish_dht22_log("Error: Could not read data from DHT sensor");
            sensor_record_error_logged(&dht22_error_state);
        }
    }
    
    sensor_publish_health_if_changed(&dht22_error_state, "dht22");
}

#endif // CONFIG_DHT22_SENSOR_SUPPORT
