#include "esp_log.h"
#include <stdarg.h>

void stdout_log(const char* level, const char* tag, const char* format, ...)
{

  va_list list;
  va_start(list, format);
  printf(format, list);
  va_end(list);

}

