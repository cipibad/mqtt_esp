#include "esp_system.h"
#ifdef CONFIG_PRESENCE_AUTOMATION_SUPPORT

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "app_presence.h"
#include "app_publish_data.h"

#include "app_relay.h"
extern QueueHandle_t relayQueue;

extern bool local_motion_detected;
#ifdef CONFIG_BH1750_SENSOR
extern uint16_t illuminance;
#endif // CONFIG_BH1750_SENSOR

bool presence_detected = false;
bool presence_cooldown = false;

EventGroupHandle_t presenceEventGroup;
const int PRESENCE_NEW_DATA_BIT = BIT0;
const int PRESENCE_TIMER_BIT = BIT1;

TimerHandle_t presence_detection_timer = NULL;
TimerHandle_t presence_cooldown_timer = NULL;

static const char *TAG = "PRESENCE";

void presence_cooldown_timer_callback( TimerHandle_t xTimer )
{
    ESP_LOGI(TAG, "cooldown expired, should disable cooldown status now");
    presence_cooldown = false;
    xEventGroupSetBits (presenceEventGroup, PRESENCE_NEW_DATA_BIT);
}

void presence_detection_timer_callback( TimerHandle_t xTimer )
{
    ESP_LOGI(TAG, "detection timer expired, should disable presence status now");
    xEventGroupSetBits (presenceEventGroup, PRESENCE_TIMER_BIT);
}

void publish_presence_data()
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/presence/sw";

  char data[16];
  memset(data,0,16);
  sprintf(data, presence_detected ? "true" : "false");
  publish_non_persistent_data(topic, data);
}

void app_presence_task(void* pvParameters)
{
    ESP_LOGI(TAG, "app_presence task started");
    presenceEventGroup = xEventGroupCreate();
    /* Was the event group created successfully? */
    if( presenceEventGroup == NULL )
    {
        ESP_LOGE(TAG, "The event group was not created because there was insufficient \
                        FreeRTOS heap available");
        return;
    }

    presence_detection_timer = xTimerCreate( "presence_detection_timer",           /* Text name. */
                    pdMS_TO_TICKS(CONFIG_PRESENCE_AUTOMATION_DISABLE_TIMER * 1000),  /* Period. */
                    pdFALSE,                /* Autoreload. */
                    (void *)0,                  /* no ID. */
                    presence_detection_timer_callback );  /* Callback function. */
    if (presence_detection_timer == NULL) {
        ESP_LOGE(TAG, "No presence_detection_timer found");
        return;
    }

    presence_cooldown_timer = xTimerCreate( "presence_cooldown_timer",           /* Text name. */
                    pdMS_TO_TICKS(2 * 1000),  /* Period. */
                    pdFALSE,                /* Autoreload. */
                    (void *)0,                  /* no ID. */
                    presence_cooldown_timer_callback );  /* Callback function. */
    if (presence_cooldown_timer == NULL) {
        ESP_LOGE(TAG, "No presence_cooldown_timer found");
        return;
    }

    EventBits_t mBits;
    while(1) {
        mBits = xEventGroupWaitBits(presenceEventGroup, PRESENCE_NEW_DATA_BIT | PRESENCE_TIMER_BIT,
                                    true, false, portMAX_DELAY);
        if (mBits & PRESENCE_NEW_DATA_BIT) {
            if (presence_cooldown)
                continue;

            if (local_motion_detected
#ifdef CONFIG_BH1750_SENSOR
                && illuminance/100 < CONFIG_PRESENCE_AUTOMATION_ILLUMINANCE_THRESHOLD
#endif // CONFIG_BH1750_SENSOR
         ) {
                if (!presence_detected) {
                    presence_detected = true;
                    publish_presence_data();
                    struct RelayMessage r = {RELAY_CMD_STATUS, 0, RELAY_STATUS_ON};
                    if (xQueueSend( relayQueue,
                                    ( void * )&r,
                                    RELAY_QUEUE_TIMEOUT) != pdPASS) {
                    ESP_LOGE(TAG, "Cannot send to relayQueue");
                    }
                }
                if( xTimerStop(presence_detection_timer, 0 ) != pdPASS )
                {
                    ESP_LOGE(TAG, "Cannot stop presence_detection_timer");
                    continue;
                }
                ESP_LOGI(TAG, "Stopped presence_detection_timer");
            }
            else if (presence_detected) { // should stop with delay, start timer
                if( xTimerIsTimerActive( presence_detection_timer ) == pdFALSE ) {
                    if( xTimerStart(presence_detection_timer, 0 ) != pdPASS )
                    {
                        ESP_LOGE(TAG, "Cannot start presence_detection_timer");
                        continue;
                    }
                    ESP_LOGI(TAG, "Started presence_detection_timer");
                } else {
                    ESP_LOGI(TAG, "Timer already active: presence_detection_timer");
                }
            }
        } else if (mBits & PRESENCE_TIMER_BIT) {
            presence_detected = false;
            publish_presence_data();

            presence_cooldown = true;
            if( xTimerIsTimerActive( presence_cooldown_timer ) == pdFALSE ) {
                if( xTimerStart(presence_cooldown_timer, 0 ) != pdPASS )
                {
                    ESP_LOGE(TAG, "Cannot start presence_cooldown_timer");
                    continue;
                }
                ESP_LOGI(TAG, "Started presence_cooldown_timer");
            } else {
                ESP_LOGI(TAG, "Timer already active: presence_cooldown_timer");
            }

            struct RelayMessage r = {RELAY_CMD_STATUS, 0, RELAY_STATUS_OFF};
            if (xQueueSend( relayQueue,
                            ( void * )&r,
                            RELAY_QUEUE_TIMEOUT) != pdPASS) {
                ESP_LOGE(TAG, "Cannot send to relayQueue");
            }
        }
    }
}

#endif // CONFIG_PRESENCE_AUTOMATION_SUPPORT
