#include <limits.h>

#include "catch.hpp"
#include "hippomocks.h"

#include "esp_system.h"
#include "app_thermostat.h"

using HippoMocks::CString;

extern "C" {
#include "app_mqtt.h"
}

extern "C" {
  void publish_thermostat_current_temperature_evt(int id);
  void publish_thermostat_target_temperature_evt(int id);
  void publish_thermostat_temperature_tolerance_evt(int id);
  void publish_thermostat_mode_evt(int id);
  void publish_thermostat_action_evt(int id);
  void publish_normal_thermostat_notification(enum ThermostatState state,
                                              unsigned int duration,
                                              const char *reason);
  void publish_circuit_thermostat_notification(enum HeatingState state,
                                               unsigned int duration);
}

extern short temperatureTolerance[CONFIG_MQTT_THERMOSTATS_NB];
extern short targetTemperature[CONFIG_MQTT_THERMOSTATS_NB];
extern short currentTemperature[CONFIG_MQTT_THERMOSTATS_NB];
extern short currentTemperatureFlag[CONFIG_MQTT_THERMOSTATS_NB];
extern enum ThermostatMode thermostatMode[CONFIG_MQTT_THERMOSTATS_NB];
extern enum ThermostatType thermostatType[CONFIG_MQTT_THERMOSTATS_NB];
extern enum ThermostatState thermostatState;
extern enum HeatingState heatingState;

TEST_CASE("publish_thermostat_current_temperature_evt_valid", "[tag]" ) {
  MockRepository mocks;
  const char* topic = "device_type/client_id/evt/ctemp/thermostat/0";
  const char* mqtt_data = "10.5";
  currentTemperature[0] = 105;
  currentTemperatureFlag[0] = 1;
  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(topic), CString(mqtt_data), QOS_1, RETAIN);

  publish_thermostat_current_temperature_evt(0);
}

TEST_CASE("publish_thermostat_current_temperature_evt_invalid", "[tag]" ) {
  MockRepository mocks;
  currentTemperature[0] = SHRT_MIN;
  mocks.NeverCallFunc(mqtt_publish_data);

  publish_thermostat_current_temperature_evt(0);
}

TEST_CASE("publish_thermostat_target_temperature_evt", "[tag]" ) {
  MockRepository mocks;
  const char* topic = "device_type/client_id/evt/temp/thermostat/0";
  const char* mqtt_data = "12.5";
  targetTemperature[0] = 125;
  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(topic), CString(mqtt_data), QOS_1, RETAIN);

  publish_thermostat_target_temperature_evt(0);
}

TEST_CASE("publish_thermostat_temperature_tolerance_evt", "[tag]" ) {
  MockRepository mocks;
  const char* topic = "device_type/client_id/evt/tolerance/thermostat/0";
  const char* mqtt_data = "0.3";
  temperatureTolerance[0] = 3;
  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(topic), CString(mqtt_data), QOS_1, RETAIN);

  publish_thermostat_temperature_tolerance_evt(0);
}

TEST_CASE("publish_thermostat_mode_evt_unset", "[tag]" ) {
  MockRepository mocks;
  const char* topic = "device_type/client_id/evt/mode/thermostat/0";
  const char* mqtt_data = "off";
  thermostatMode[0] = THERMOSTAT_MODE_UNSET;
  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(topic), CString(mqtt_data), QOS_1, RETAIN);

  publish_thermostat_mode_evt(0);
}

TEST_CASE("publish_thermostat_mode_evt_off", "[tag]" ) {
  MockRepository mocks;
  const char* topic = "device_type/client_id/evt/mode/thermostat/0";
  const char* mqtt_data = "off";
  thermostatMode[0] = THERMOSTAT_MODE_OFF;
  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(topic), CString(mqtt_data), QOS_1, RETAIN);

  publish_thermostat_mode_evt(0);
}

TEST_CASE("publish_thermostat_mode_evt_heat", "[tag]" ) {
  MockRepository mocks;
  const char* topic = "device_type/client_id/evt/mode/thermostat/0";
  const char* mqtt_data = "heat";
  thermostatMode[0] = THERMOSTAT_MODE_HEAT;
  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(topic), CString(mqtt_data), QOS_1, RETAIN);

  publish_thermostat_mode_evt(0);
}

TEST_CASE("publish_normal_thermostat_action_evt_unset", "[tag]" ) {
  MockRepository mocks;
  const char* topic = "device_type/client_id/evt/action/thermostat/0";
  const char* mqtt_data = "off";
  thermostatType[0] = THERMOSTAT_TYPE_NORMAL;
  thermostatMode[0] = THERMOSTAT_MODE_UNSET;
  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(topic), CString(mqtt_data), QOS_1, RETAIN);

  publish_thermostat_action_evt(0);
}

TEST_CASE("publish_normal_thermostat_action_evt_off", "[tag]" ) {
  MockRepository mocks;
  const char* topic = "device_type/client_id/evt/action/thermostat/0";
  const char* mqtt_data = "off";
  thermostatMode[0] = THERMOSTAT_MODE_OFF;
  thermostatType[0] = THERMOSTAT_TYPE_NORMAL;
  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(topic), CString(mqtt_data), QOS_1, RETAIN);

  publish_thermostat_action_evt(0);
}

TEST_CASE("publish_normal_thermostat_action_evt_heat_idle", "[tag]" ) {
  MockRepository mocks;
  const char* topic = "device_type/client_id/evt/action/thermostat/0";
  const char* mqtt_data = "idle";
  thermostatMode[0] = THERMOSTAT_MODE_HEAT;
  thermostatType[0] = THERMOSTAT_TYPE_NORMAL;
  thermostatState = THERMOSTAT_STATE_IDLE;
  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(topic), CString(mqtt_data), QOS_1, RETAIN);

  publish_thermostat_action_evt(0);
}

TEST_CASE("publish_normal_thermostat_action_evt_heat_heating", "[tag]" ) {
  MockRepository mocks;
  const char* topic = "device_type/client_id/evt/action/thermostat/0";
  const char* mqtt_data = "heating";
  thermostatMode[0] = THERMOSTAT_MODE_HEAT;
  thermostatType[0] = THERMOSTAT_TYPE_NORMAL;
  thermostatState = THERMOSTAT_STATE_HEATING;
  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(topic), CString(mqtt_data), QOS_1, RETAIN);

  publish_thermostat_action_evt(0);
}

TEST_CASE("publish_circuit_thermostat_action_evt_unset", "[tag]" ) {
  MockRepository mocks;
  const char* topic = "device_type/client_id/evt/action/thermostat/0";
  const char* mqtt_data = "off";
  thermostatMode[0] = THERMOSTAT_MODE_UNSET;
  thermostatType[0] = THERMOSTAT_TYPE_CIRCUIT;
  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(topic), CString(mqtt_data), QOS_1, RETAIN);

  publish_thermostat_action_evt(0);
}

TEST_CASE("publish_circuit_thermostat_action_evt_off", "[tag]" ) {
  MockRepository mocks;
  const char* topic = "device_type/client_id/evt/action/thermostat/0";
  const char* mqtt_data = "off";
  thermostatMode[0] = THERMOSTAT_MODE_OFF;
  thermostatType[0] = THERMOSTAT_TYPE_CIRCUIT;
  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(topic), CString(mqtt_data), QOS_1, RETAIN);

  publish_thermostat_action_evt(0);
}

TEST_CASE("publish_circuit_thermostat_action_evt_heat_idle", "[tag]" ) {
  MockRepository mocks;
  const char* topic = "device_type/client_id/evt/action/thermostat/0";
  const char* mqtt_data = "idle";
  thermostatMode[0] = THERMOSTAT_MODE_HEAT;
  thermostatType[0] = THERMOSTAT_TYPE_CIRCUIT;
  heatingState = HEATING_STATE_IDLE;
  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(topic), CString(mqtt_data), QOS_1, RETAIN);

  publish_thermostat_action_evt(0);
}

TEST_CASE("publish_circuit_thermostat_action_evt_heat_heating", "[tag]" ) {
  MockRepository mocks;
  const char* topic = "device_type/client_id/evt/action/thermostat/0";
  const char* mqtt_data = "heating";
  thermostatMode[0] = THERMOSTAT_MODE_HEAT;
  thermostatType[0] = THERMOSTAT_TYPE_CIRCUIT;
  heatingState = HEATING_STATE_ENABLED;
  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(topic), CString(mqtt_data), QOS_1, RETAIN);

  publish_thermostat_action_evt(0);
}

TEST_CASE("publish_normal_thermostat_notification_on", "[tag]" ) {
  MockRepository mocks;
  const char* notification_topic = "device_type/client_id/evt/notification/thermostat";
  const char* notification_mqtt_data = "Thermostat state changed to on due to some reason. It was off for 10 minutes";
  const char* action_topic = "device_type/client_id/evt/action/thermostat/0";
  const char* action_mqtt_data = "heating";
  thermostatMode[0] = THERMOSTAT_MODE_HEAT;
  thermostatType[0] = THERMOSTAT_TYPE_NORMAL;
  thermostatState = THERMOSTAT_STATE_HEATING;

  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(notification_topic), CString(notification_mqtt_data), QOS_0, NO_RETAIN);
  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(action_topic), CString(action_mqtt_data), QOS_1, RETAIN);

  publish_normal_thermostat_notification(thermostatState, 10, "some reason");
  publish_thermostat_action_evt(0);
}

TEST_CASE("publish_normal_thermostat_notification_off", "[tag]" ) {
  MockRepository mocks;
  const char* notification_topic = "device_type/client_id/evt/notification/thermostat";
  const char* notification_mqtt_data = "Thermostat state changed to off due to some reason. It was on for 10 minutes";
  const char* action_topic = "device_type/client_id/evt/action/thermostat/0";
  const char* action_mqtt_data = "idle";
  thermostatMode[0] = THERMOSTAT_MODE_HEAT;
  thermostatType[0] = THERMOSTAT_TYPE_NORMAL;
  thermostatState = THERMOSTAT_STATE_IDLE;

  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(notification_topic), CString(notification_mqtt_data), QOS_0, NO_RETAIN);
  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(action_topic), CString(action_mqtt_data), QOS_1, RETAIN);

  publish_normal_thermostat_notification(thermostatState, 10, "some reason");
  publish_thermostat_action_evt(0);
}


TEST_CASE("publish_circuit_thermostat_notification_off", "[tag]" ) {
  MockRepository mocks;
  const char* notification_topic = "device_type/client_id/evt/notification/thermostat";
  const char* notification_mqtt_data = "Heating state changed to off. It was on for 10 minutes";
  const char* action_topic = "device_type/client_id/evt/action/thermostat/0";
  const char* action_mqtt_data = "idle";
  thermostatMode[0] = THERMOSTAT_MODE_HEAT;
  thermostatType[0] = THERMOSTAT_TYPE_CIRCUIT;
  heatingState = HEATING_STATE_IDLE;

  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(notification_topic), CString(notification_mqtt_data), QOS_0, NO_RETAIN);
  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(action_topic), CString(action_mqtt_data), QOS_1, RETAIN);

  publish_circuit_thermostat_notification(heatingState, 10);
  publish_thermostat_action_evt(0);
}
TEST_CASE("publish_circuit_thermostat_notification_on", "[tag]" ) {
  MockRepository mocks;
  const char* notification_topic = "device_type/client_id/evt/notification/thermostat";
  const char* notification_mqtt_data = "Heating state changed to on. It was off for 10 minutes";
  const char* action_topic = "device_type/client_id/evt/action/thermostat/0";
  const char* action_mqtt_data = "heating";
  thermostatMode[0] = THERMOSTAT_MODE_HEAT;
  thermostatType[0] = THERMOSTAT_TYPE_CIRCUIT;
  heatingState = HEATING_STATE_ENABLED;

  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(notification_topic), CString(notification_mqtt_data), QOS_0, NO_RETAIN);
  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(action_topic), CString(action_mqtt_data), QOS_1, RETAIN);

  publish_circuit_thermostat_notification(heatingState, 10);
  publish_thermostat_action_evt(0);
}


// TEST_CASE("handle_room_update", "[tag]" ) {
//   room0Temperature = SHRT_MIN;
//   room0TemperatureFlag = 0;

//   handle_room_temperature_msg(SHRT_MIN);
//   REQUIRE(room0Temperature == SHRT_MIN);
//   REQUIRE(room0TemperatureFlag == 0);

//   handle_room_temperature_msg(100); //10.0 degrees
//   REQUIRE(room0Temperature == 100);
//   REQUIRE(room0TemperatureFlag == SENSOR_LIFETIME);

//   handle_room_temperature_msg(100); //10.0 degrees
//   REQUIRE(room0Temperature == 100);
//   REQUIRE(room0TemperatureFlag == SENSOR_LIFETIME);

//   handle_room_temperature_msg(110); //11.0 degrees
//   REQUIRE(room0Temperature == 110);
//   REQUIRE(room0TemperatureFlag == SENSOR_LIFETIME);

//   handle_room_temperature_msg(SHRT_MIN); //11.0 degrees
//   REQUIRE(room0Temperature == 110);
//   REQUIRE(room0TemperatureFlag == SENSOR_LIFETIME);

//   room0TemperatureFlag=1;
//   handle_room_temperature_msg(SHRT_MIN); //11.0 degrees
//   REQUIRE(room0Temperature == 110);
//   REQUIRE(room0TemperatureFlag == 1);
// }
