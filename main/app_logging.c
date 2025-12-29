#include "esp_system.h"
#include <stdarg.h>
#include <string.h>
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sdkconfig.h"

#include "app_publish_data.h"


// Rate limiting
static TickType_t last_log_tick = 0;

static bool should_log()
{
  if (CONFIG_MQTT_LOG_RATE_LIMIT_MS == 0) {
    return true;
  }

  TickType_t current_tick = xTaskGetTickCount();
  TickType_t elapsed = current_tick - last_log_tick;
  TickType_t limit_ticks = CONFIG_MQTT_LOG_RATE_LIMIT_MS / portTICK_PERIOD_MS;

  if (elapsed >= limit_ticks) {
    last_log_tick = current_tick;
    return true;
  }
  return false;
}

void publish_log_message(const char *level, const char *module, const char *message)
{
  if (!should_log()) {
    return;
  }

  char log_topic[128];
  char payload[256];
  TickType_t tick_count = xTaskGetTickCount();

  sprintf(log_topic, "device/%s/evt/log/%s", CONFIG_CLIENT_ID, level);
  sprintf(payload, "[%lu] [%s] %s, message: %s",
          (unsigned long)tick_count,
          level, module, message);

  publish_non_persistent_data(log_topic, payload);
}

void publish_error_log(const char *module, const char *format, ...)
{
  char buffer[128];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  publish_log_message("error", module, buffer);
}

void publish_warning_log(const char *module, const char *format, ...)
{
  char buffer[128];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  publish_log_message("warning", module, buffer);
}

void publish_info_log(const char *module, const char *format, ...)
{
  char buffer[128];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  publish_log_message("info", module, buffer);
}

#ifdef CONFIG_MQTT_LOG_LEVEL_DEBUG
void publish_debug_log(const char *module, const char *format, ...)
{
  char buffer[128];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  publish_log_message("debug", module, buffer);
}
#endif
