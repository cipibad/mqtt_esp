#include <limits.h>

#include "catch.hpp"
#include "hippomocks.h"

#include "app_thermostat.h"

using HippoMocks::CString;

extern "C" {
#include "app_mqtt.h"
}

extern "C" {
  void publish_thermostat_cfg();
  void handle_room_temperature_msg(short);
}

TEST_CASE("publish_thermostat_cfg", "[tag]" ) {
  MockRepository mocks;
  const char* topic = "device_type/client_id/evt/thermostat/cfg";
  const char* mqtt_data = "\
{\"circuitTargetTemperature\":23.0, \"waterTargetTemperature\":23.0, \
\"waterTemperatureSensibility\":0.5, \"room0TargetTemperature\":22.0, \
\"room0TemperatureSensibility\":0.2, \"thermostatMode\":1, \"holdOffMode\":1}";

  mocks.ExpectCallFunc(mqtt_publish_data).With(CString(topic), CString(mqtt_data), QOS_1, RETAIN);

  publish_thermostat_cfg();
  REQUIRE(true);
}

TEST_CASE("handle_room_update", "[tag]" ) {
  extern short room0Temperature;
  extern unsigned char room0TemperatureFlag;

  REQUIRE(room0Temperature == SHRT_MIN);
  REQUIRE(room0TemperatureFlag == 0);

  handle_room_temperature_msg(SHRT_MIN);
  REQUIRE(room0Temperature == SHRT_MIN);
  REQUIRE(room0TemperatureFlag == 0);

  handle_room_temperature_msg(100); //10.0 degrees
  REQUIRE(room0Temperature == 100);
  REQUIRE(room0TemperatureFlag == SENSOR_LIFETIME);

  handle_room_temperature_msg(100); //10.0 degrees
  REQUIRE(room0Temperature == 100);
  REQUIRE(room0TemperatureFlag == SENSOR_LIFETIME);

  handle_room_temperature_msg(110); //11.0 degrees
  REQUIRE(room0Temperature == 110);
  REQUIRE(room0TemperatureFlag == SENSOR_LIFETIME);

  handle_room_temperature_msg(SHRT_MIN); //11.0 degrees
  REQUIRE(room0Temperature == 110);
  REQUIRE(room0TemperatureFlag == SENSOR_LIFETIME);

  room0TemperatureFlag=1;
  handle_room_temperature_msg(SHRT_MIN); //11.0 degrees
  REQUIRE(room0Temperature == 110);
  REQUIRE(room0TemperatureFlag == 1);
}
