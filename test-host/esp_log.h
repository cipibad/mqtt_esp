void stdout_log(const char* level, const char* tag, const char* format, ...);

#define ESP_LOGE( tag, format, ... ) stdout_log("Error:", tag, format, ##__VA_ARGS__)
#define ESP_LOGW( tag, format, ... ) stdout_log("Warning:", tag, format, ##__VA_ARGS__)
#define ESP_LOGI( tag, format, ... ) stdout_log("Info:", tag, format, ##__VA_ARGS__)
#define ESP_LOGD( tag, format, ... ) stdout_log("Debug:", tag, format, ##__VA_ARGS__)
#define ESP_LOGV( tag, format, ... ) stdout_log("Verbose:", tag, format, ##__VA_ARGS__)
