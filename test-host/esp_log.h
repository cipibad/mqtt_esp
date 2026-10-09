#ifndef ESP_LOG_HOST_H
#define ESP_LOG_HOST_H

#include <stdarg.h>

void stdout_log(const char* level, const char* tag, const char* format, ...);

#define ESP_LOGE( tag, format, ... ) stdout_log("Error:", tag, format, ##__VA_ARGS__)
#define ESP_LOGW( tag, format, ... ) stdout_log("Warning:", tag, format, ##__VA_ARGS__)
#define ESP_LOGI( tag, format, ... ) stdout_log("Info:", tag, format, ##__VA_ARGS__)
#define ESP_LOGD( tag, format, ... ) stdout_log("Debug:", tag, format, ##__VA_ARGS__)
#define ESP_LOGV( tag, format, ... ) stdout_log("Verbose:", tag, format, ##__VA_ARGS__)

typedef int (*vprintf_like_t)(const char *, va_list);
vprintf_like_t esp_log_set_vprintf(vprintf_like_t func);

typedef enum {
  ESP_LOG_NONE = 0,
  ESP_LOG_ERROR,
  ESP_LOG_WARN,
  ESP_LOG_INFO,
  ESP_LOG_DEBUG,
  ESP_LOG_VERBOSE
} esp_log_level_t;

void esp_log_level_set(const char * tag, esp_log_level_t level);

//test inspection helpers, last values passed to esp_log_level_set
extern const char * esp_log_last_level_tag;
extern esp_log_level_t esp_log_last_level;

#endif /* ESP_LOG_HOST_H */
