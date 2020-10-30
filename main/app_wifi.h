#ifndef APP_WIFI_H
#define APP_WIFI_H

#include "esp_system.h"

#ifdef CONFIG_TARGET_DEVICE_ESP32
#define MAX_WIFI_SSID_LEN 33
#define MAX_WIFI_PASS_LEN 65
#endif //CONFIG_TARGET_DEVICE_ESP32

#ifdef CONFIG_TARGET_DEVICE_ESP8266
#define MAX_WIFI_CONFIG_LEN 33
#endif //CONFIG_TARGET_DEVICE_ESP8266

void wifi_init(void);

#endif /* APP_WIFI_H */
