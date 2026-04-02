#ifndef SENSOR_COMMON_H
#define SENSOR_COMMON_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef enum {
    SENSOR_HEALTH_UNKNOWN,
    SENSOR_HEALTH_ONLINE,
    SENSOR_HEALTH_DEGRADED,
    SENSOR_HEALTH_OFFLINE
} sensor_health_t;

typedef struct {
    int consecutive_errors;
    TickType_t last_error_log;
    sensor_health_t health;
    sensor_health_t last_published_health;
} sensor_error_state_t;

typedef struct {
    short min;
    short max;
} sensor_value_range_t;

bool sensor_should_publish_error(sensor_error_state_t *state);
void sensor_record_error(sensor_error_state_t *state);
void sensor_record_success(sensor_error_state_t *state);
void sensor_record_error_logged(sensor_error_state_t *state);
const char* sensor_health_to_string(sensor_health_t health);
void sensor_publish_health(const char *sensor_name, sensor_health_t health);
void sensor_update_health(sensor_error_state_t *state);
void sensor_publish_health_if_changed(sensor_error_state_t *state, const char *sensor_name);
bool sensor_validate_value(short value, const sensor_value_range_t *range);

void publish_data_to_thermostat(const char *topic, int value);
void publish_sensor_data(const char *topic, int value);

#endif // SENSOR_COMMON_H
