#include "esp_system.h"
#ifdef CONFIG_BH1750_SENSOR

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"

#include "sensor_common.h"
#include "sensor_bh1750.h"
#include "app_main.h"
#include "app_publish_data.h"
#include "app_i2c.h"
#include "app_bh1750.h"

static const char *TAG = "SENSOR_BH1750";

static sensor_error_state_t bh1750_error_state = {
    .health = SENSOR_HEALTH_UNKNOWN, 
    .last_published_health = SENSOR_HEALTH_UNKNOWN
};

static const sensor_value_range_t bh1750_range = {0, 32767};  // 0 to 32767 lux (short max)

uint16_t illuminance;

static void publish_bh1750_data(void)
{
    const char *topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/illuminance/bh1750";

    char data[16];
    memset(data, 0, 16);
    sprintf(data, "%d.%02d", illuminance / 100, abs(illuminance % 100));
    publish_non_persistent_data(topic, data);

#ifdef CONFIG_PRESENCE_AUTOMATION_SUPPORT
    extern EventGroupHandle_t presenceEventGroup;
    extern const int PRESENCE_NEW_DATA_BIT;
    if (presenceEventGroup) {
        xEventGroupSetBits(presenceEventGroup, PRESENCE_NEW_DATA_BIT);
    }
#endif
}

void bh1750_sensor_init(void)
{
    // BH1750 is initialized via I2C
}

void bh1750_read_and_publish(void)
{
    esp_err_t ret = i2c_master_BH1750_read(&illuminance);
    if (ret == ESP_ERR_TIMEOUT) {
        sensor_record_error(&bh1750_error_state);
        ESP_LOGE(TAG, "I2C Timeout");
    } else if (ret == ESP_OK) {
        if (sensor_validate_value((short)illuminance, &bh1750_range)) {
            sensor_record_success(&bh1750_error_state);
            ESP_LOGI(TAG, "data: %d.%02d", illuminance/100, illuminance%100);
            publish_bh1750_data();
        } else {
            sensor_record_error(&bh1750_error_state);
            ESP_LOGW(TAG, "Value out of range, skipping");
        }
    } else {
        sensor_record_error(&bh1750_error_state);
        ESP_LOGW(TAG, "%s: No ack, sensor not connected...skip...", esp_err_to_name(ret));
    }
    
    sensor_publish_health_if_changed(&bh1750_error_state, "bh1750");
}

#endif // CONFIG_BH1750_SENSOR
