#ifndef APP_THERMOSTAT_H
#define APP_THERMOSTAT_H


struct ThermostatCfgMessage {
  int32_t columnTargetTemperature;
  int32_t waterTargetTemperature;
  int32_t waterTemperatureSensibility;
  int32_t room0TargetTemperature;
  int32_t room0TemperatureSensibility;
};

struct ThermostatSensorsMessage {
  int32_t wtemperature;
  int32_t ctemperature;
};

struct ThermostatRoomMessage {
  int32_t temperature;
};

union ThermostatData {
  struct ThermostatCfgMessage cfgData;
  struct ThermostatSensorsMessage sensorsData;
  struct ThermostatRoomMessage roomData;
};

#define THERMOSTAT_CFG_MSG 1
#define THERMOSTAT_SENSORS_MSG 2
#define THERMOSTAT_ROOM_0_MSG 3

struct ThermostatMessage {
  unsigned char msgType;
  union ThermostatData data;
};

#include "mqtt_client.h"
void publish_thermostat_data(esp_mqtt_client_handle_t client);
void update_thermostat(esp_mqtt_client_handle_t client);

esp_err_t read_thermostat_nvs(const char * tag, int * value);

void handle_thermostat_cmd_task(void* pvParameters);
void read_nvs_thermostat_data(void);



#endif /* APP_THERMOSTAT_H */
