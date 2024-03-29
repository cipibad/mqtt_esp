#include "esp_system.h"
#ifdef CONFIG_TARGET_DEVICE_ESP8266
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "app_wifi.h"
#include "app_nvs.h"

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
#ifdef CONFIG_WIFI_MODE_MIXED
  case SYSTEM_EVENT_AP_START:
    ESP_LOGI(TAG, "SoftAP started");
    break;
  case SYSTEM_EVENT_AP_STOP:
    ESP_LOGI(TAG, "SoftAP stopped");
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
#endif // CONFIG_WIFI_MODE_MIXED
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
  return ESP_OK;
}

void wifi_init(void)
{

  memset(wifi_ssid, 0, MAX_WIFI_CONFIG_LEN);
  memset(wifi_pass, 0, MAX_WIFI_CONFIG_LEN);
  tcpip_adapter_init();
  ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL) );
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
  ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

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

#ifdef CONFIG_WIFI_MODE_MIXED
  wifi_config_t wifi_config_ap = {
    .ap = {
      .max_connection = 5,
      .ssid = CONFIG_WIFI_AP_SSID,
      .password = CONFIG_WIFI_AP_PASSWORD,
      .authmode = WIFI_AUTH_WPA_WPA2_PSK,
    },
  };

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config_sta));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config_ap));
#else // CONFIG_WIFI_MODE_MIXED
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config_sta));
#endif // CONFIG_WIFI_MODE_MIXED
  ESP_LOGI(TAG, "connecting to WiFi ssid:[%s]", wifi_config_sta.sta.ssid);
  ESP_LOGI(TAG, "with pass:[%s]", wifi_config_sta.sta.password);
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG, "Waiting for wifi");
  xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
}
#endif //CONFIG_TARGET_DEVICE_ESP8266
