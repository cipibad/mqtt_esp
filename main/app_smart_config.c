#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_smartconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"


#include "app_wifi.h"
#include "app_nvs.h"
#include "app_smart_config.h"

#include "app_esp8266.h"
#include "app_relay.h"
extern int relayStatus[CONFIG_MQTT_RELAYS_NB];
extern QueueHandle_t relayQueue;

static const char *TAG = "MQTTS_SMARTCONFIG";

const char * smartconfigTAG="smartconfigFlag";
int smartconfigFlag = 0;

const char * wifi_ssid_tag;
const char * wifi_pass_tag;

char wifi_ssid[MAX_WIFI_CONFIG_LEN];
char wifi_pass[MAX_WIFI_CONFIG_LEN];

extern QueueHandle_t smartconfigQueue;
EventGroupHandle_t smartconfig_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int REQUESTED_BIT = BIT0;
static const int CONNECTED_BIT = BIT1;
static const int ESPTOUCH_DONE_BIT = BIT2;

static void sc_callback(smartconfig_status_t status, void *pdata)
{
  switch (status) {
  case SC_STATUS_WAIT:
    ESP_LOGI(TAG, "SC_STATUS_WAIT");
    break;
  case SC_STATUS_FIND_CHANNEL:
    ESP_LOGI(TAG, "SC_STATUS_FINDING_CHANNEL");
    break;
  case SC_STATUS_GETTING_SSID_PSWD:
    ESP_LOGI(TAG, "SC_STATUS_GETTING_SSID_PSWD");
    break;
  case SC_STATUS_LINK:
    ESP_LOGI(TAG, "SC_STATUS_LINK");
    wifi_config_t *wifi_config = pdata;
    strncpy(wifi_ssid, (char *)wifi_config->sta.ssid, sizeof(wifi_ssid));
    strncpy(wifi_pass, (char *)wifi_config->sta.password, sizeof(wifi_pass));
    ESP_LOGI(TAG, "SSID:%s", wifi_ssid);
    ESP_LOGI(TAG, "PASSWORD:%s", wifi_pass);
    ESP_ERROR_CHECK( esp_wifi_disconnect() );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_connect() );
    break;
  case SC_STATUS_LINK_OVER:
    ESP_LOGI(TAG, "SC_STATUS_LINK_OVER");
    if (pdata != NULL) {
      uint8_t phone_ip[4] = { 0 };
      memcpy(phone_ip, (uint8_t* )pdata, 4);
      ESP_LOGI(TAG, "Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
    }
    xEventGroupSetBits(smartconfig_event_group, ESPTOUCH_DONE_BIT);
    break;
  default:
    break;
  }
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
  switch(event->event_id) {
  case SYSTEM_EVENT_STA_START:
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    ESP_ERROR_CHECK( esp_smartconfig_start(sc_callback) );
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    xEventGroupSetBits(smartconfig_event_group, CONNECTED_BIT);
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    esp_wifi_connect();
    xEventGroupClearBits(smartconfig_event_group, CONNECTED_BIT);
    break;
  default:
    break;
  }
  return ESP_OK;
}

static void initialise_wifi(void)
{
  tcpip_adapter_init();
  smartconfig_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
  ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK( esp_wifi_start() );
}

void smartconfig_cmd_task(void* pvParameters)
{
  ESP_LOGI(TAG, "smartconfig_cmd_task started");
  smartconfig_event_group = xEventGroupCreate();
  int n;
  EventBits_t uxBits;
  if (smartconfigFlag) {
    ESP_LOGI(TAG, "starting smartconfig");
    initialise_wifi();
    while (1) {
      uxBits = xEventGroupWaitBits(smartconfig_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);

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
    ESP_LOGI(TAG, "ticksToWait: %ld", ticksToWait);
    while (1) {
      if( xQueueReceive( smartconfigQueue, &n , portMAX_DELAY) )
        {
          ESP_LOGI(TAG, "received switch event at: %d", n);
          if (n - lastRead < 5) {
            ESP_LOGI(TAG, "duplicate call, ignored ");
            continue;
          }
          lastRead = n;
          if (pushTick == 0) {
            ESP_LOGI(TAG, "down ");
            pushTick = n;
          }
          else {
            ESP_LOGI(TAG, "up ");
            if ((n - pushTick ) < ticksToWait) {
              struct RelayMessage r={0, !(relayStatus[0] == ON)};
              xQueueSendFromISR(relayQueue
                                ,( void * )&r
                                ,NULL);
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
          ESP_LOGI(TAG, "pushTick: %ld", pushTick);

        }
    }
  }
}
