#include "esp_system.h"
#ifdef CONFIG_MQTT_BELL

#include "esp_log.h"
#include "mqtt_client.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#include "lwip/apps/sntp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app_main.h"
#include "app_bell.h"
#include "app_bell_engine.h"
#include "app_mqtt.h"
#include "app_nvs.h"
#include "app_publish_data.h"
#include "app_relay.h"
#include "cJSON.h"

static const char *TAG = "MQTTS_BELL";

#define BELL_TASK_STACK_SIZE 5120
#define BELL_TASK_PRIORITY 5
#define BELL_QUEUE_LEN 4
#define BELL_RING_COOLDOWN_S 10
#define BELL_SCRATCH_LEN 2048
#define BELL_TOPIC_MAX 96
#define BELL_MUTE_DEBOUNCE_MS 250

#define BELL_CMD_PREFIX CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/cmd/"
#define BELL_EVT_STATUS CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/status/bell/0"
#define BELL_EVT_CONFIG CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/config/bell/0"
#define BELL_EVT_EXC CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/exc/bell/0"
#define BELL_EVT_ALIAS CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/alias/bell/0"
#define BELL_EVT_RING CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/ring/bell/0"
#define BELL_EVT_ERROR CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/error/bell/0"

static const char * bell_sntp_servers[] = {
  "pool.ntp.org", "ro.pool.ntp.org", "time.google.com"
};
#define BELL_SNTP_SERVERS_NB (sizeof(bell_sntp_servers) / sizeof(bell_sntp_servers[0]))

enum BellMsgType {
  BELL_MSG_GET,
  BELL_MSG_RING,
  BELL_MSG_CONFIG,
  BELL_MSG_EXC,
  BELL_MSG_ALIAS,
  BELL_MSG_MUTE_TODAY,
};

struct BellMessage {
  unsigned char msgType;
  unsigned char pattern;
};

extern QueueHandle_t relayQueue;

static QueueHandle_t bellQueue = NULL;

static struct BellProgram live_prog;
static struct BellCalendar live_cal;
static struct BellAlias live_alias;
static struct BellAlias pend_alias;
static volatile bool pend_prog_valid = false;
static volatile bool pend_cal_valid = false;
static volatile bool pend_alias_valid = false;

/* parse happens in mqtt task context: builders are too large for its stack;
 * after a successful parse the builder content IS the pending document */
static struct BellProgramBuilder prog_builder;
static struct BellCalendarBuilder cal_builder;

#ifdef CONFIG_MQTT_BELL_MUTE_BUTTON
static TickType_t bell_mute_last_edge = 0;
#endif // CONFIG_MQTT_BELL_MUTE_BUTTON

/* static RX reassembly buffer: dispatch runs in mqtt task context (single threaded) */
static char bell_rx_buf[BELL_MAX_PAYLOAD + 1];
static int bell_rx_len = 0;

/* bell task only: blob io + json serialization scratch */
static unsigned char bell_scratch[BELL_SCRATCH_LEN];

static bool time_synced = false;
static time_t last_ring_time = 0;
static bool last_ring_valid = false;
static time_t cooldown_until = 0;

static const char *const day_keys[BELL_DAYS] = {
  "mon", "tue", "wed", "thu", "fri", "sat", "sun"
};

static bool check_time_synced(void)
{
  time_t now = time(NULL);
  struct tm lt;
  localtime_r(&now, &lt);
  return (lt.tm_year + 1900) >= 2020;
}

static void fmt_iso_min(time_t t, char *buf, size_t buf_len)
{
  struct tm lt;
  localtime_r(&t, &lt);
  strftime(buf, buf_len, "%Y-%m-%dT%H:%M", &lt);
}

static const char *bell_err_str(int err)
{
  switch (err) {
  case BELL_ERR_CAPS: return "caps";
  case BELL_ERR_TIME: return "time";
  case BELL_ERR_DUP_TIME: return "dup_time";
  case BELL_ERR_PATTERN_REF: return "pattern_ref";
  case BELL_ERR_DATE: return "date";
  case BELL_ERR_DATE_ORDER: return "date_order";
  case BELL_ERR_TYPE: return "type";
  case BELL_ERR_CORRUPT: return "corrupt";
  case BELL_ERR_JSON: return "json";
  default: return "unknown";
  }
}

static void publish_bell_error(const char *err, const char *detail)
{
  char data[160];
  snprintf(data, sizeof(data), "{\"err\":\"%s\",\"detail\":\"%s\",\"v\":%d}",
           err, detail, (int)live_prog.v);
  publish_non_persistent_data(BELL_EVT_ERROR, data);
}

static void publish_bell_status(void)
{
  char data[192];
  char date[11] = "";
  char next[17] = "";
  char last[17] = "";
  char next_json[22] = "null";
  char last_json[22] = "null";
  unsigned char next_pattern = 0;
  bool off_today = false;

  if (time_synced) {
    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    strftime(date, sizeof(date), "%Y-%m-%d", &lt);
    off_today = bell_is_day_off(&live_cal, &lt);

    struct tm next_day;
    unsigned short next_min;
    if (bell_next_event(&live_prog, &live_cal, &lt, &next_day, &next_min, &next_pattern)) {
      time_t t = mktime(&next_day);
      if (t > 0) {
        fmt_iso_min(t, next, sizeof(next));
        snprintf(next_json, sizeof(next_json), "\"%s\"", next);
      }
    }
  }
  if (last_ring_valid) {
    fmt_iso_min(last_ring_time, last, sizeof(last));
    snprintf(last_json, sizeof(last_json), "\"%s\"", last);
  }

  snprintf(data, sizeof(data),
           "{\"pv\":%d,\"xv\":%d,\"date\":\"%s\",\"mode\":\"%s\",\"next\":%s,"
           "\"pattern\":%d,\"last\":%s,\"sync\":\"%s\"}",
           (int)live_prog.v, (int)live_cal.v, date,
           off_today ? "off" : "normal", next_json, (int)next_pattern, last_json,
           time_synced ? "ok" : "stale");

  publish_persistent_data(BELL_EVT_STATUS, data);
}

static void send_relay(int on)
{
  struct RelayMessage rm;
  memset(&rm, 0, sizeof(rm));
  rm.msgType = RELAY_CMD_STATUS;
  rm.relayId = CONFIG_MQTT_BELL_RELAY_ID;
  rm.data = on ? RELAY_STATUS_ON : RELAY_STATUS_OFF;
  if (xQueueSend(relayQueue, &rm, 0) != pdPASS) {
    ESP_LOGE(TAG, "cannot send to relayQueue");
  }
}

static void ring_execute(unsigned char pattern, bool manual)
{
  if (pattern >= live_prog.pattern_count) {
    publish_bell_error("pattern_ref", "ring pattern out of range");
    return;
  }
  const struct BellPattern *pat = &live_prog.patterns[pattern];
  int total = bell_pattern_total_s(pat);

  if (!manual && !time_synced) {
    ESP_LOGW(TAG, "scheduled ring suppressed, time stale");
    return;
  }

  time_t now = time(NULL);
  if (now < cooldown_until) {
    if (manual)
      publish_bell_error("cooldown", "ring rejected within cooldown");
    else
      ESP_LOGW(TAG, "scheduled ring skipped, cooldown active");
    return;
  }
  cooldown_until = now + total + BELL_RING_COOLDOWN_S;

  char data[64];
  char iso[20];
  fmt_iso_min(now, iso, sizeof(iso));
  snprintf(data, sizeof(data), "{\"p\":%d,\"t\":\"%s\",\"manual\":%d}",
           (int)pattern, time_synced ? iso : "", manual ? 1 : 0);
  publish_non_persistent_data(BELL_EVT_RING, data);

  for (int i = 0; i < (int)pat->count; i++) {
    send_relay(1);
    vTaskDelay((pat->dur_s * 1000) / portTICK_PERIOD_MS);
    send_relay(0);
    if (i + 1 < (int)pat->count)
      vTaskDelay((pat->gap_s * 1000) / portTICK_PERIOD_MS);
  }

  last_ring_time = now;
  last_ring_valid = true;
  publish_bell_status();
}

static bool parse_hhmm(const char *s, int *min_of_day)
{
  int h = -1, m = -1;
  if (sscanf(s, "%d:%d", &h, &m) != 2)
    return false;
  *min_of_day = h * 60 + m;
  return true;
}

static bool parse_ymd(const char *s, int *y, int *mo, int *d)
{
  if (sscanf(s, "%d-%d-%d", y, mo, d) != 3)
    return false;
  return true;
}

static int parse_program_json(const char *json)
{
  cJSON *root = cJSON_Parse(json);
  if (!root)
    return BELL_ERR_JSON;

  cJSON *v = cJSON_GetObjectItem(root, "v");
  cJSON *enabled = cJSON_GetObjectItem(root, "enabled");
  if (!cJSON_IsNumber(v) || !cJSON_IsBool(enabled)) {
    cJSON_Delete(root);
    return BELL_ERR_JSON;
  }
  bell_program_builder_init(&prog_builder, (int)v->valueint,
                            cJSON_IsTrue(enabled));

  cJSON *patterns = cJSON_GetObjectItem(root, "patterns");
  if (!cJSON_IsArray(patterns)) {
    cJSON_Delete(root);
    return BELL_ERR_JSON;
  }
  int nb = cJSON_GetArraySize(patterns);
  for (int i = 0; i < nb; i++) {
    cJSON *pat = cJSON_GetArrayItem(patterns, i);
    cJSON *count = cJSON_GetObjectItem(pat, "count");
    cJSON *dur = cJSON_GetObjectItem(pat, "dur_s");
    cJSON *gap = cJSON_GetObjectItem(pat, "gap_s");
    cJSON *pen = cJSON_GetObjectItem(pat, "enabled");
    if (!cJSON_IsNumber(count) || !cJSON_IsNumber(dur) || !cJSON_IsNumber(gap) ||
        !cJSON_IsBool(pen)) {
      cJSON_Delete(root);
      return BELL_ERR_JSON;
    }
    int err = bell_builder_add_pattern(&prog_builder, (int)count->valueint,
                                       (int)dur->valueint, (int)gap->valueint,
                                       cJSON_IsTrue(pen));
    if (err != BELL_OK) {
      cJSON_Delete(root);
      return err;
    }
  }

  cJSON *weekly = cJSON_GetObjectItem(root, "weekly");
  if (!cJSON_IsObject(weekly)) {
    cJSON_Delete(root);
    return BELL_ERR_JSON;
  }
  for (int d = 0; d < BELL_DAYS; d++) {
    cJSON *day = cJSON_GetObjectItem(weekly, day_keys[d]);
    if (!day)
      continue;
    if (!cJSON_IsArray(day)) {
      cJSON_Delete(root);
      return BELL_ERR_JSON;
    }
    int n = cJSON_GetArraySize(day);
    for (int i = 0; i < n; i++) {
      cJSON *ev = cJSON_GetArrayItem(day, i);
      cJSON *t = cJSON_GetObjectItem(ev, "t");
      cJSON *p = cJSON_GetObjectItem(ev, "p");
      int minute = -1;
      if (!cJSON_IsString(t) || !cJSON_IsNumber(p) || !parse_hhmm(t->valuestring, &minute)) {
        cJSON_Delete(root);
        return BELL_ERR_JSON;
      }
      int err = bell_builder_add_event(&prog_builder, d, minute, (int)p->valueint);
      if (err != BELL_OK) {
        cJSON_Delete(root);
        return err;
      }
    }
  }

  int err = bell_program_builder_finish(&prog_builder);
  cJSON_Delete(root);
  return err;
}

static int parse_alias_json(const char *json)
{
  memset(&pend_alias, 0, sizeof(pend_alias));

  cJSON *root = cJSON_Parse(json);
  if (!root)
    return BELL_ERR_JSON;

  cJSON *bell = cJSON_GetObjectItem(root, "bell");
  if (bell) {
    if (!cJSON_IsString(bell) || strlen(bell->valuestring) > BELL_MAX_ALIAS_NAME) {
      cJSON_Delete(root);
      return BELL_ERR_CAPS;
    }
    strcpy(pend_alias.bell, bell->valuestring);
  }

  cJSON *patterns = cJSON_GetObjectItem(root, "patterns");
  if (patterns) {
    if (!cJSON_IsArray(patterns)) {
      cJSON_Delete(root);
      return BELL_ERR_JSON;
    }
    int nb = cJSON_GetArraySize(patterns);
    if (nb > BELL_MAX_PATTERNS) {
      cJSON_Delete(root);
      return BELL_ERR_CAPS;
    }
    for (int i = 0; i < nb; i++) {
      cJSON *name = cJSON_GetArrayItem(patterns, i);
      if (!cJSON_IsString(name) || strlen(name->valuestring) > BELL_MAX_ALIAS_NAME) {
        cJSON_Delete(root);
        return BELL_ERR_CAPS;
      }
      strcpy(pend_alias.patterns[i], name->valuestring);
    }
    pend_alias.pattern_count = (unsigned char)nb;
  }

  cJSON_Delete(root);
  return bell_validate_alias(&pend_alias);
}

static int parse_calendar_json(const char *json)
{
  cJSON *root = cJSON_Parse(json);
  if (!root)
    return BELL_ERR_JSON;

  cJSON *v = cJSON_GetObjectItem(root, "v");
  if (!cJSON_IsNumber(v)) {
    cJSON_Delete(root);
    return BELL_ERR_JSON;
  }
  bell_calendar_builder_init(&cal_builder, (int)v->valueint);

  cJSON *exceptions = cJSON_GetObjectItem(root, "exceptions");
  if (!cJSON_IsArray(exceptions)) {
    cJSON_Delete(root);
    return BELL_ERR_JSON;
  }
  int nb = cJSON_GetArraySize(exceptions);
  for (int i = 0; i < nb; i++) {
    cJSON *exc = cJSON_GetArrayItem(exceptions, i);
    cJSON *from = cJSON_GetObjectItem(exc, "from");
    cJSON *to = cJSON_GetObjectItem(exc, "to");
    int fy, fmo, fd, ty, tmo, td;
    if (!cJSON_IsString(from) || !cJSON_IsString(to) ||
        !parse_ymd(from->valuestring, &fy, &fmo, &fd) ||
        !parse_ymd(to->valuestring, &ty, &tmo, &td)) {
      cJSON_Delete(root);
      return BELL_ERR_JSON;
    }
    int type = 0;
    cJSON *type_item = cJSON_GetObjectItem(exc, "type");
    if (type_item) {
      if (cJSON_IsString(type_item) && strcmp(type_item->valuestring, "off") == 0) {
        type = 0;
      } else {
        cJSON_Delete(root);
        return BELL_ERR_TYPE;
      }
    }
    int err = bell_builder_add_exception(&cal_builder, fy, fmo, fd, ty, tmo, td, type);
    if (err != BELL_OK) {
      cJSON_Delete(root);
      return err;
    }
  }

  int err = bell_calendar_builder_finish(&cal_builder);
  cJSON_Delete(root);
  return err;
}

static void load_bell_nvs(void)
{
  size_t len = BELL_SCRATCH_LEN;
  esp_err_t err = read_nvs_blob("bellprog", bell_scratch, &len);
  if (err == ESP_OK && len == bell_program_blob_size() &&
      bell_decode_program(bell_scratch, len, &live_prog)) {
    ESP_LOGI(TAG, "program loaded from nvs (v=%d)", (int)live_prog.v);
  } else {
    memset(&live_prog, 0, sizeof(live_prog));
    publish_bell_error("corrupt", "program blob, defaults loaded");
  }

  len = BELL_SCRATCH_LEN;
  err = read_nvs_blob("bellexc", bell_scratch, &len);
  if (err == ESP_OK && len == bell_calendar_blob_size() &&
      bell_decode_calendar(bell_scratch, len, &live_cal)) {
    ESP_LOGI(TAG, "calendar loaded from nvs (v=%d)", (int)live_cal.v);
  } else {
    memset(&live_cal, 0, sizeof(live_cal));
    publish_bell_error("corrupt", "calendar blob, defaults loaded");
  }

  len = BELL_SCRATCH_LEN;
  err = read_nvs_blob("bellal", bell_scratch, &len);
  if (err == ESP_OK && len == bell_alias_blob_size() &&
      bell_decode_alias(bell_scratch, len, &live_alias)) {
    ESP_LOGI(TAG, "alias loaded from nvs (bell=%s)", live_alias.bell);
  } else {
    memset(&live_alias, 0, sizeof(live_alias));
  }
}

static void save_bell_nvs(void)
{
  size_t written = bell_encode_program(&live_prog, bell_scratch, BELL_SCRATCH_LEN);
  if (written == bell_program_blob_size())
    ESP_ERROR_CHECK(write_nvs_blob("bellprog", bell_scratch, written));

  written = bell_encode_calendar(&live_cal, bell_scratch, BELL_SCRATCH_LEN);
  if (written == bell_calendar_blob_size())
    ESP_ERROR_CHECK(write_nvs_blob("bellexc", bell_scratch, written));

  written = bell_encode_alias(&live_alias, bell_scratch, BELL_SCRATCH_LEN);
  if (written == bell_alias_blob_size())
    ESP_ERROR_CHECK(write_nvs_blob("bellal", bell_scratch, written));
}

static void publish_bell_documents(void)
{
  size_t n = bell_program_to_json(&live_prog, (char *)bell_scratch, BELL_SCRATCH_LEN);
  if (n > 0)
    publish_persistent_data(BELL_EVT_CONFIG, (char *)bell_scratch);
  else
    publish_bell_error("caps", "program json too large");

  n = bell_calendar_to_json(&live_cal, (char *)bell_scratch, BELL_SCRATCH_LEN);
  if (n > 0)
    publish_persistent_data(BELL_EVT_EXC, (char *)bell_scratch);
  else
    publish_bell_error("caps", "calendar json too large");

  n = bell_alias_to_json(&live_alias, (char *)bell_scratch, BELL_SCRATCH_LEN);
  if (n > 0)
    publish_persistent_data(BELL_EVT_ALIAS, (char *)bell_scratch);
  else
    publish_bell_error("caps", "alias json too large");
}

#ifdef CONFIG_MQTT_BELL_MUTE_BUTTON
static void bell_mute_today(void)
{
  TickType_t now = xTaskGetTickCount();
  if (bell_mute_last_edge != 0 && (now - bell_mute_last_edge) < pdMS_TO_TICKS(BELL_MUTE_DEBOUNCE_MS))
    return;
  bell_mute_last_edge = now;

  if (!time_synced) {
    publish_bell_error("time", "mute button ignored, clock not synced");
    return;
  }

  time_t t = time(NULL);
  struct tm lt;
  localtime_r(&t, &lt);
  int y = lt.tm_year + 1900, mo = lt.tm_mon + 1, d = lt.tm_mday;

  if (bell_is_day_off(&live_cal, &lt)) {
    ESP_LOGI(TAG, "mute button: %04d-%02d-%02d already off", y, mo, d);
    return;
  }

  bell_calendar_builder_init(&cal_builder, live_cal.v + 1);
  for (int i = 0; i < live_cal.exception_count; i++) {
    const struct BellException *e = &live_cal.exceptions[i];
    bell_builder_add_exception(&cal_builder,
                               e->from.year, e->from.month, e->from.day,
                               e->to.year, e->to.month, e->to.day, e->type);
  }
  int err = bell_builder_add_exception(&cal_builder, y, mo, d, y, mo, d, 0);
  if (err != BELL_OK) {
    publish_bell_error(bell_err_str(err), "mute button rejected, last-good kept");
    return;
  }

  live_cal = cal_builder.cal;
  save_bell_nvs();
  publish_bell_documents();
  publish_bell_status();
  ESP_LOGI(TAG, "mute button: added day-off exception %04d-%02d-%02d (xv=%d)",
           y, mo, d, (int)live_cal.v);
}
#endif // CONFIG_MQTT_BELL_MUTE_BUTTON

static void handle_bell_msg(struct BellMessage *msg)
{
  switch (msg->msgType) {
  case BELL_MSG_GET:
    publish_bell_documents();
    publish_bell_status();
    break;

  case BELL_MSG_RING:
    ring_execute(msg->pattern, true);
    break;

  case BELL_MSG_CONFIG:
    if (!pend_prog_valid)
      break;
    if (live_prog.v != 0 && prog_builder.prog.v < live_prog.v)
      ESP_LOGW(TAG, "program version not monotonic: %d -> %d",
               (int)live_prog.v, (int)prog_builder.prog.v);
    live_prog = prog_builder.prog;
    pend_prog_valid = false;
    save_bell_nvs();
    publish_bell_documents();
    publish_bell_status();
    ESP_LOGI(TAG, "program updated (v=%d)", (int)live_prog.v);
    break;

  case BELL_MSG_EXC:
    if (!pend_cal_valid)
      break;
    if (live_cal.v != 0 && cal_builder.cal.v < live_cal.v)
      ESP_LOGW(TAG, "calendar version not monotonic: %d -> %d",
               (int)live_cal.v, (int)cal_builder.cal.v);
    live_cal = cal_builder.cal;
    pend_cal_valid = false;
    save_bell_nvs();
    publish_bell_documents();
    publish_bell_status();
    ESP_LOGI(TAG, "calendar updated (v=%d)", (int)live_cal.v);
    break;

  case BELL_MSG_ALIAS:
    if (!pend_alias_valid)
      break;
    live_alias = pend_alias;
    pend_alias_valid = false;
    save_bell_nvs();
    publish_bell_documents();
    ESP_LOGI(TAG, "alias updated (bell=%s)", live_alias.bell);
    break;

#ifdef CONFIG_MQTT_BELL_MUTE_BUTTON
  case BELL_MSG_MUTE_TODAY:
    bell_mute_today();
    break;
#endif // CONFIG_MQTT_BELL_MUTE_BUTTON

  default:
    ESP_LOGW(TAG, "unhandled bell msg type %d", msg->msgType);
  }
}

static void handle_bell(void* pvParameters)
{
  ESP_LOGI(TAG, "bell task started");
  load_bell_nvs();

  for (unsigned int i = 0; i < BELL_SNTP_SERVERS_NB; i++)
    sntp_setservername(i, bell_sntp_servers[i]);
  sntp_init();

  time_synced = check_time_synced();
  publish_bell_status();

  struct BellMessage msg;
  bool sync_published = time_synced;
  while (1) {
    time_t now = time(NULL);
    time_synced = check_time_synced();
    if (time_synced != sync_published) {
      publish_bell_status();
      sync_published = time_synced;
    }

    time_t t_next = 0;
    unsigned char next_pattern = 0;
    if (time_synced) {
      struct tm lt;
      localtime_r(&now, &lt);
      struct tm next_day;
      unsigned short next_min;
      if (bell_next_event(&live_prog, &live_cal, &lt, &next_day, &next_min, &next_pattern))
        t_next = mktime(&next_day);
    }

    int wait_ms = 10000;
    if (t_next > 0) {
      long remaining = (long)(t_next - now) * 1000;
      if (remaining < 0)
        remaining = 0;
      if (remaining < wait_ms)
        wait_ms = (int)remaining;
    }

    if (xQueueReceive(bellQueue, &msg, pdMS_TO_TICKS(wait_ms))) {
      handle_bell_msg(&msg);
    } else if (t_next > 0 && time(NULL) >= t_next) {
      ring_execute(next_pattern, false);
    }
  }
}

#ifdef CONFIG_MQTT_BELL_MUTE_BUTTON
static void bell_mute_isr_handler(void *arg)
{
  struct BellMessage msg;
  msg.msgType = BELL_MSG_MUTE_TODAY;
  msg.pattern = 0;
  xQueueSendFromISR(bellQueue, &msg, NULL);
}
#endif // CONFIG_MQTT_BELL_MUTE_BUTTON

void bell_init(void)
{
  setenv("TZ", "EET-2EEST,M3.5.0,M10.5.0", 1);
  tzset();

  bellQueue = xQueueCreate(BELL_QUEUE_LEN, sizeof(struct BellMessage));
  if (bellQueue == NULL) {
    ESP_LOGE(TAG, "cannot create bellQueue");
    return;
  }

#ifdef CONFIG_MQTT_BELL_MUTE_BUTTON
  gpio_config_t mute_io = {
    .pin_bit_mask = 1ULL << CONFIG_MQTT_BELL_MUTE_BUTTON_GPIO,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_ANYEDGE,
  };
  ESP_ERROR_CHECK(gpio_config(&mute_io));
  esp_err_t isr_err = gpio_install_isr_service(0);
  if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "cannot install gpio isr service (%s)", esp_err_to_name(isr_err));
  } else {
    ESP_ERROR_CHECK(gpio_isr_handler_add(CONFIG_MQTT_BELL_MUTE_BUTTON_GPIO,
                                         bell_mute_isr_handler, NULL));
    ESP_LOGI(TAG, "mute button armed on GPIO %d", CONFIG_MQTT_BELL_MUTE_BUTTON_GPIO);
  }
#endif // CONFIG_MQTT_BELL_MUTE_BUTTON

  if (xTaskCreate(handle_bell, "bell_task", BELL_TASK_STACK_SIZE, NULL,
                  BELL_TASK_PRIORITY, NULL) != pdPASS) {
    ESP_LOGE(TAG, "cannot create bell task");
    return;
  }
  ESP_LOGI(TAG, "bell module initialized (queue + task ready)");
}

void bell_on_mqtt_connected(void)
{
  /* republish documents + retained status on every (re)connect */
  struct BellMessage msg;
  memset(&msg, 0, sizeof(msg));
  msg.msgType = BELL_MSG_GET;
  if (xQueueSend(bellQueue, &msg, 0) != pdPASS)
    ESP_LOGW(TAG, "cannot send to bellQueue");
}

static bool bell_route(esp_mqtt_event_handle_t event, char *action, size_t action_len)
{
  static const char prefix[] = BELL_CMD_PREFIX;

  if (event->topic_len < (int)strlen(prefix) ||
      strncmp(event->topic, prefix, strlen(prefix)) != 0)
    return false;

  char topic[BELL_TOPIC_MAX];
  int len = event->topic_len < (int)sizeof(topic) - 1 ? event->topic_len : (int)sizeof(topic) - 1;
  memcpy(topic, event->topic, len);
  topic[len] = 0;

  char *rest = topic + strlen(prefix);
  char *act = strtok(rest, "/");
  char *service = strtok(NULL, "/");
  char *id = strtok(NULL, "/");
  if (!act || !service || strcmp(service, "bell") != 0)
    return false;
  if (!id || atoi(id) != 0) {
    ESP_LOGW(TAG, "unhandled bell id");
    return true;
  }
  snprintf(action, action_len, "%s", act);
  return true;
}

bool bell_handle_mqtt_event(esp_mqtt_event_handle_t event)
{
  /* bell route: runs FIRST in dispatch_mqtt_event, before the 16-byte cap */

  char action[16];
  if (!bell_route(event, action, sizeof(action)))
    return false;

  if (event->total_data_len > BELL_MAX_PAYLOAD) {
    publish_bell_error("caps", "payload too big");
    return true;
  }

  if (event->current_data_offset == 0)
    bell_rx_len = 0;
  if (bell_rx_len != event->current_data_offset) {
    bell_rx_len = 0;
    publish_bell_error("json", "fragmented payload out of order");
    return true;
  }
  memcpy(bell_rx_buf + bell_rx_len, event->data, event->data_len);
  bell_rx_len += event->data_len;
  if (bell_rx_len < event->total_data_len)
    return true;
  bell_rx_buf[bell_rx_len] = 0;

  struct BellMessage msg;
  memset(&msg, 0, sizeof(msg));

  if (strcmp(action, "get") == 0) {
    if (strcmp(bell_rx_buf, "1") != 0) {
      ESP_LOGW(TAG, "unhandled bell get payload: %s", bell_rx_buf);
      return true;
    }
    msg.msgType = BELL_MSG_GET;
  } else if (strcmp(action, "ring") == 0) {
    cJSON *root = cJSON_Parse(bell_rx_buf);
    if (!root) {
      publish_bell_error("json", "ring payload malformed");
      return true;
    }
    cJSON *p = cJSON_GetObjectItem(root, "p");
    if (p && cJSON_IsNumber(p) && p->valueint >= 0 && p->valueint < BELL_MAX_PATTERNS)
      msg.pattern = (unsigned char)p->valueint;
    cJSON_Delete(root);
    msg.msgType = BELL_MSG_RING;
  } else if (strcmp(action, "config") == 0) {
    int err = parse_program_json(bell_rx_buf);
    if (err != BELL_OK) {
      publish_bell_error(bell_err_str(err), "program rejected, last-good kept");
      return true;
    }
    pend_prog_valid = true;
    msg.msgType = BELL_MSG_CONFIG;
  } else if (strcmp(action, "exc") == 0) {
    int err = parse_calendar_json(bell_rx_buf);
    if (err != BELL_OK) {
      publish_bell_error(bell_err_str(err), "calendar rejected, last-good kept");
      return true;
    }
    pend_cal_valid = true;
    msg.msgType = BELL_MSG_EXC;
  } else if (strcmp(action, "alias") == 0) {
    int err = parse_alias_json(bell_rx_buf);
    if (err != BELL_OK) {
      publish_bell_error(bell_err_str(err), "alias rejected, last-good kept");
      return true;
    }
    pend_alias_valid = true;
    msg.msgType = BELL_MSG_ALIAS;
  } else {
    ESP_LOGW(TAG, "unhandled bell action: %s", action);
    return true;
  }

  if (xQueueSend(bellQueue, &msg, 0) != pdPASS) {
    ESP_LOGE(TAG, "cannot send to bellQueue");
    publish_bell_error("caps", "bell queue full");
  }
  return true;
}

#endif // CONFIG_MQTT_BELL
