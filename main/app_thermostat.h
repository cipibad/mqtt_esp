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

enum ThermostatState {
  THERMOSTAT_STATE_IDLE = 1,
  THERMOSTAT_STATE_HEATING = 3,
};

enum HoldOffMode {
  HOLD_OFF_DISABLED = 1,
  HOLD_OFF_ENABLED,
};

struct ThermostatSensorsMessage {
  short wtemperature;
  short ctemperature;
};

struct ThermostatRoomMessage {
  short temperature;
};

union ThermostatData {
  struct ThermostatSensorsMessage sensorsData;
  struct ThermostatRoomMessage roomData;
  enum ThermostatMode thermostatMode;
  int targetTemperature;
  int tolerance;
};

#define THERMOSTAT_SENSORS_MSG 2
#define THERMOSTAT_ROOM_0_MSG 3
#define THERMOSTAT_LIFE_TICK 4


//new modes
//FIXME: translate this to enum values
#define THERMOSTAT_CMD_MODE 6
#define WATER_THERMOSTAT_CMD_MODE 7
#define THERMOSTAT_CMD_TARGET_TEMPERATURE 8
#define WATER_THERMOSTAT_CMD_TARGET_TEMPERATURE 9
#define THERMOSTAT_CMD_TOLERANCE 10
#define WATER_THERMOSTAT_CMD_TOLERANCE 11
#define CO_THERMOSTAT_CMD_MODE 12
#define CO_THERMOSTAT_CMD_TARGET_TEMPERATURE 13

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
