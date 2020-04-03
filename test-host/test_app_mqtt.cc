#include "esp_system.h"
#include "catch.hpp"
#include "hippomocks.h"
#include "cJSON.h"

using HippoMocks::CString;

extern "C" {
}

TEST_CASE("test get ", "[tag]" ) {
  short temp=0;
  REQUIRE(temp == 0);
}
