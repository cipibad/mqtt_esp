#ifndef APP_THERMOSTAT_H
#define APP_THERMOSTAT_H


struct ThermostatMessage {
  float targetTemperature;
  float targetTemperatureSensibility;
};

#ifdef ESP8266
#include "MQTTClient.h"
void publish_thermostat_data(MQTTClient* client);
void update_thermostat(MQTTClient* client);
#endif //ESP8266

#ifdef ESP32
#include "mqtt_client.h"
void publish_thermostat_data(esp_mqtt_client_handle_t client);
void update_thermostat(esp_mqtt_client_handle_t client);
#endif //ESP32

esp_err_t read_thermostat_nvs(const char * tag, int * value);

void handle_thermostat_cmd_task(void* pvParameters);



#endif /* APP_THERMOSTAT_H */
