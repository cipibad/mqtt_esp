#include "esp_system.h"
#ifdef CONFIG_TARGET_DEVICE_ESP8266

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
#include "smartconfig_ack.h"


#include "app_main.h"

static const char *TAG = "MQTTS_SMARTCONFIG";

const char * smartconfigTAG="smartconfigFlag";
int smartconfigFlag = 0;

const char * wifi_ssid_tag;
const char * wifi_pass_tag;

char wifi_ssid[MAX_WIFI_CONFIG_LEN];
char wifi_pass[MAX_WIFI_CONFIG_LEN];


/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int REQUESTED_BIT = BIT0;
static const int CONNECTED_BIT = BIT1;
static const int ESPTOUCH_DONE_BIT = BIT2;


extern QueueHandle_t smartconfigQueue;
EventGroupHandle_t smartconfig_event_group;

#if CONFIG_MQTT_RELAYS_NB
#include "app_relay.h"
extern int relayStatus[CONFIG_MQTT_RELAYS_NB];
extern QueueHandle_t relayQueue;
#endif //CONFIG_MQTT_RELAYS_NB

#ifdef CONFIG_MQTT_OTA
#include "app_ota.h"
extern QueueHandle_t otaQueue;
#endif // CONFIG_MQTT_OTA

#define TICKS_FORMAT "%ld"

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
      sc_callback_data_t *sc_callback_data = (sc_callback_data_t *)pdata;
      switch (sc_callback_data->type) {
        case SC_ACK_TYPE_ESPTOUCH:
          ESP_LOGI(TAG, "Phone ip: %d.%d.%d.%d", sc_callback_data->ip[0], sc_callback_data->ip[1], sc_callback_data->ip[2], sc_callback_data->ip[3]);
          ESP_LOGI(TAG, "TYPE: ESPTOUCH");
          break;
        case SC_ACK_TYPE_AIRKISS:
          ESP_LOGI(TAG, "TYPE: AIRKISS");
          break;
        default:
          ESP_LOGE(TAG, "TYPE: ERROR");
          break;
      }
    }
    xEventGroupSetBits(smartconfig_event_group, ESPTOUCH_DONE_BIT);
    break;
  default:
    break;
  }
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
  /* For accessing reason codes in case of disconnection */
  system_event_info_t *info = &event->event_info;

  switch(event->event_id) {
  case SYSTEM_EVENT_STA_START:
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_AIRKISS) );
    ESP_ERROR_CHECK( esp_smartconfig_start(sc_callback) );
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    xEventGroupSetBits(smartconfig_event_group, CONNECTED_BIT);
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    ESP_LOGE(TAG, "Disconnect reason : %d", info->disconnected.reason);
    if (info->disconnected.reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
        /*Switch to 802.11 bgn mode */
        esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    }

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
  struct SmartConfigMessage scm;;
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
    const TickType_t three_seconds = 3 * 1000 / portTICK_PERIOD_MS;
    const TickType_t seven_seconds = 7 * 1000 / portTICK_PERIOD_MS;

    const TickType_t startupTicks = xTaskGetTickCount();
    while (1) {
      if( xQueueReceive( smartconfigQueue, &scm , portMAX_DELAY) )
        {
          ESP_LOGI(TAG, "received switch event at: %d", scm.ticks);
          if (scm.ticks - lastRead < 5) {
            lastRead = scm.ticks;
            ESP_LOGI(TAG, "duplicate call, ignored");
            continue;
          }
          if (scm.ticks < startupTicks) {
            lastRead = scm.ticks;
            ESP_LOGI(TAG, "ignore signals at startup");
            continue;
          }
          lastRead = scm.ticks;
          if (pushTick == 0) {
            ESP_LOGI(TAG, "down ");
            pushTick = scm.ticks;
          } else {
            ESP_LOGI(TAG, "up ");
            if ((scm.ticks - pushTick ) < three_seconds) {
#if CONFIG_MQTT_RELAYS_NB
              struct RelayMessage r={RELAY_CMD_STATUS, scm.relayId, !(relayStatus[(int)scm.relayId] == GPIO_HIGH)};
              xQueueSend(relayQueue,
                         ( void * )&r,
                         RELAY_QUEUE_TIMEOUT);
#endif //CONFIG_MQTT_RELAYS_NB
            } else if ((scm.ticks - pushTick ) >= three_seconds &&
                       (scm.ticks - pushTick ) < seven_seconds) {
              ESP_LOGI(TAG, "received smartconfig request:");
              ESP_ERROR_CHECK(write_nvs_integer(smartconfigTAG, ! smartconfigFlag));
              ESP_LOGI(TAG, "Prepare to restart system in 10 seconds!");
              vTaskDelay(2   / portTICK_PERIOD_MS);
              esp_restart();
            } else { // (scm.ticks - pushTick ) >= seven_seconds
              #ifdef CONFIG_MQTT_OTA
              //trigger OTA
              struct OtaMessage o={"https://sw.iot.cipex.ro:8911/" CONFIG_CLIENT_ID ".bin"};
              if (xQueueSend( otaQueue
                    ,( void * )&o
                    ,OTA_QUEUE_TIMEOUT) != pdPASS) {
                ESP_LOGE(TAG, "Cannot send to otaQueue");
              }
              #endif // CONFIG_MQTT_OTA
            }
            pushTick = 0;
          }
          ESP_LOGI(TAG, "pushTick: " TICKS_FORMAT, pushTick);
        }
    }
  }
}
#endif //CONFIG_TARGET_DEVICE_ESP8266
