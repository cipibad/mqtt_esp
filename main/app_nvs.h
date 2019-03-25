#ifndef APP_NVS_H
#define APP_NVS_H


esp_err_t write_nvs_integer(const char * tag, int value);
esp_err_t read_nvs_integer(const char * tag, int * value);
esp_err_t write_nvs_str(const char * tag, char * value);
esp_err_t read_nvs_str(const char * tag, char * value, size_t * length);


#endif /* APP_NVS_H */
