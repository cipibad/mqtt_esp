# Agent Guidelines for ESP8266/ESP32 MQTT Firmware

This document provides build commands and code style guidelines for working with this ESP-IDF based IoT firmware.

## Build Commands

### Main Firmware
```bash
# Build the project (ESP-IDF build system)
make

# Clean build artifacts
make clean

# Flash to device (with proper configuration)
make flash
```

### Test Suite (Host-based)
Tests are located in `test-host/` directory and use Catch2 framework.

```bash
# Build all tests
cd test-host && make

# Run all tests
cd test-host && make test
# Or: ./test_mqtt_esp

# Run a specific test by name/tag
./test_mqtt_esp "[tag]"

# Run long tests
make long-test

# Generate coverage report
make coverage_report

# Clean test artifacts
make clean
```

## Code Style Guidelines

### Imports/Includes
Organize includes in this order:
1. System headers (`esp_system.h`, `esp_log.h`, `nvs_flash.h`)
2. FreeRTOS headers (`freertos/FreeRTOS.h`, `freertos/task.h`, `freertos/queue.h`, `freertos/event_groups.h`, `freertos/semphr.h`)
3. Driver headers (`driver/gpio.h`, `rom/gpio.h`)
4. Standard C headers (`string.h`, `stdlib.h`)
5. ESP-IDF library headers (`mqtt_client.h`)
6. Project headers (`app_main.h`, `app_mqtt.h`, etc.)
7. Component headers (`cJSON.h`)

Use `extern "C"` for C headers in C++ test files.

### Formatting
- Indentation: 2 spaces
- Opening braces: Same line for functions, new line for control structures
- Spaces around operators and after commas
- Maximum line length: No strict limit, but prefer ~80-100 chars
- Use `//` for comments, rarely `/* */`

### Naming Conventions
- **Files**: `app_<module>.c` and `app_<module>.h` (e.g., `app_mqtt.c`, `app_relay.h`)
- **Functions**: `snake_case` (e.g., `mqtt_init_and_start`, `handle_relay_task`, `publish_sensors_data`)
- **Constants**: `UPPER_CASE_WITH_UNDERSCORES` (e.g., `MQTT_CONNECTED_BIT`, `RELAY_STATUS_ON`, `SENSOR_LIFETIME`)
- **Enums**: `UPPER_CASE_WITH_UNDERSCORES` (e.g., `THERMOSTAT_MODE_OFF`, `RELAY_CMD_STATUS`, `HEATING_STATE_IDLE`)
- **Structs**: `PascalCase` (e.g., `RelayMessage`, `ThermostatMessage`, `SchedulerCfgMessage`)
- **Variables**: `snake_case` (e.g., `mqttQueue`, `thermostatId`, `connect_reason`)
- **Task names**: `snake_case` with descriptive names (e.g., `handle_mqtt_sub_pub`, `sensors_read`)
- **Globals**: Prefix with `g_` or use extern in header (e.g., `mqtt_event_group`, `client`)

### Types
- Use `esp_err_t` for ESP-IDF error returns
- Use `uint8_t`, `uint16_t`, `int8_t`, `int16_t`, `unsigned char`, `signed char` for specific-width integers
- Use `unsigned char` for message IDs and small numeric values
- Use `short` (or `int16_t`) for temperatures (stored as tenths of degrees)
- Use `int` for general-purpose integers
- Use `bool` for boolean values with `true`/`false`
- Use `char*` for strings, not `std::string` (C codebase)
- Struct members follow naming conventions of their type

### Error Handling
- Use `ESP_ERROR_CHECK()` for critical operations that must succeed
- Use `esp_err_t` return types for functions that can fail
- Log errors with `ESP_LOGE(TAG, "message")`
- Log warnings with `ESP_LOGW(TAG, "message")`
- Log info with `ESP_LOGI(TAG, "message")`
- Return `ESP_OK` on success
- Return appropriate ESP_ERR_* codes on failure
- Always check return values from ESP-IDF API calls
- For queue operations, check `pdPASS` return value and log errors if send fails

### Conditional Compilation
- Use `#ifdef CONFIG_*` for feature flags (defined in Kconfig)
- Use `#if CONFIG_* > 0` for numeric configuration values
- Use `#ifdef CONFIG_TARGET_DEVICE_ESP32` and `#ifdef CONFIG_TARGET_DEVICE_ESP8266` for platform-specific code
- Nest conditionals logically but avoid excessive depth
- Comment complex conditional logic with `//` comments
- Use `extern` declarations for conditionally compiled variables
- Wrap entire functions/modules in conditionals when appropriate

### FreeRTOS Tasks
- Create tasks with `xTaskCreate()`
- Task names: descriptive strings like `"handle_mqtt_sub_pub"`
- Stack sizes: Use `configMINIMAL_STACK_SIZE * multiplier` (e.g., `* 3`, `* 9`)
- Priority: Typically 5 for application tasks, 3 for LED tasks, 10 for sensor tasks
- Task functions: `void task_name(void* pvParameters)`
- Use `portMAX_DELAY` for blocking operations
- Use `portTICK_PERIOD_MS` for time conversions

### Message Passing
- Use FreeRTOS Queues (`xQueueCreate`, `xQueueSend`, `xQueueReceive`)
- Queue handles: Global `QueueHandle_t` variables (e.g., `mqttQueue`, `relayQueue`)
- Message structs: Include `msgType` field and data union
- Timeout values: Define as `*_QUEUE_TIMEOUT` macros
- Use `xQueueSendFromISR` in interrupt handlers
- Always check queue send/receive return values

### Structs and Unions
- Define message types with enums
- Use unions for variant message data
- Initialize structs with `memset(&struct, 0, sizeof(struct))`
- Message structs: `{ msgType, id, union data }` pattern
- Always use explicit sizes in `memset` and `memcpy`

### Logging
- Define TAG at file scope: `static const char *TAG = "MODULE_NAME";`
- Set log levels: `esp_log_level_set("*", ESP_LOG_INFO)`
- Use descriptive log messages with relevant data
- Log state changes and important events
- Reduce verbose logging in production builds

### Constants and Macros
- Define numeric constants in macros at top of files
- Use feature-specific prefixes (e.g., `MQTT_`, `THERMOSTAT_`, `RELAY_`)
- Define topic strings as constants: `const char *topic = "..."`
- Use macros for configurable timeouts and limits
- Document magic numbers with named constants

### Component Organization
- Main application code in `main/`
- Components in `components/` (e.g., `bme280`, `dht`, `ds18x20`, `onewire`)
- Each component has `component.mk` and source files
- Test code in `test-host/` directory
- HTTP server files in `main/http/`

### Configuration
- All compile-time configuration in `Kconfig.projbuild`
- Configuration accessed as `CONFIG_*` macros
- Feature flags enable/disable entire subsystems
- Numeric config values define counts, timeouts, GPIOs
- Platform selection via choice menus

### Testing
- Unit tests in `test-host/*.cc` using Catch2
- Use HippoMocks for mocking in tests
- Test naming: `TEST_CASE("test_name", "[tag]")`
- Mock external dependencies (ESP-IDF functions, queues)
- Test both success and failure paths
- Use `REQUIRE()` for assertions
- Run tests after making changes to affected modules

### Memory Management
- Static allocation preferred over dynamic
- Use fixed-size buffers for messages
- Always check buffer sizes before copying
- Use `memcpy()` and `memset()` for buffer operations
- Avoid heap allocation in ISRs
- Be mindful of ESP8266 memory constraints
