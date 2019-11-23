#include "esp_system.h"
#include "catch.hpp"
#include "hippomocks.h"
#include "cJSON.h"

using HippoMocks::CString;

extern "C" {
  bool getTemperatureValue(short* value, const cJSON* root, const char* tag);
}

TEST_CASE("test get ", "[tag]" ) {
  short temp=0;
  const char* json = "{\"humidity\":53.3,\"temperature\":18.3}";
  cJSON * root   = cJSON_Parse(json);

  REQUIRE(getTemperatureValue(&temp, root, "temperature"));
  REQUIRE(temp == 183);
}
