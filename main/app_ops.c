#include "esp_system.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "app_esp8266.h"
#include "app_ops.h"

static const char *TAG = "MQTTS_OPS";



extern EventGroupHandle_t mqtt_event_group;
extern const int MQTT_PUBLISHED_BIT;
extern const int MQTT_INIT_FINISHED_BIT;

void publish_ops_data(esp_mqtt_client_handle_t client)
{
  if (xEventGroupGetBits(mqtt_event_group) & MQTT_INIT_FINISHED_BIT)
    {
      const char * connect_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/ops";
      char data[256];
      memset(data,0,256);

      sprintf(data, "{\"free_heap\":%d, \"min_free_heap\":%d}",
              esp_get_free_heap_size(),
              esp_get_minimum_free_heap_size());
      xEventGroupClearBits(mqtt_event_group, MQTT_PUBLISHED_BIT);
      int msg_id = esp_mqtt_client_publish(client, connect_topic, data,strlen(data), 1, 0);
      if (msg_id > 0) {
        ESP_LOGI(TAG, "sent publish connected data successful, msg_id=%d", msg_id);
        EventBits_t bits = xEventGroupWaitBits(mqtt_event_group, MQTT_PUBLISHED_BIT, false, true, MQTT_FLAG_TIMEOUT);
        if (bits & MQTT_PUBLISHED_BIT) {
          ESP_LOGI(TAG, "publish ack received, msg_id=%d", msg_id);
        } else {
          ESP_LOGW(TAG, "publish ack not received, msg_id=%d", msg_id);
        }
      } else {
        ESP_LOGW(TAG, "failed to publish connected data, msg_id=%d", msg_id);
      }
    }
}


void ops_pub_task(void* pvParameters)
{
  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;
  while (1) {
    publish_ops_data(client);
    vTaskDelay(60000 / portTICK_PERIOD_MS);
  }
}

