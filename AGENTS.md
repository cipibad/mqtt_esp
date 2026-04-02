# AGENTS.md

This document provides guidance for AI coding agents working in this repository.

## Project Overview

ESP8266/ESP32 IoT firmware for home automation using MQTT. Built on ESP-IDF framework. Supports relays, thermostats, sensors, valves, schedulers, and more.

## Build Commands

### Firmware Build (requires ESP-IDF)
```bash
# Build firmware
make -j4

# Flash to device
make flash

# Monitor serial output
make monitor

# Full build, flash, and monitor
make -j4 flash monitor
```

### Host-based Unit Tests
```bash
# Run all tests
make -C test-host test

# Run tests with coverage
make -C test-host coverage_report

# Clean test artifacts
make -C test-host clean
```

### Running a Single Test
```bash
# Run specific test by tag
./test-host/test_mqtt_esp "[tag]"

# Run specific test by name (wildcard supported)
./test-host/test_mqtt_esp "publish_thermostat_current_temperature_evt_valid"

# Run tests matching a pattern
./test-host/test_mqtt_esp "[thermostat]"

# Long-running tests
make -C test-host long-test
```

## Code Style Guidelines

### File Naming
- Source files: `app_<module>.c` (e.g., `app_thermostat.c`, `app_relay.c`)
- Header files: `app_<module>.h`
- Test files: `test_app_<module>.cc` (C++ with Catch2)
- Components: lowercase (e.g., `ds18x20/`, `bme280/`)

### Include Order
```c
// 1. System headers
#include <string.h>
#include <stdlib.h>
#include <limits.h>

// 2. ESP-IDF headers
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

// 3. FreeRTOS headers
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// 4. Driver headers
#include "driver/gpio.h"

// 5. Project headers (app_main.h first, then others)
#include "app_main.h"
#include "app_thermostat.h"
#include "app_nvs.h"
```

### Include Guards
Use `#ifndef/#define/#endif` pattern with `MODULE_NAME_H` format:
```c
#ifndef APP_THERMOSTAT_H
#define APP_THERMOSTAT_H

// content

#endif /* APP_THERMOSTAT_H */
```

### Naming Conventions

#### Functions
- snake_case: `handle_thermostat_cmd_task()`, `publish_relay_status()`
- Prefix with module: `app_`, `update_`, `handle_`, `publish_`, `read_`, `write_`

#### Variables
- snake_case: `thermostat_mode`, `relay_status`, `current_temperature`
- Arrays indexed: `relayStatus[id]`, `targetTemperature[id]`

#### Constants/Macros
- UPPER_SNAKE_CASE: `THERMOSTAT_TIMEOUT`, `MAX_TOPIC_LEN`, `QUEUE_TIMEOUT`
- Config values: `CONFIG_*` (from Kconfig)

#### Types (structs, enums)
- CamelCase for type names: `ThermostatMessage`, `RelayMessage`
- UPPER_SNAKE_CASE with prefix for enum values: `THERMOSTAT_MODE_HEAT`, `RELAY_STATUS_ON`

#### Static TAG for logging
```c
static const char *TAG = "MQTTS_THERMOSTAT";  // or "MQTTS_RELAY", etc.
```

### Logging

Use ESP-IDF logging macros:
```c
ESP_LOGI(TAG, "Information message: %d", value);
ESP_LOGW(TAG, "Warning: something unexpected");
ESP_LOGE(TAG, "Error: something failed");
```

### Error Handling

Use `esp_err_t` return type and `ESP_ERROR_CHECK` macro:
```c
esp_err_t err = nvs_flash_init();
if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
}
ESP_ERROR_CHECK(err);
```

Queue operations should check return value:
```c
if (xQueueSend(queue, &msg, timeout) != pdPASS) {
    ESP_LOGE(TAG, "Cannot send to queue");
}
```

### FreeRTOS Patterns

#### Task Functions
```c
void handle_thermostat_cmd_task(void* pvParameters)
{
    struct ThermostatMessage t;
    while(1) {
        if (xQueueReceive(thermostatQueue, &t, portMAX_DELAY)) {
            // handle message
        }
    }
}
```

#### Queue Creation
```c
QueueHandle_t thermostatQueue;
thermostatQueue = xQueueCreate(3, sizeof(struct ThermostatMessage));
```

#### Timer Creation
```c
TimerHandle_t timer = xTimerCreate(
    "timerName",
    pdMS_TO_TICKS(period_ms),
    pdTRUE,  // autoreload
    (void *)id,
    vTimerCallback
);
```

### Conditional Compilation

Use `#ifdef`/`#endif` with Kconfig options:
```c
#if CONFIG_MQTT_RELAYS_NB
    relays_init();
#endif // CONFIG_MQTT_RELAYS_NB

#ifdef CONFIG_SENSOR_SUPPORT
    xTaskCreate(sensors_read, "sensors_read", stack_size, NULL, 10, NULL);
#endif // CONFIG_SENSOR_SUPPORT
```

For indexed configuration:
```c
#if CONFIG_MQTT_RELAYS_NB > 1
    // handle second relay
#endif // CONFIG_MQTT_RELAYS_NB > 1
```

### Message Structures

Use unions for message data to save memory:
```c
struct ThermostatMessage {
    unsigned char msgType;
    unsigned char thermostatId;
    union ThermostatData data;
};

union ThermostatData {
    enum ThermostatMode thermostatMode;
    int targetTemperature;
    short currentTemperature;
};
```

### Test Code Style (C++ with Catch2)

```cpp
extern "C" {
#include "app_thermostat.h"
}

extern short targetTemperature[CONFIG_MQTT_THERMOSTATS_NB];

TEST_CASE("test_name", "[tag]") {
    MockRepository mocks;
    // Setup
    targetTemperature[0] = 125;
    
    // Expectations
    mocks.ExpectCallFunc(publish_persistent_data)
          .With(CString(topic), CString(data));
    
    // Execute
    publish_thermostat_target_temperature_evt(0);
}
```

### Memory Management

- Use `memset()` to initialize structures before use
- Define buffer sizes with constants: `char data[16];`
- Use FreeRTOS heap functions in embedded context

### Code Organization

- `main/` - Application source files
- `components/` - Reusable drivers (ds18x20, bme280, dht, etc.)
- `test-host/` - Host-based unit tests with mocks

### MQTT Topic Pattern

```
CONFIG_DEVICE_TYPE/CONFIG_CLIENT_ID/<action>/<service>/<id>
Example: device_type/client_id/evt/status/relay/0
         device_type/client_id/cmd/mode/thermostat/1
```

## Notes

- Firmware version is auto-generated from git describe in `main/version.h`
- NVS (Non-Volatile Storage) is used for persisting configuration
- Project supports both ESP8266 and ESP32 targets via CONFIG options
