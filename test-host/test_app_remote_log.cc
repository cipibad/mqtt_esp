#include <string.h>

#include "catch.hpp"

#include "esp_system.h"

extern "C" {
#include "app_remote_log.h"
#include "app_publish_data.h"
#include "esp_log.h"
}

static char vprintf_captured[160];

static int capture_vprintf(const char* fmt, va_list l)
{
  return vsnprintf(vprintf_captured, sizeof(vprintf_captured), fmt, l);
}

static int call_remote_log_vprintf(const char* fmt, ...)
{
  va_list l;
  va_start(l, fmt);
  int len = remote_log_vprintf(fmt, l);
  va_end(l);
  return len;
}

TEST_CASE("remote_log_parse_line_valid", "[remote_log]" ) {
  char tag[REMOTE_LOG_MAX_TAG_LEN];
  char payload[64];

  bool parsed = remote_log_parse_line("I (6123) APP_THERMOSTAT: target 21.5\n",
                                      tag, sizeof(tag), payload, sizeof(payload));

  REQUIRE(parsed == true);
  REQUIRE(strcmp(tag, "APP_THERMOSTAT") == 0);
  REQUIRE(strcmp(payload, "target 21.5") == 0);
}

TEST_CASE("remote_log_parse_line_with_ansi_color", "[remote_log]" ) {
  char tag[REMOTE_LOG_MAX_TAG_LEN];
  char payload[64];

  bool parsed = remote_log_parse_line("\x1b[0;32mI (12) SCHEDULER: tick done",
                                      tag, sizeof(tag), payload, sizeof(payload));

  REQUIRE(parsed == true);
  REQUIRE(strcmp(tag, "SCHEDULER") == 0);
  REQUIRE(strcmp(payload, "tick done") == 0);
}

TEST_CASE("remote_log_parse_line_rejects_plain_text", "[remote_log]" ) {
  char tag[REMOTE_LOG_MAX_TAG_LEN];

  REQUIRE(remote_log_parse_line("reset reason: 5", tag, sizeof(tag), NULL, 0) == false);
  REQUIRE(remote_log_parse_line("", tag, sizeof(tag), NULL, 0) == false);
}

TEST_CASE("remote_log_parse_line_rejects_bad_level", "[remote_log]" ) {
  char tag[REMOTE_LOG_MAX_TAG_LEN];

  REQUIRE(remote_log_parse_line("x (12) TAG: message", tag, sizeof(tag), NULL, 0) == false);
  REQUIRE(remote_log_parse_line("info (12) TAG: message", tag, sizeof(tag), NULL, 0) == false);
}

TEST_CASE("remote_log_tag_allow_list_filters_lines", "[remote_log]" ) {
  remote_log_set_tags("APP_SENSOR");

  REQUIRE(remote_log_is_tag_allowed("APP_SENSOR") == true);
  REQUIRE(remote_log_is_tag_allowed("APP_THERMOSTAT") == false);

  char tag[REMOTE_LOG_MAX_TAG_LEN];
  bool parsed = remote_log_parse_line("E (3) APP_SENSORS: bad read",
                                      tag, sizeof(tag), NULL, 0);
  REQUIRE(parsed == true);
  REQUIRE(remote_log_is_tag_allowed(tag) == false);
}

TEST_CASE("remote_log_denied_tags_are_never_forwarded", "[remote_log]" ) {
  remote_log_set_tags("MQTTS_MQTTS,MQTT_CLIENT,APP_THERMOSTAT");

  char tags[REMOTE_LOG_CMD_MAX_LEN];
  remote_log_get_tags(tags, sizeof(tags));
  REQUIRE(strcmp(tags, "APP_THERMOSTAT") == 0);

  REQUIRE(remote_log_is_tag_allowed("MQTT_CLIENT") == false);
  REQUIRE(remote_log_is_tag_allowed("TRANSPORT_SSL") == false);
  REQUIRE(remote_log_is_tag_allowed("REMOTE_LOG") == false);
}

TEST_CASE("remote_log_set_and_get_tags_roundtrip", "[remote_log]" ) {
  remote_log_set_tags("APP_THERMOSTAT,SCHEDULER,MQTTS_RELAY");

  char tags[REMOTE_LOG_CMD_MAX_LEN];
  remote_log_get_tags(tags, sizeof(tags));
  REQUIRE(strcmp(tags, "APP_THERMOSTAT,SCHEDULER,MQTTS_RELAY") == 0);

  remote_log_set_tags("");
  remote_log_get_tags(tags, sizeof(tags));
  REQUIRE(strcmp(tags, "") == 0);
  REQUIRE(remote_log_is_tag_allowed("APP_THERMOSTAT") == false);
}

TEST_CASE("remote_log_max_tags_enforced", "[remote_log]" ) {
  remote_log_set_tags("T1,T2,T3,T4,T5,T6");

  char tags[REMOTE_LOG_CMD_MAX_LEN];
  remote_log_get_tags(tags, sizeof(tags));
  REQUIRE(strcmp(tags, "T1,T2,T3,T4,T5") == 0);
}

TEST_CASE("remote_log_too_long_tag_ignored", "[remote_log]" ) {
  remote_log_set_tags("TAGTHATISTOOLONGXX");

  char tags[REMOTE_LOG_CMD_MAX_LEN];
  remote_log_get_tags(tags, sizeof(tags));
  REQUIRE(strcmp(tags, "") == 0);
}

TEST_CASE("remote_log_build_topic_is_lowercased", "[remote_log]" ) {
  char topic[MAX_TOPIC_LEN];

  remote_log_build_topic(topic, sizeof(topic), "APP_THERMOSTAT");

  REQUIRE(strcmp(topic, "device_type/client_id/evt/log/app_thermostat") == 0);
}

TEST_CASE("remote_log_clean_line_strips_ansi_and_newline", "[remote_log]" ) {
  char line[] = "\x1b[0;32mI (6123) APP_THERMOSTAT: target 21.5\x1b[0m\n";

  remote_log_clean_line(line);

  REQUIRE(strcmp(line, "I (6123) APP_THERMOSTAT: target 21.5") == 0);
}

TEST_CASE("remote_log_level_from_char", "[remote_log]" ) {
  REQUIRE(remote_log_level_from_char('E') == ESP_LOG_ERROR);
  REQUIRE(remote_log_level_from_char('w') == ESP_LOG_WARN);
  REQUIRE(remote_log_level_from_char('I') == ESP_LOG_INFO);
  REQUIRE(remote_log_level_from_char('d') == ESP_LOG_DEBUG);
  REQUIRE(remote_log_level_from_char('V') == ESP_LOG_VERBOSE);
  REQUIRE(remote_log_level_from_char('X') == -1);
  REQUIRE(remote_log_level_from_char('=') == -1);
}

TEST_CASE("remote_log_set_level_applies_log_level", "[remote_log]" ) {
  remote_log_set_level("APP_THERMOSTAT=D");

  REQUIRE(strcmp(esp_log_last_level_tag, "APP_THERMOSTAT") == 0);
  REQUIRE(esp_log_last_level == ESP_LOG_DEBUG);

  remote_log_set_level("*=V");

  REQUIRE(strcmp(esp_log_last_level_tag, "*") == 0);
  REQUIRE(esp_log_last_level == ESP_LOG_VERBOSE);
}

TEST_CASE("remote_log_set_level_ignores_bad_payloads", "[remote_log]" ) {
  esp_log_last_level_tag = NULL;

  remote_log_set_level("APP_THERMOSTAT");
  remote_log_set_level("=D");
  remote_log_set_level("APP_THERMOSTAT=");
  remote_log_set_level("APP_THERMOSTAT=X");

  REQUIRE(esp_log_last_level_tag == NULL);
}

TEST_CASE("remote_log_vprintf_chains_to_original_vprintf", "[remote_log]" ) {
  memset(vprintf_captured, 0, sizeof(vprintf_captured));

  esp_log_set_vprintf(capture_vprintf);
  remote_log_init();

  int len = call_remote_log_vprintf("I (123) APP_THERMOSTAT: value %d", 42);

  REQUIRE(len > 0);
  REQUIRE(strcmp(vprintf_captured, "I (123) APP_THERMOSTAT: value 42") == 0);
}
