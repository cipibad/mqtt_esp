#ifndef APP_THERMOSTAT_H
#define APP_THERMOSTAT_H



#define BIT_WATER_SENSOR = 1;
#define BIT_ROOM_SENSOR = 1 << 1;


enum ThermostatMode {
  TERMOSTAT_MODE_OFF = 0, 
  TERMOSTAT_MODE_SUMMER = BIT_WATER_SENSOR, //1
  TERMOSTAT_MODE_SPRING_AUTUMN = BIT_WATER_SENSOR | BIT_ROOM_SENSOR, //3
  TERMOSTAT_MODE_WINTER = BIT_ROOM_SENSOR, //2
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
