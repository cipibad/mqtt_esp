#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_smartconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"


#include "app_nvs.h"
#include "app_smart_config.h"

static const char *TAG = "MQTTS_SMARTCONFIG";

const char * smartconfigTAG="targetTemp";
int smartconfigFlag = 0; 

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
            ESP_LOGI(TAG, "SSID:%s", wifi_config->sta.ssid);
            ESP_LOGI(TAG, "PASSWORD:%s", wifi_config->sta.password);
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

void smartconfig_example_task(void * parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    ESP_ERROR_CHECK( esp_smartconfig_start(sc_callback) );
    while (1) {
        uxBits = xEventGroupWaitBits(smartconfig_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY); 
        if(uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            //FIXME save to nvs and reboot in 10 seconds
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
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

void smartconfig_cmd_task(void* pvParameters)
{
  ESP_LOGI(TAG, "smartconfig_cmd_task started");
  smartconfig_event_group = xEventGroupCreate();
  int n;
  while (1) {

    if (smartconfigFlag) {
      ESP_LOGI(TAG, "starting smartconfig");

    } else {
      ESP_LOGI(TAG, "smartconfig not enabled, waiting request");
      if( xQueueReceive( smartconfigQueue, &n , portMAX_DELAY) )
        {
          ESP_LOGI(TAG, "received smartconfig request: %d", n);
          ESP_ERROR_CHECK(write_nvs_integer(smartconfigTAG, ! smartconfigFlag));
          ESP_LOGI(TAG, "Prepare to restart system in 10 seconds!");
          vTaskDelay(10000 / portTICK_PERIOD_MS);
          esp_restart();
        }
    }
  }
}
