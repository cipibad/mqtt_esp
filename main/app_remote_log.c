#include "esp_system.h"
#ifdef CONFIG_MQTT_REMOTE_LOG

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "app_publish_data.h"
#include "app_nvs.h"
#include "app_remote_log.h"

static const char *TAG = "REMOTE_LOG";

//NVS keys are limited to 15 characters
#define REMOTE_LOG_NVS_KEY "remote_log_tags"
#define REMOTE_LOG_QUEUE_DEPTH 8
//the drain task calls the mqtt publish chain which logs(published
//qos0 data) from the same stack, and every ESP_LOG line pays the
//vprintf hook cost on top, 4608 bytes overflowed during thermostat
//tick bursts, this is sized to hold the full chain with headroom
#define REMOTE_LOG_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 6)
#define REMOTE_LOG_TASK_PRIORITY 5
//pace forwarded lines so that a log burst cannot monopolize the mqtt mutex
#define REMOTE_LOG_PUBLISH_DELAY_MS 10

struct RemoteLogMessage {
  char line[REMOTE_LOG_LINE_LEN];
};

static QueueHandle_t remoteLogQueue = NULL;
static vprintf_like_t remoteLogOriginalVprintf = NULL;
static unsigned int remoteLogDropped = 0;

//the allow list is written only from the mqtt event task on config commands
//and read from any task in the vprintf hook, entries are small and writes
//are rare so plain access is accepted
static char remoteLogAllowList[REMOTE_LOG_MAX_TAGS][REMOTE_LOG_MAX_TAG_LEN];
static unsigned char remoteLogAllowNb = 0;

//tags that are never forwarded even if explicitly allowed: esp-mqtt logging
//its own activity about publishing a log line would feed back into the
//forwarded stream, same for this module's own logs
static const char * REMOTE_LOG_DENIED_TAGS[] = {
  "MQTT_CLIENT",
  "TRANSPORT_TCP",
  "TRANSPORT_SSL",
  "TRANSPORT",
  "OUTBOX",
  "MQTTS_MQTTS",
  "REMOTE_LOG",
  NULL
};

static bool is_denied_tag(const char *tag)
{
  for (int i = 0; REMOTE_LOG_DENIED_TAGS[i] != NULL; i++) {
    if (strcmp(tag, REMOTE_LOG_DENIED_TAGS[i]) == 0) {
      return true;
    }
  }
  return false;
}

bool remote_log_is_tag_allowed(const char *tag)
{
  if (is_denied_tag(tag)) {
    return false;
  }
  for (unsigned char i = 0; i < remoteLogAllowNb; i++) {
    if (strcmp(tag, remoteLogAllowList[i]) == 0) {
      return true;
    }
  }
  return false;
}

static void apply_tags(const char *tagsCsv)
{
  char buffer[REMOTE_LOG_CMD_MAX_LEN];
  memset(buffer, 0, sizeof(buffer));
  strncpy(buffer, tagsCsv, sizeof(buffer) - 1);

  remoteLogAllowNb = 0;
  char *token = strtok(buffer, ",");
  while (token != NULL && remoteLogAllowNb < REMOTE_LOG_MAX_TAGS) {
    if (strlen(token) >= REMOTE_LOG_MAX_TAG_LEN) {
      ESP_LOGW(TAG, "tag too long, ignored: %s", token);
    } else if (is_denied_tag(token)) {
      ESP_LOGW(TAG, "tag not allowed to be forwarded: %s", token);
    } else {
      strcpy(remoteLogAllowList[remoteLogAllowNb], token);
      remoteLogAllowNb++;
    }
    token = strtok(NULL, ",");
  }
}

void remote_log_set_tags(const char *tagsCsv)
{
  apply_tags(tagsCsv);

  char saved[REMOTE_LOG_CMD_MAX_LEN];
  remote_log_get_tags(saved, sizeof(saved));
  write_nvs_str(REMOTE_LOG_NVS_KEY, saved);
  ESP_LOGI(TAG, "forwarding tags: %s", saved);
}

void remote_log_get_tags(char *buffer, size_t len)
{
  memset(buffer, 0, len);
  for (unsigned char i = 0; i < remoteLogAllowNb; i++) {
    if (i > 0) {
      strncat(buffer, ",", len - strlen(buffer) - 1);
    }
    strncat(buffer, remoteLogAllowList[i], len - strlen(buffer) - 1);
  }
}

int remote_log_level_from_char(char level)
{
  switch (toupper(level)) {
  case 'E':
    return ESP_LOG_ERROR;
  case 'W':
    return ESP_LOG_WARN;
  case 'I':
    return ESP_LOG_INFO;
  case 'D':
    return ESP_LOG_DEBUG;
  case 'V':
    return ESP_LOG_VERBOSE;
  default:
    return -1;
  }
}

void remote_log_set_level(const char *payload)
{
  char buffer[REMOTE_LOG_CMD_MAX_LEN];
  memset(buffer, 0, sizeof(buffer));
  strncpy(buffer, payload, sizeof(buffer) - 1);

  char *separator = strchr(buffer, '=');
  if (separator == NULL || separator == buffer || separator[1] == '\0') {
    ESP_LOGE(TAG, "wrong level payload, expected TAG=LEVEL");
    return;
  }
  *separator = '\0';

  int level = remote_log_level_from_char(separator[1]);
  if (level < 0) {
    ESP_LOGE(TAG, "unknown level %c", separator[1]);
    return;
  }
  esp_log_level_set(buffer, (esp_log_level_t)level);
  ESP_LOGI(TAG, "log level for %s set to %c", buffer, toupper(separator[1]));
}

bool remote_log_parse_line(const char *line, char *tag, size_t tagLen,
                           char *payload, size_t payloadLen)
{
  const char *p = line;

  //skip ansi color escape codes
  while (*p == '\x1b') {
    while (*p != '\0' && *p != 'm') {
      p++;
    }
    if (*p == 'm') {
      p++;
    }
  }

  //level character
  if (*p == '\0' || strchr("EWIDV", *p) == NULL) {
    return false;
  }
  p++;

  if (strncmp(p, " (", 2) != 0) {
    return false;
  }
  p += 2;
  while (*p >= '0' && *p <= '9') {
    p++;
  }
  if (strncmp(p, ") ", 2) != 0) {
    return false;
  }
  p += 2;

  //tag until ':'
  const char *tagStart = p;
  const char *separator = strchr(p, ':');
  if (separator == NULL) {
    return false;
  }
  size_t tagSize = separator - tagStart;
  if (tagSize == 0 || tagSize >= tagLen) {
    return false;
  }
  memcpy(tag, tagStart, tagSize);
  tag[tagSize] = '\0';

  if (separator[1] == ' ') {
    p = separator + 2;
  } else {
    p = separator + 1;
  }

  if (payload != NULL && payloadLen > 0) {
    strncpy(payload, p, payloadLen - 1);
    payload[payloadLen - 1] = '\0';
    size_t payloadSize = strlen(payload);
    if (payloadSize > 0 && payload[payloadSize - 1] == '\n') {
      payload[payloadSize - 1] = '\0';
    }
  }
  return true;
}

void remote_log_build_topic(char *topic, size_t len, const char *tag)
{
  char loweredTag[REMOTE_LOG_MAX_TAG_LEN];
  memset(loweredTag, 0, sizeof(loweredTag));
  for (size_t i = 0; i < sizeof(loweredTag) - 1 && tag[i] != '\0'; i++) {
    loweredTag[i] = tolower(tag[i]);
  }

  snprintf(topic, len, CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/log/%s", loweredTag);
}

void remote_log_clean_line(char *line)
{
  int read = 0;
  int write = 0;
  while (line[read] != '\0') {
    if (line[read] == '\x1b') {
      while (line[read] != '\0' && line[read] != 'm') {
        read++;
      }
      if (line[read] == 'm') {
        read++;
      }
    } else {
      line[write] = line[read];
      write++;
      read++;
    }
  }
  line[write] = '\0';

  while (write > 0 && (line[write - 1] == '\n' || line[write - 1] == '\r')) {
    write--;
    line[write] = '\0';
  }
}

int remote_log_vprintf(const char *fmt, va_list l)
{
  int len = 0;
  //the hook runs in the stack context of whoever logs, including the
  //timer service task, so it must stay as cheap as possible: with no
  //allow list configured it costs one compare only
  if (remoteLogQueue != NULL && remoteLogAllowNb > 0) {
    struct RemoteLogMessage msg;
    memset(&msg, 0, sizeof(msg));

    va_list copy;
    va_copy(copy, l);
    len = vsnprintf(msg.line, sizeof(msg.line), fmt, copy);
    va_end(copy);

    char tag[REMOTE_LOG_MAX_TAG_LEN];
    if (len > 0 && remote_log_parse_line(msg.line, tag, sizeof(tag), NULL, 0)
        && remote_log_is_tag_allowed(tag)) {
      remote_log_clean_line(msg.line);
      if (xQueueSend(remoteLogQueue, &msg, 0) != pdPASS) {
        remoteLogDropped++;
      }
    }
  }

  //keep the local console output intact
  if (remoteLogOriginalVprintf != NULL) {
    return remoteLogOriginalVprintf(fmt, l);
  }
  return len;
}

static void remote_log_task(void* pvParameters)
{
  struct RemoteLogMessage msg;
  char tag[REMOTE_LOG_MAX_TAG_LEN];
  char topic[MAX_TOPIC_LEN];

  while(1) {
    if (xQueueReceive(remoteLogQueue, &msg, portMAX_DELAY) == pdTRUE) {
      if (remote_log_parse_line(msg.line, tag, sizeof(tag), NULL, 0)) {
        remote_log_build_topic(topic, sizeof(topic), tag);
        publish_non_persistent_data(topic, msg.line);
        vTaskDelay(REMOTE_LOG_PUBLISH_DELAY_MS / portTICK_PERIOD_MS);
      }
      if (remoteLogDropped > 0) {
        ESP_LOGW(TAG, "%d remote log lines dropped", remoteLogDropped);
        remoteLogDropped = 0;
      }
    }
  }
}

void remote_log_init(void)
{
  char saved[REMOTE_LOG_CMD_MAX_LEN];

  remoteLogQueue = xQueueCreate(REMOTE_LOG_QUEUE_DEPTH, sizeof(struct RemoteLogMessage));
  if (remoteLogQueue == NULL) {
    ESP_LOGE(TAG, "cannot create remote log queue");
    return;
  }

  //read_nvs_str passes the size in and out, without the buffer size
  //nvs_get_str fails with ESP_ERR_NVS_INVALID_LENGTH
  size_t savedLen = sizeof(saved);
  memset(saved, 0, sizeof(saved));
  if (read_nvs_str(REMOTE_LOG_NVS_KEY, saved, &savedLen) == ESP_OK) {
    apply_tags(saved);
  }

  xTaskCreate(remote_log_task, "remote_log", REMOTE_LOG_TASK_STACK_SIZE, NULL,
              REMOTE_LOG_TASK_PRIORITY, NULL);
  remoteLogOriginalVprintf = esp_log_set_vprintf(remote_log_vprintf);

  char tags[REMOTE_LOG_CMD_MAX_LEN];
  remote_log_get_tags(tags, sizeof(tags));
  ESP_LOGI(TAG, "remote logging started, tags: %s", tags);
}

#endif // CONFIG_MQTT_REMOTE_LOG
