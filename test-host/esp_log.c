#include "esp_log.h"
#include <stdarg.h>
#include <stdio.h>

void stdout_log(const char* level, const char* tag, const char* format, ...)
{
  //FIXME enable log only is some switch is enabled
  /* va_list list; */
  /* va_start(list, format); */
  /* printf(format, list); */
  /* va_end(list); */
}

static vprintf_like_t esp_log_current_vprintf = NULL;

vprintf_like_t esp_log_set_vprintf(vprintf_like_t func)
{
  vprintf_like_t previous = esp_log_current_vprintf;
  esp_log_current_vprintf = func;
  return previous;
}

const char * esp_log_last_level_tag = NULL;
esp_log_level_t esp_log_last_level = ESP_LOG_NONE;

void esp_log_level_set(const char * tag, esp_log_level_t level)
{
  esp_log_last_level_tag = tag;
  esp_log_last_level = level;
}
