#include <string.h>

#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "app_wifi.h"
#include "app_nvs.h"


int wifi_reconnect_counter = 0;

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
  case SYSTEM_EVENT_STA_START:
    ESP_LOGW(TAG, "Wifi: SYSTEM_EVENT_STA_START");
    esp_wifi_connect();
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    ESP_LOGW(TAG, "SYSTEM_EVENT_STA_GOT_IP");
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    wifi_reconnect_counter = 0;
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    ESP_LOGW(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
    wifi_reconnect_counter += 1;
    ESP_LOGI(TAG, "wifi_reconnect_counter: %d", wifi_reconnect_counter);
    if (wifi_reconnect_counter > (6 * 5)) { //FIXME find proper time interval
      vTaskDelay(10000 / portTICK_PERIOD_MS);
      esp_restart();
    }
    break;

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

  wifi_config_t wifi_config = {
    .sta = {
      .ssid = CONFIG_WIFI_SSID,
      .password = CONFIG_WIFI_PASSWORD,
    },
  };

  if (strlen(wifi_ssid) && strlen(wifi_pass)) {
    ESP_LOGI(TAG, "using nvs wifi config");
    strcpy((char*)wifi_config.sta.ssid, wifi_ssid);
    strcpy((char*)wifi_config.sta.password, wifi_pass);
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  ESP_LOGI(TAG, "start the WIFI SSID:[%s]", wifi_config.sta.ssid);
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG, "Waiting for wifi");
  xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
}
