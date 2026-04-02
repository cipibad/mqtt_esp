#include "esp_system.h"
#ifdef CONFIG_DS18X20_SENSOR

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "driver/gpio.h"

#include "sensor_common.h"
#include "sensor_ds18x20.h"
#include "app_main.h"
#include "app_publish_data.h"

static const char *TAG = "SENSOR_DS18X20";

static const gpio_num_t SENSOR_GPIO = CONFIG_DS18X20_SENSOR_GPIO;
static bool ds18x20_addresses_cached = false;
static sensor_error_state_t ds18x20_error_state = {
    .health = SENSOR_HEALTH_UNKNOWN, 
    .last_published_health = SENSOR_HEALTH_UNKNOWN
};

static const sensor_value_range_t ds18x20_temp_range = {-550, 1250}; // -55.0°C to 125.0°C

ds18x20_addr_t ds18x20_addrs[DS18X20_MAX_SENSORS];
float ds18x20_temps[DS18X20_MAX_SENSORS];
int ds18x20_sensor_count = 0;

static bool ds18x20_ensure_scanned(void) {
    if (ds18x20_addresses_cached && ds18x20_sensor_count > 0) {
        return true;
    }
    
    ds18x20_sensor_count = ds18x20_scan_devices(SENSOR_GPIO, ds18x20_addrs, DS18X20_MAX_SENSORS);
    if (ds18x20_sensor_count > 0) {
        ds18x20_addresses_cached = true;
        ESP_LOGI(TAG, "Cached %d sensor address(es)", ds18x20_sensor_count);
        return true;
    }
    
    return false;
}

static void ds18x20_invalidate_cache(void) {
    ds18x20_addresses_cached = false;
    ESP_LOGI(TAG, "Address cache invalidated");
}

static void ds18x20_publish_sensor_health(sensor_health_t health) {
    for (int i = 0; i < ds18x20_sensor_count; i++) {
        char topic[64];
        sprintf(topic, CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/status/%08x%08x",
                (uint32_t)(ds18x20_addrs[i] >> 32), (uint32_t)ds18x20_addrs[i]);
        publish_persistent_data(topic, sensor_health_to_string(health));
    }
}

static void publish_ds18x20_temperature(int sensor_id)
{
    const char *temperature_topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/temperature";

    char topic[64];
    memset(topic, 0, 64);
    sprintf(topic, "%s/%08x%08x", temperature_topic,
            (uint32_t)(ds18x20_addrs[sensor_id] >> 32),
            (uint32_t)ds18x20_addrs[sensor_id]);

    publish_sensor_data(topic, ds18x20_temps[sensor_id] * 10);
}

static void publish_ds18x20_data(void)
{
    for (int i = 0; i < ds18x20_sensor_count; i++) {
        publish_ds18x20_temperature(i);
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void ds18x20_sensor_init(void)
{
    // GPIO is configured as open-drain output for 1-Wire
}

void ds18x20_read_and_publish(void)
{
    if (ds18x20_ensure_scanned()) {
        esp_err_t read_result = ds18x20_measure_and_read_multi(SENSOR_GPIO, ds18x20_addrs, ds18x20_sensor_count, ds18x20_temps);
        if (read_result == ESP_OK) {
            bool all_valid = true;
            for (int j = 0; j < ds18x20_sensor_count; j++) {
                char addr[8+8+1];
                sprintf(addr, "%08x", (uint32_t)(ds18x20_addrs[j] >> 32));
                sprintf(addr + 8, "%08x", (uint32_t)ds18x20_addrs[j]);
                short temp_c = (short)(ds18x20_temps[j] * 10);
                if (sensor_validate_value(temp_c, &ds18x20_temp_range)) {
                    ESP_LOGI(TAG, "Sensor %s reports %d.%dC", addr, temp_c/10, abs(temp_c%10));
                } else {
                    all_valid = false;
                }
            }
            if (all_valid) {
                sensor_record_success(&ds18x20_error_state);
                publish_ds18x20_data();
            } else {
                sensor_record_error(&ds18x20_error_state);
            }
        } else {
            sensor_record_error(&ds18x20_error_state);
            ESP_LOGW(TAG, "Read failed, invalidating cache");
            ds18x20_invalidate_cache();
        }
    } else {
        sensor_record_error(&ds18x20_error_state);
        ESP_LOGW(TAG, "No sensors detected!");
    }
    
    if (ds18x20_error_state.health != ds18x20_error_state.last_published_health) {
        ds18x20_publish_sensor_health(ds18x20_error_state.health);
        ds18x20_error_state.last_published_health = ds18x20_error_state.health;
    }
}

#endif // CONFIG_DS18X20_SENSOR
