#include "esp_system.h"
#ifdef CONFIG_SENSOR_SUPPORT

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sensor_common.h"
#include "app_main.h"
#include "app_publish_data.h"

#if CONFIG_MQTT_THERMOSTATS_NB > 0
#include "app_thermostat.h"
#endif

static const char *TAG = "SENSOR_COMMON";

bool sensor_should_publish_error(sensor_error_state_t *state) {
    if (state->consecutive_errors == CONFIG_SENSOR_ERROR_THRESHOLD_FIRST) {
        return true;
    }
    if (state->consecutive_errors % CONFIG_SENSOR_ERROR_THRESHOLD_REPEAT == 0) {
        return true;
    }
    TickType_t now = xTaskGetTickCount();
    if ((now - state->last_error_log) > pdMS_TO_TICKS(CONFIG_SENSOR_ERROR_LOG_INTERVAL * 1000)) {
        return true;
    }
    return false;
}

void sensor_record_error(sensor_error_state_t *state) {
    state->consecutive_errors++;
    sensor_update_health(state);
}

void sensor_record_success(sensor_error_state_t *state) {
    state->consecutive_errors = 0;
    sensor_update_health(state);
}

void sensor_record_error_logged(sensor_error_state_t *state) {
    state->last_error_log = xTaskGetTickCount();
}

const char* sensor_health_to_string(sensor_health_t health) {
    switch (health) {
        case SENSOR_HEALTH_ONLINE: return "online";
        case SENSOR_HEALTH_DEGRADED: return "degraded";
        case SENSOR_HEALTH_OFFLINE: return "offline";
        default: return "unknown";
    }
}

void sensor_publish_health(const char *sensor_name, sensor_health_t health) {
    char topic[64];
    sprintf(topic, CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/status/%s", sensor_name);
    publish_persistent_data(topic, sensor_health_to_string(health));
}

void sensor_update_health(sensor_error_state_t *state) {
    sensor_health_t old_health = state->health;
    
    if (state->consecutive_errors == 0) {
        state->health = SENSOR_HEALTH_ONLINE;
    } else if (state->consecutive_errors < 3) {
        state->health = SENSOR_HEALTH_DEGRADED;
    } else {
        state->health = SENSOR_HEALTH_OFFLINE;
    }
    
    if (state->health != old_health) {
        ESP_LOGI(TAG, "Sensor health changed: %s -> %s", 
                 sensor_health_to_string(old_health), 
                 sensor_health_to_string(state->health));
    }
}

void sensor_publish_health_if_changed(sensor_error_state_t *state, const char *sensor_name) {
    if (state->health != state->last_published_health) {
        sensor_publish_health(sensor_name, state->health);
        state->last_published_health = state->health;
    }
}

bool sensor_validate_value(short value, const sensor_value_range_t *range) {
    if (value < range->min || value > range->max) {
        ESP_LOGW(TAG, "Value %d out of range [%d, %d]", value, range->min, range->max);
        return false;
    }
    return true;
}

void publish_data_to_thermostat(const char *topic, int value)
{
#ifdef CONFIG_MQTT_THERMOSTATS_NB0_SENSOR_TYPE_LOCAL
  if (strncmp(topic, CONFIG_MQTT_THERMOSTATS_NB0_LOCAL_SENSOR_TOPIC, strlen(CONFIG_MQTT_THERMOSTATS_NB0_LOCAL_SENSOR_TOPIC)) == 0) {
    thermostat_publish_local_data(0, value);
  }
#endif

#ifdef CONFIG_MQTT_THERMOSTATS_NB1_SENSOR_TYPE_LOCAL
  if (strncmp(topic, CONFIG_MQTT_THERMOSTATS_NB1_LOCAL_SENSOR_TOPIC, strlen(CONFIG_MQTT_THERMOSTATS_NB1_LOCAL_SENSOR_TOPIC)) == 0) {
    thermostat_publish_local_data(1, value);
  }
#endif

#ifdef CONFIG_MQTT_THERMOSTATS_NB2_SENSOR_TYPE_LOCAL
  if (strncmp(topic, CONFIG_MQTT_THERMOSTATS_NB2_LOCAL_SENSOR_TOPIC, strlen(CONFIG_MQTT_THERMOSTATS_NB2_LOCAL_SENSOR_TOPIC)) == 0) {
    thermostat_publish_local_data(2, value);
  }
#endif

#ifdef CONFIG_MQTT_THERMOSTATS_NB3_SENSOR_TYPE_LOCAL
  if (strncmp(topic, CONFIG_MQTT_THERMOSTATS_NB3_LOCAL_SENSOR_TOPIC, strlen(CONFIG_MQTT_THERMOSTATS_NB3_LOCAL_SENSOR_TOPIC)) == 0) {
    thermostat_publish_local_data(3, value);
  }
#endif

#ifdef CONFIG_MQTT_THERMOSTATS_NB4_SENSOR_TYPE_LOCAL
  if (strncmp(topic, CONFIG_MQTT_THERMOSTATS_NB4_LOCAL_SENSOR_TOPIC, strlen(CONFIG_MQTT_THERMOSTATS_NB4_LOCAL_SENSOR_TOPIC)) == 0) {
    thermostat_publish_local_data(4, value);
  }
#endif

#ifdef CONFIG_MQTT_THERMOSTATS_NB5_SENSOR_TYPE_LOCAL
  if (strncmp(topic, CONFIG_MQTT_THERMOSTATS_NB5_LOCAL_SENSOR_TOPIC, strlen(CONFIG_MQTT_THERMOSTATS_NB5_LOCAL_SENSOR_TOPIC)) == 0) {
    thermostat_publish_local_data(5, value);
  }
#endif
}

void publish_sensor_data(const char *topic, int value)
{
  publish_data_to_thermostat(topic, value);

  char data[16];
  memset(data, 0, 16);
  sprintf(data, "%d.%d", value / 10, abs(value % 10));
  publish_non_persistent_data(topic, data);
}

#endif // CONFIG_SENSOR_SUPPORT
