#include "sdkconfig.h"

#ifdef CONFIG_WIFI_PROVISIONING_PORTAL

#include <string.h>
#include <stdlib.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "app_main.h"
#include "app_wifi_portal.h"
#include "app_nvs.h"

static const char *TAG = "WIFI_PORTAL";

extern const char portal_html_start[] asm("_binary_portal_html_start");

#define PORTAL_TIMEOUT_MS (10 * 60 * 1000)
#define PORTAL_STA_WAIT_MS 15000
#define PORTAL_RESTART_DELAY_MS 4000
#define PORTAL_SCAN_MAX 20

static httpd_handle_t portal_httpd = NULL;
static int64_t portal_last_activity = 0;
static volatile bool portal_restart_pending = false;

static void portal_touch(void)
{
  portal_last_activity = esp_timer_get_time() / 1000;
}

/* ---------- page ---------- */

static esp_err_t portal_index_get(httpd_req_t *req)
{
  portal_touch();
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, portal_html_start, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}
static httpd_uri_t portal_index = {.uri = "/", .method = HTTP_GET, .handler = portal_index_get};

/* ---------- wifi scan ---------- */

static esp_err_t portal_scan_get(httpd_req_t *req)
{
  portal_touch();
  wifi_scan_config_t sc = { .show_hidden = false };
  esp_wifi_scan_start(&sc, true);
  wifi_ap_record_t recs[PORTAL_SCAN_MAX];
  uint16_t n = PORTAL_SCAN_MAX;
  esp_wifi_scan_get_ap_records(&n, recs);

  char *buf = malloc(1024);
  if (!buf) {
    httpd_resp_set_status(req, "500 Server Error");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
  }
  int off = snprintf(buf, 1024, "[");
  for (int i = 0; i < n && off < 900; i++) {
    char esc[64];
    size_t j = 0;
    for (const char *s = (const char *)recs[i].ssid; *s && j < sizeof(esc) - 5; s++) {
      if (*s == '"' || *s == '\\') esc[j++] = '\\';
      esc[j++] = *s;
    }
    esc[j] = 0;
    int auth = recs[i].authmode != WIFI_AUTH_OPEN;
    off += snprintf(buf + off, 1024 - off, "%s{\"s\":\"%s\",\"r\":%d,\"a\":%d}",
                    i ? "," : "", esc, recs[i].rssi, auth);
  }
  snprintf(buf + off, 1024 - off, "]");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
  free(buf);
  return ESP_OK;
}
static httpd_uri_t portal_scan = {.uri = "/scan", .method = HTTP_GET, .handler = portal_scan_get};

/* ---------- save with validation ---------- */

static bool portal_sta_has_ip(void)
{
  esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  tcpip_adapter_ip_info_t info;
  memset(&info, 0, sizeof(info));
  if (!sta) return false;
  if (esp_netif_get_ip_info(sta, &info) != ESP_OK) return false;
  return info.ip.addr != 0;
}

static esp_err_t portal_save_post(httpd_req_t *req)
{
  portal_touch();
  char body[512];
  int received = 0;
  while (received < (int)sizeof(body) - 1) {
    int r = httpd_req_recv(req, body + received, sizeof(body) - 1 - received);
    if (r <= 0) break;
    received += r;
  }
  body[received] = 0;

  char ssid[33] = {0};
  char pass[65] = {0};
  char *q = strstr(body, "\"ssid\":\"");
  if (q) {
    q += 8;
    size_t i = 0;
    while (*q && *q != '"' && i < sizeof(ssid) - 1) ssid[i++] = *q++;
  }
  q = strstr(body, "\"pass\":\"");
  if (q) {
    q += 8;
    size_t i = 0;
    while (*q && *q != '"' && i < sizeof(pass) - 1) pass[i++] = *q++;
  }
  if (!ssid[0]) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"state\":\"fail\",\"why\":\"ssid\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }

  wifi_config_t wc;
  memset(&wc, 0, sizeof(wc));
  strcpy((char *)wc.sta.ssid, ssid);
  strcpy((char *)wc.sta.password, pass);
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wc));
  esp_wifi_disconnect();
  esp_wifi_connect();

  bool ok = false;
  for (int waited = 0; waited < PORTAL_STA_WAIT_MS; waited += 500) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    if (portal_sta_has_ip()) { ok = true; break; }
  }

  httpd_resp_set_type(req, "application/json");
  if (ok) {
    ESP_ERROR_CHECK(write_nvs_str("wifi_ssid", ssid));
    ESP_ERROR_CHECK(write_nvs_str("wifi_pass", pass));
    ESP_LOGI(TAG, "credentials validated, saved, restarting");
    portal_restart_pending = true;
    httpd_resp_send(req, "{\"state\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
  } else {
    esp_wifi_disconnect();
    httpd_resp_send(req, "{\"state\":\"fail\",\"why\":\"connect\"}", HTTPD_RESP_USE_STRLEN);
  }
  return ESP_OK;
}
static httpd_uri_t portal_save = {.uri = "/save", .method = HTTP_POST, .handler = portal_save_post};

static esp_err_t portal_status_get(httpd_req_t *req)
{
  portal_touch();
  char resp[48];
  snprintf(resp, sizeof(resp), "{\"state\":\"%s\"}", portal_restart_pending ? "saved" : "idle");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}
static httpd_uri_t portal_status = {.uri = "/status", .method = HTTP_GET, .handler = portal_status_get};

/* ---------- lifecycle ---------- */

void wifi_portal_start(void)
{
  esp_netif_init();
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  esp_netif_create_default_wifi_ap();
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t wic = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wic));

  uint8_t mac[6];
  ESP_ERROR_CHECK(esp_wifi_get_mac(ESP_IF_WIFI_STA, mac));
  /* ssid carries the mac suffix so multiple boards are distinguishable
     in the phone's wifi list; the password is a fixed kconfig value */
  char ap_pass[65];
  snprintf(ap_pass, sizeof(ap_pass), "%s", CONFIG_WIFI_PORTAL_PASSWORD);
  char ap_name[33];
  snprintf(ap_name, sizeof(ap_name), "%s-setup-%02x%02x%02x",
           CONFIG_CLIENT_ID, mac[3], mac[4], mac[5]);
  ESP_LOGI(TAG, "portal starting: ssid=%s pass=%s", ap_name, ap_pass);

  wifi_config_t ap;
  memset(&ap, 0, sizeof(ap));
  strcpy((char *)ap.ap.ssid, ap_name);
  strcpy((char *)ap.ap.password, ap_pass);
  ap.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
  ap.ap.max_connection = 2;
  ap.ap.channel = 1;
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap));
  ESP_ERROR_CHECK(esp_wifi_start());

  httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
  hc.stack_size = 4096;
  if (httpd_start(&portal_httpd, &hc) != ESP_OK) {
    ESP_LOGE(TAG, "cannot start httpd, restarting");
    esp_restart();
  }
  httpd_register_uri_handler(portal_httpd, &portal_index);
  httpd_register_uri_handler(portal_httpd, &portal_scan);
  httpd_register_uri_handler(portal_httpd, &portal_save);
  httpd_register_uri_handler(portal_httpd, &portal_status);

  portal_touch();
  while (1) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    int64_t now = esp_timer_get_time() / 1000;
    if (portal_restart_pending) {
      vTaskDelay(PORTAL_RESTART_DELAY_MS / portTICK_PERIOD_MS);
      esp_restart();
    }
    if (now - portal_last_activity > PORTAL_TIMEOUT_MS) {
      ESP_LOGW(TAG, "portal idle timeout, restarting");
      esp_restart();
    }
  }
}

#endif // CONFIG_WIFI_PROVISIONING_PORTAL
