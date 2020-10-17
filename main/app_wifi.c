#include <string.h>

#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "app_wifi.h"
#include "app_nvs.h"

EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

static const char *TAG = "MQTTS_WIFI";

const char * wifi_ssid_tag = "wifi_ssid";
const char * wifi_pass_tag = "wifi_pass";

char wifi_ssid[MAX_WIFI_SSID_LEN];
char wifi_pass[MAX_WIFI_PASS_LEN];



static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    ESP_LOGW(TAG, "Wifi: SYSTEM_EVENT_STA_START");
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGW(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
  }
}

void wifi_init(void)
{

  memset(wifi_ssid, 0, MAX_WIFI_SSID_LEN);
  memset(wifi_pass, 0, MAX_WIFI_PASS_LEN);

  size_t length = sizeof(wifi_ssid);
  esp_err_t err=read_nvs_str(wifi_ssid_tag, wifi_ssid, &length);
  ESP_ERROR_CHECK( err );

  length = sizeof(wifi_pass);
  err=read_nvs_str(wifi_pass_tag, wifi_pass, &length);
  ESP_ERROR_CHECK( err );


  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

  wifi_config_t wifi_config = {
     .sta = {
      .ssid = CONFIG_WIFI_SSID,
      .password = CONFIG_WIFI_PASSWORD,
      /* Setting a password implies station will connect to all security modes including WEP/WPA.
       * However these modes are deprecated and not advisable to be used. Incase your Access point
       * doesn't support WPA2, these mode can be enabled by commenting below line */
      .threshold.authmode = WIFI_AUTH_WPA2_PSK,

      .pmf_cfg = {
                  .capable = true,
                  .required = false
                  },
             },
  };

  if (strlen(wifi_ssid) && strlen(wifi_pass)) {
    ESP_LOGI(TAG, "using nvs wifi config");
    strcpy((char*)wifi_config.sta.ssid, wifi_ssid);
    strcpy((char*)wifi_config.sta.password, wifi_pass);
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
  ESP_LOGI(TAG, "start the WIFI SSID:[%s]", wifi_config.sta.ssid);
  ESP_LOGI(TAG, "connecting with pass:[%s]", wifi_config.sta.password);
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG, "wifi_init_sta finished, waiting for wifi");

  xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);

}
