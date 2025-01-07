#include "esp_system.h"
#ifdef CONFIG_TARGET_DEVICE_ESP8266
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef CONFIG_SOUTH_INTERFACE_UDP
#include "mdns.h"
#endif // CONFIG_SOUTH_INTERFACE_UDP


#include "app_wifi.h"
#include "app_nvs.h"

#ifdef CONFIG_NORTH_INTERFACE_HTTP
#include "app_http_server.h"
extern httpd_handle_t server;
#endif // CONFIG_NORTH_INTERFACE_HTTP

EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

static const char *TAG = "MQTTS_WIFI";

const char * wifi_ssid_tag = "wifi_ssid";
const char * wifi_pass_tag = "wifi_pass";

char wifi_ssid[MAX_WIFI_CONFIG_LEN];
char wifi_pass[MAX_WIFI_CONFIG_LEN];



static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
  switch (event->event_id) {
#if defined(CONFIG_WIFI_MODE_MIXED) || defined(CONFIG_WIFI_MODE_ACCESS_POINT)
  case SYSTEM_EVENT_AP_START:
    ESP_LOGI(TAG, "SoftAP started");
#ifdef CONFIG_NORTH_INTERFACE_HTTP
    /* Start the web server */
    if (server == NULL) {
        server = start_webserver();
    }
#endif // CONFIG_NORTH_INTERFACE_HTTP
    break;
  case SYSTEM_EVENT_AP_STOP:
    ESP_LOGI(TAG, "SoftAP stopped");
#ifdef CONFIG_NORTH_INTERFACE_HTTP
    /* Stop the web server */
    if (server) {
        stop_webserver(server);
        server = NULL;
    }
#endif // CONFIG_NORTH_INTERFACE_HTTP
    break;
  case SYSTEM_EVENT_AP_STACONNECTED:
    ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d",
            MAC2STR(event->event_info.sta_connected.mac),
            event->event_info.sta_connected.aid);
    break;
  case SYSTEM_EVENT_AP_STADISCONNECTED:
    ESP_LOGI(TAG, "station:"MACSTR"leave, AID=%d",
            MAC2STR(event->event_info.sta_disconnected.mac),
            event->event_info.sta_disconnected.aid);
    break;
#endif // defined(CONFIG_WIFI_MODE_MIXED) || defined(CONFIG_WIFI_MODE_ACCESS_POINT)
  case SYSTEM_EVENT_STA_START:
    ESP_LOGW(TAG, "Wifi: SYSTEM_EVENT_STA_START");
    esp_wifi_connect();
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    ESP_LOGW(TAG, "SYSTEM_EVENT_STA_GOT_IP");
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    ESP_LOGW(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
    esp_wifi_connect();
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    break;
  default:
    break;
  }
  #ifdef CONFIG_SOUTH_INTERFACE_UDP
  mdns_handle_system_event(ctx, event);
  #endif // CONFIG_SOUTH_INTERFACE_UDP

  return ESP_OK;
}

void wifi_init(void)
{
  #ifdef CONFIG_SOUTH_INTERFACE_UDP
  ESP_LOGI(TAG, "mdns_init");
  ESP_ERROR_CHECK( mdns_init() );
  // set mDNS hostname (required if you want to advertise services)
  ESP_LOGI(TAG, "mdns_hostname_set to %s", CONFIG_CLIENT_ID);
  ESP_ERROR_CHECK( mdns_hostname_set(CONFIG_CLIENT_ID) );
  // set default mDNS instance name
  ESP_LOGI(TAG, "mdns_instance_name_set");
  ESP_ERROR_CHECK( mdns_instance_name_set(CONFIG_CLIENT_ID) );

  ESP_LOGI(TAG, "mdns_service_add");
  mdns_txt_item_t serviceTxtData[] = {};
  ESP_ERROR_CHECK( mdns_service_add(CONFIG_CLIENT_ID, "_uiot", "_udp", CONFIG_SOUTH_INTERFACE_UDP_PORT, serviceTxtData, 0) );
  ESP_LOGI(TAG, "mdns configured");
  #endif // CONFIG_SOUTH_INTERFACE_UDP

  memset(wifi_ssid, 0, MAX_WIFI_CONFIG_LEN);
  memset(wifi_pass, 0, MAX_WIFI_CONFIG_LEN);
  tcpip_adapter_init();
  ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL) );
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
  ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

#ifdef CONFIG_WIFI_MODE_ACCESS_POINT
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
#endif // CONFIG_WIFI_MODE_ACCESS_POINT

#ifdef CONFIG_WIFI_MODE_STATION
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
#endif // CONFIG_WIFI_MODE_STATION

#ifdef CONFIG_WIFI_MODE_MIXED
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
#endif // CONFIG_WIFI_MODE_MIXED

#if defined(CONFIG_WIFI_MODE_MIXED) || defined(CONFIG_WIFI_MODE_ACCESS_POINT)
  wifi_config_t wifi_config_ap = {
    .ap = {
      .max_connection = 5,
      .ssid = CONFIG_WIFI_AP_SSID,
      .password = CONFIG_WIFI_AP_PASSWORD,
      .authmode = WIFI_AUTH_WPA_WPA2_PSK,
      .ssid_hidden = 1,
    },
  };

  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config_ap));
#endif // defined(CONFIG_WIFI_MODE_MIXED) || defined(CONFIG_WIFI_MODE_ACCESS_POINT)

#if defined(CONFIG_WIFI_MODE_STATION) || defined(CONFIG_WIFI_MODE_MIXED)
  size_t length = sizeof(wifi_ssid);
  esp_err_t err=read_nvs_str(wifi_ssid_tag, wifi_ssid, &length);
  ESP_ERROR_CHECK( err );

  length = sizeof(wifi_pass);
  err=read_nvs_str(wifi_pass_tag, wifi_pass, &length);
  ESP_ERROR_CHECK( err );

  wifi_config_t wifi_config_sta = {
    .sta = {
      .ssid = CONFIG_WIFI_SSID,
      .password = CONFIG_WIFI_PASSWORD,
    },
  };

  if (strlen(wifi_ssid) && strlen(wifi_pass)) {
    ESP_LOGI(TAG, "using nvs wifi config");
    strcpy((char*)wifi_config_sta.sta.ssid, wifi_ssid);
    strcpy((char*)wifi_config_sta.sta.password, wifi_pass);
  }
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config_sta));
  ESP_LOGI(TAG, "connecting to WiFi ssid:[%s]", wifi_config_sta.sta.ssid);
  ESP_LOGI(TAG, "with pass:[%s]", wifi_config_sta.sta.password);
#endif // defined(CONFIG_WIFI_MODE_STATION) || defined(CONFIG_WIFI_MODE_MIXED)

  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG, "Waiting for wifi");
  xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
}
#endif //CONFIG_TARGET_DEVICE_ESP8266
