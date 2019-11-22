#include "esp_system.h"
#include "catch.hpp"
#include "hippomocks.h"
using HippoMocks::CString;

extern "C" {
#include "app_mqtt.h"
}

extern "C" {
  void publish_thermostat_cfg();
}

TEST_CASE("test 1", "[tag]" ) {
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

TEST_CASE("test 2", "[tag]" ) {
  REQUIRE(true);
}
