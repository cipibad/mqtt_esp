#ifndef APP_REMOTE_LOG_H
#define APP_REMOTE_LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

//max number of TAGs in the allow list
#define REMOTE_LOG_MAX_TAGS 5
//max length of a TAG including '\0', longest tag in the code is MQTTS_SMARTCONFIG
#define REMOTE_LOG_MAX_TAG_LEN 18
//max length of a forwarded log line, longer lines are truncated
#define REMOTE_LOG_LINE_LEN 128
//max length of the cmd/tags/log and cmd/level/log payloads including '\0'
#define REMOTE_LOG_CMD_MAX_LEN 96

//parse one formatted ESP_LOG line "L (uptime) TAG: message", ansi color
//escape prefixes are tolerated, fills tag and optional payload
bool remote_log_parse_line(const char *line, char *tag, size_t tagLen,
                           char *payload, size_t payloadLen);

//true if a TAG is in the allow list and is not in the internal deny list
bool remote_log_is_tag_allowed(const char *tag);

//set the allow list from a comma separated TAG list, empty string clears it,
//list is persisted in NVS
void remote_log_set_tags(const char *tagsCsv);

//serialize the current allow list into a comma separated TAG list
void remote_log_get_tags(char *buffer, size_t len);

//map a level character(both cases) E W I D V to the esp_log_level_t value,
//returns -1 for unknown levels
int remote_log_level_from_char(char level);

//set the runtime log level of one TAG, payload format is "TAG=LEVEL"
//where LEVEL is one of E W I D V, TAG can also be "*"
void remote_log_set_level(const char *payload);

//build the topic a TAG is forwarded to:
//CONFIG_DEVICE_TYPE/CONFIG_CLIENT_ID/evt/log/<lowercase tag>
void remote_log_build_topic(char *topic, size_t len, const char *tag);

//remove ansi color escape sequences and trailing newlines from a
//formatted log line, in place
void remote_log_clean_line(char *line);

//the vprintf hook installed with esp_log_set_vprintf, exposed for tests
int remote_log_vprintf(const char *fmt, va_list l);

//boot initialization: load persisted tags, create queue and drain task,
//install the vprintf hook, call it after nvs_flash_init
void remote_log_init(void);

#endif /* APP_REMOTE_LOG_H */
