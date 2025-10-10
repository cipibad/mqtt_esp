#ifndef APP_THERMOSTAT_H
#define APP_THERMOSTAT_H

#include "freertos/FreeRTOS.h"

#define BIT_THERMOSTAT 1
#define BIT_HEAT (1 << 1)

enum ThermostatType {
  THERMOSTAT_TYPE_NORMAL = 1,
  THERMOSTAT_TYPE_CIRCUIT,
};


enum ThermostatMode {
  THERMOSTAT_MODE_UNSET = 0, //0
  THERMOSTAT_MODE_OFF = BIT_THERMOSTAT, //1
  THERMOSTAT_MODE_HEAT = BIT_THERMOSTAT | BIT_HEAT, //3
};

enum ThermostatState {
  THERMOSTAT_STATE_IDLE = 1,
  THERMOSTAT_STATE_HEATING = 3,
};

enum HeatingState {
  HEATING_STATE_IDLE = 1,
  HEATING_STATE_ENABLED = 3,
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

struct ThermostatMqttSensorsMessage {
  unsigned char relayId;
  short temperature;
};


union ThermostatData {
  struct ThermostatSensorsMessage sensorsData;
  struct ThermostatRoomMessage roomData;
  enum ThermostatMode thermostatMode;
  int targetTemperature;
  int tolerance;
  short currentTemperature;
};

enum ThermostatMsgType {
  THERMOSTAT_SENSORS_MSG = 2,
  THERMOSTAT_ROOM_0_MSG = 3,
  THERMOSTAT_LIFE_TICK = 4,
  //new modes
  THERMOSTAT_CMD_MODE = 6,
  THERMOSTAT_CMD_TARGET_TEMPERATURE = 7,
  THERMOSTAT_CMD_TOLERANCE = 8,
  THERMOSTAT_CMD_BUMP = 9,
  THERMOSTAT_CURRENT_TEMPERATURE = 10,
  // from scheduler
  THERMOSTAT_CMD_WATER_TEMP_LOW = 11,
  THERMOSTAT_CMD_WATER_TEMP_HIGH = 12,
  THERMOSTAT_CMD_ROOM_TEMP_SLIGHT_INC,
  THERMOSTAT_CMD_ROOM_TEMP_MODERATE_INC,
  THERMOSTAT_CMD_ROOM_TEMP_SLIGHT_DEC,
  THERMOSTAT_CMD_ROOM_TEMP_MODERATE_DEC,
};

struct ThermostatMessage {
  enum ThermostatMsgType msgType;
  unsigned char thermostatId;
  union ThermostatData data;
};

void publish_thermostat_data();

esp_err_t read_thermostat_nvs(const char * tag, int * value);

void handle_thermostat_cmd_task(void* pvParameters);
void read_nvs_thermostat_data(void);

void thermostat_publish_local_data(int thermostat_id, int value);

#define SENSOR_LIFETIME 10

#define THERMOSTAT_TIMEOUT 60
#define THERMOSTAT_QUEUE_TIMEOUT (THERMOSTAT_TIMEOUT * 1000 / portTICK_PERIOD_MS)

#endif /* APP_THERMOSTAT_H */
