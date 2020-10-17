#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_smartconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"


#include "app_wifi.h"
#include "app_nvs.h"
#include "app_smart_config.h"

#include "app_main.h"
#include "app_relay.h"

static const char *TAG = "MQTTS_SMARTCONFIG";

const char * smartconfigTAG="smartconfigFlag";
int smartconfigFlag = 0;

const char * wifi_ssid_tag;
const char * wifi_pass_tag;

char wifi_ssid[MAX_WIFI_SSID_LEN];
char wifi_pass[MAX_WIFI_PASS_LEN];


/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int REQUESTED_BIT = BIT0;
static const int CONNECTED_BIT = BIT1;
static const int ESPTOUCH_DONE_BIT = BIT2;


extern QueueHandle_t smartconfigQueue;

EventGroupHandle_t s_wifi_event_group;

#if CONFIG_MQTT_RELAYS_NB
extern int relayStatus[CONFIG_MQTT_RELAYS_NB];
extern QueueHandle_t relayQueue;
#endif //CONFIG_MQTT_RELAYS_NB


#ifdef CONFIG_TARGET_DEVICE_ESP32
#define TICKS_FORMAT "%d"
#else // CONFIG_TARGET_DEVICE_ESP32
#define TICKS_FORMAT "%ld"
#endif // CONFIG_TARGET_DEVICE_ESP32


static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    esp_wifi_connect();
    xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
  } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
    ESP_LOGI(TAG, "Scan done");
  } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
    ESP_LOGI(TAG, "Found channel");
  } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
    ESP_LOGI(TAG, "Got SSID and password");

    smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;

    memset(wifi_ssid, 0, MAX_WIFI_SSID_LEN);
    memset(wifi_pass, 0, MAX_WIFI_PASS_LEN);

    memcpy(wifi_ssid, evt->ssid, sizeof(evt->ssid));
    memcpy(wifi_pass, evt->password, sizeof(evt->password));
    ESP_LOGI(TAG, "SSID:%s", wifi_ssid);
    ESP_LOGI(TAG, "PASSWORD:%s", wifi_pass);

    wifi_config_t wifi_config;

    bzero(&wifi_config, sizeof(wifi_config_t));
    memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
    wifi_config.sta.bssid_set = evt->bssid_set;
    if (wifi_config.sta.bssid_set == true) {
      memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
    }

    ESP_ERROR_CHECK( esp_wifi_disconnect() );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_connect() );
  } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
    xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
  }
}

static void initialise_wifi(void)
{
  ESP_ERROR_CHECK(esp_netif_init());
  s_wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

  ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
  ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
  ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );

  ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK( esp_wifi_start() );
}

void smartconfig_cmd_task(void* pvParameters)
{
  ESP_LOGI(TAG, "smartconfig_cmd_task started");
  struct SmartConfigMessage scm;;
  if (smartconfigFlag) {
    ESP_LOGI(TAG, "starting smartconfig");
    initialise_wifi();

    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );

    while (1) {
      uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
      if(uxBits & CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi Connected to ap");
      }
      if(uxBits & ESPTOUCH_DONE_BIT) {
        ESP_LOGI(TAG, "smartconfig over");
        esp_err_t err=write_nvs_str(wifi_ssid_tag, wifi_ssid);
        ESP_ERROR_CHECK( err );

        err=write_nvs_str(wifi_pass_tag, wifi_pass);
        ESP_ERROR_CHECK( err );


        ESP_LOGI(TAG, "Prepare to restart system in 10 seconds!");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        esp_restart();
      }
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  } else {
    ESP_LOGI(TAG, "smartconfig not enabled, waiting request");
    TickType_t pushTick = 0;
    TickType_t lastRead = 0;
    const TickType_t ticksToWait = 3000/portTICK_PERIOD_MS ; // 3 seconds
    ESP_LOGI(TAG, "ticksToWait: " TICKS_FORMAT, ticksToWait);
    while (1) {
      if( xQueueReceive( smartconfigQueue, &scm , portMAX_DELAY) )
        {
          ESP_LOGI(TAG, "received switch event at: %d", scm.ticks);
          if (scm.ticks - lastRead < 5) {
            ESP_LOGI(TAG, "duplicate call, ignored ");
            lastRead = scm.ticks;
            continue;
          }
          lastRead = scm.ticks;
          if (pushTick == 0) {
            ESP_LOGI(TAG, "down ");
            pushTick = scm.ticks;
          } else {
            ESP_LOGI(TAG, "up ");
            if ((scm.ticks - pushTick ) < ticksToWait) {
#if CONFIG_MQTT_RELAYS_NB
              struct RelayMessage r={RELAY_CMD_STATUS, scm.relayId, !(relayStatus[(int)scm.relayId] == RELAY_ON)};
              xQueueSend(relayQueue,
                         ( void * )&r,
                         RELAY_QUEUE_TIMEOUT);
#endif //CONFIG_MQTT_RELAYS_NB
            }
            else {
              ESP_LOGI(TAG, "received smartconfig request:");
              ESP_ERROR_CHECK(write_nvs_integer(smartconfigTAG, ! smartconfigFlag));
              ESP_LOGI(TAG, "Prepare to restart system in 10 seconds!");
              vTaskDelay(10000 / portTICK_PERIOD_MS);
              esp_restart();
            }
            pushTick = 0;
          }
          ESP_LOGI(TAG, "pushTick: " TICKS_FORMAT, pushTick);
        }
    }
  }
}
