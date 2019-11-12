#ifndef APP_THERMOSTAT_H
#define APP_THERMOSTAT_H

#define BIT_THERMOSTAT 1
#define BIT_WATER_SENSOR (1 << 1)
#define BIT_ROOM_SENSOR (1 << 2)
#define BIT_HOLD_OFF (1 << 3)

enum ThermostatMode {
  TERMOSTAT_MODE_OFF = BIT_THERMOSTAT, //1
  TERMOSTAT_MODE_SUMMER = BIT_WATER_SENSOR | BIT_THERMOSTAT, //3
  TERMOSTAT_MODE_SPRING_AUTUMN = BIT_WATER_SENSOR | BIT_ROOM_SENSOR | BIT_THERMOSTAT, //7
  TERMOSTAT_MODE_WINTER = BIT_ROOM_SENSOR | BIT_THERMOSTAT, //5
  TERMOSTAT_MODE_SUMMER_HOLD_OFF = BIT_HOLD_OFF | BIT_WATER_SENSOR | BIT_THERMOSTAT, //11
  TERMOSTAT_MODE_SPRING_AUTUMN_HOLD_OFF = BIT_HOLD_OFF | BIT_WATER_SENSOR | BIT_ROOM_SENSOR | BIT_THERMOSTAT, //15
  TERMOSTAT_MODE_WINTER_HOLD_OFF = BIT_HOLD_OFF | BIT_ROOM_SENSOR | BIT_THERMOSTAT, //13
};

struct ThermostatCfgMessage {
  int32_t circuitTargetTemperature;
  int32_t waterTargetTemperature;
  int32_t waterTemperatureSensibility;
  int32_t room0TargetTemperature;
  int32_t room0TemperatureSensibility;
  enum ThermostatMode thermostatMode;
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
#define THERMOSTAT_LIFE_TICK 4

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
