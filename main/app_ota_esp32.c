#include "esp_system.h"
#ifdef CONFIG_TARGET_DEVICE_ESP32
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"


#include "esp_wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "cJSON.h"

#include "app_main.h"
#include "app_ota.h"

static const char *TAG = "MQTTS_OTA";

extern QueueHandle_t otaQueue;

extern EventGroupHandle_t mqtt_event_group;
extern const int MQTT_PUBLISHED_BIT;
extern const int MQTT_INIT_FINISHED_BIT;

/* #define BUFFSIZE 1024 */
/* #define HASH_LEN 32 /\* SHA-256 digest length *\/ */

/* /\*an ota data write buffer ready to write to the flash*\/ */
/* static char ota_write_data[BUFFSIZE + 1] = { 0 }; */


/* static void http_cleanup(esp_http_client_handle_t client) */
/* { */
/*     esp_http_client_close(client); */
/*     esp_http_client_cleanup(client); */
/* } */


/* int handle_ota_update_cmd(esp_mqtt_event_handle_t event) */
/* { */
/*   //  esp_mqtt_client_handle_t client = event->client; */
/*   char * url = "https://sw.iot.cipex.ro:8911/" CONFIG_MQTT_CLIENT_ID ".bin"; */
/*   extern const uint8_t server_cert_pem_start[] asm("_binary_sw_iot_cipex_ro_pem_start"); */

/*   printf("url: %s\r\n", url); */

/*   esp_err_t err; */
/*   /\* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() *\/ */
/*   esp_ota_handle_t update_handle = 0 ; */
/*   const esp_partition_t *update_partition = NULL; */

/*   ESP_LOGI(TAG, "Starting OTA example..."); */

/*   const esp_partition_t *configured = esp_ota_get_boot_partition(); */
/*   const esp_partition_t *running = esp_ota_get_running_partition(); */

/*   if (configured != running) { */
/*     ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x", */
/*              configured->address, running->address); */
/*     ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)"); */
/*   } */
/*   ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)", */
/*            running->type, running->subtype, running->address); */


/*   esp_http_client_config_t config = { */
/*     .url = url, */
/*     .cert_pem = (char *)server_cert_pem_start, */
/*   }; */
/*     esp_http_client_handle_t client = esp_http_client_init(&config); */
/*     if (client == NULL) { */
/*         ESP_LOGE(TAG, "Failed to initialise HTTP connection"); */
/*         //task_fatal_error(); */
/*     } */
/*     err = esp_http_client_open(client, 0); */
/*     if (err != ESP_OK) { */
/*         ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err)); */
/*         esp_http_client_cleanup(client); */
/*         //task_fatal_error(); */
/*     } */
/*     esp_http_client_fetch_headers(client); */

/*     update_partition = esp_ota_get_next_update_partition(NULL); */
/*     ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x", */
/*              update_partition->subtype, update_partition->address); */
/*     assert(update_partition != NULL); */

/*     err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle); */
/*     if (err != ESP_OK) { */
/*         ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err)); */
/*         http_cleanup(client); */
/*         //task_fatal_error(); */
/*     } */
/*     ESP_LOGI(TAG, "esp_ota_begin succeeded"); */

/*     int binary_file_length = 0; */
/*     /\*deal with all receive packet*\/ */
/*     while (1) { */
/*         int data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE); */
/*         if (data_read < 0) { */
/*             ESP_LOGE(TAG, "Error: SSL data read error"); */
/*             http_cleanup(client); */
/*             //task_fatal_error(); */
/*         } else if (data_read > 0) { */
/*             err = esp_ota_write( update_handle, (const void *)ota_write_data, data_read); */
/*             if (err != ESP_OK) { */
/*                 http_cleanup(client); */
/*                 //task_fatal_error(); */
/*             } */
/*             binary_file_length += data_read; */
/*             ESP_LOGD(TAG, "Written image length %d", binary_file_length); */
/*         } else if (data_read == 0) { */
/*             ESP_LOGI(TAG, "Connection closed,all data received"); */
/*             break; */
/*         } */
/*     } */
/*     ESP_LOGI(TAG, "Total Write binary data length : %d", binary_file_length); */

/*     if (esp_ota_end(update_handle) != ESP_OK) { */
/*         ESP_LOGE(TAG, "esp_ota_end failed!"); */
/*         http_cleanup(client); */
/*         //task_fatal_error(); */
/*     } */

/*     if (esp_partition_check_identity(esp_ota_get_running_partition(), update_partition) == true) { */
/*         ESP_LOGI(TAG, "The current running firmware is same as the firmware just downloaded"); */
/*         int i = 0; */
/*         ESP_LOGI(TAG, "When a new firmware is available on the server, press the reset button to download it"); */
/*         while(1) { */
/*             ESP_LOGI(TAG, "Waiting for a new firmware ... %d", ++i); */
/*             vTaskDelay(2000 / portTICK_PERIOD_MS); */
/*         } */
/*     } */

/*     err = esp_ota_set_boot_partition(update_partition); */
/*     if (err != ESP_OK) { */
/*         ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err)); */
/*         http_cleanup(client); */
/*         //task_fatal_error(); */
/*     } */
/*     ESP_LOGI(TAG, "Prepare to restart system!"); */
/*     esp_restart(); */
/*     return ; */

/* } */

//=====================================================
// old code bellow


void publish_ota_data(esp_mqtt_client_handle_t client, int status)
{
  if (xEventGroupGetBits(mqtt_event_group) & MQTT_INIT_FINISHED_BIT)
    {
      const char * connect_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/ota";
      char data[256];
      memset(data,0,256);

      sprintf(data, "{\"status\":%d}", status);
      xEventGroupClearBits(mqtt_event_group, MQTT_PUBLISHED_BIT);
      int msg_id = esp_mqtt_client_publish(client, connect_topic, data,strlen(data), 1, 0);
      if (msg_id > 0) {
        ESP_LOGI(TAG, "sent publish ota data successful, msg_id=%d", msg_id);
        EventBits_t bits = xEventGroupWaitBits(mqtt_event_group, MQTT_PUBLISHED_BIT, false, true, MQTT_FLAG_TIMEOUT);
        if (bits & MQTT_PUBLISHED_BIT) {
          ESP_LOGI(TAG, "publish ack received, msg_id=%d", msg_id);
        } else {
          ESP_LOGW(TAG, "publish ack not received, msg_id=%d", msg_id);
        }
      } else {
        ESP_LOGI(TAG, "failed to publish ota data, msg_id=%d", msg_id);
      }
    }
}


esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
  switch(evt->event_id) {
  case HTTP_EVENT_ERROR:
    ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
    break;
  case HTTP_EVENT_ON_CONNECTED:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
    break;
  case HTTP_EVENT_HEADER_SENT:
    ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
    break;
  case HTTP_EVENT_ON_HEADER:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
    break;
  case HTTP_EVENT_ON_DATA:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
    break;
  case HTTP_EVENT_ON_FINISH:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
    break;
  case HTTP_EVENT_DISCONNECTED:
    ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
    break;
  }
  return ESP_OK;
}

void handle_ota_update_task(void* pvParameters)
{

  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;
  struct OtaMessage o;
  while(1) {
    if( xQueueReceive( otaQueue, &o , portMAX_DELAY) )
      {

        /* esp_wifi_stop(); */
        /* vTaskDelay(60000 / portTICK_PERIOD_MS); */
        /* esp_wifi_start(); */
        /* xEventGroupWaitBits(mqtt_event_group, CONNECTED_BIT, false, true, portMAX_DELAY) */;
        publish_ota_data(client, OTA_ONGOING);
        char * url=o.url;
        static const char *TAG = "simple_ota_example";

        ESP_LOGI(TAG, "Starting OTA example...");
        extern const uint8_t server_cert_pem_start[] asm("_binary_sw_iot_cipex_ro_pem_start");

        esp_http_client_config_t config = {
          .url = url,
          .cert_pem = (char *)server_cert_pem_start,
          .event_handler = _http_event_handler,
        };
        esp_err_t ret = esp_https_ota(&config);
        if (ret == ESP_OK) {
          ESP_LOGI(TAG, "Firmware Upgrades Success, will restart in 10 seconds");
          publish_ota_data(client, OTA_SUCCESFULL);
          vTaskDelay(10000 / portTICK_PERIOD_MS);
          esp_restart();
        } else {
          publish_ota_data(client, OTA_FAILED);
          ESP_LOGE(TAG, "Firmware Upgrades Failed");
        }
      }
  }
}

#endif //CONFIG_TARGET_DEVICE_ESP32
