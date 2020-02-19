#ifndef APP_THERMOSTAT_H
#define APP_THERMOSTAT_H

#include "freertos/FreeRTOS.h"

#define BIT_THERMOSTAT 1
#define BIT_HEAT (1 << 1)

enum ThermostatMode {
  TERMOSTAT_MODE_UNSET = 0, //0
  TERMOSTAT_MODE_OFF = BIT_THERMOSTAT, //1
  TERMOSTAT_MODE_HEAT = BIT_THERMOSTAT | BIT_HEAT, //3
};

enum HoldOffMode {
  HOLD_OFF_DISABLED = 1,
  HOLD_OFF_ENABLED,
};

struct ThermostatCfgMessage {
  short circuitTargetTemperature;
  short waterTargetTemperature;
  short waterTemperatureSensibility;
  short room0TargetTemperature;
  short room0TemperatureSensibility;
  enum HoldOffMode holdOffMode;
};

struct ThermostatSensorsMessage {
  short wtemperature;
  short ctemperature;
};

struct ThermostatRoomMessage {
  short temperature;
};

union ThermostatData {
  struct ThermostatCfgMessage cfgData;
  struct ThermostatSensorsMessage sensorsData;
  struct ThermostatRoomMessage roomData;
  enum ThermostatMode thermostatMode;
  int targetTemperature;
  int tolerance;
};

#define THERMOSTAT_CFG_MSG 1
#define THERMOSTAT_SENSORS_MSG 2
#define THERMOSTAT_ROOM_0_MSG 3
#define THERMOSTAT_LIFE_TICK 4


//new modes
#define THERMOSTAT_CMD_MODE 6
#define WATER_THERMOSTAT_CMD_MODE 7
#define THERMOSTAT_CMD_TARGET_TEMPERATURE 8
#define WATER_THERMOSTAT_CMD_TARGET_TEMPERATURE 8
#define THERMOSTAT_CMD_TOLERANCE 9
#define WATER_THERMOSTAT_CMD_TOLERANCE 10

struct ThermostatMessage {
  unsigned char msgType;
  union ThermostatData data;
};

void publish_thermostat_data();

esp_err_t read_thermostat_nvs(const char * tag, int * value);

void handle_thermostat_cmd_task(void* pvParameters);
void read_nvs_thermostat_data(void);

#define SENSOR_LIFETIME 10

#endif /* APP_THERMOSTAT_H */
