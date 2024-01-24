#include "esp_system.h"
#ifdef CONFIG_MOTION_SENSOR_SUPPORT

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"

#include "esp_log.h"

#include "app_motion.h"
#include "app_publish_data.h"

#define portYIELD_FROM_ISR() taskYIELD()
static const char *TAG = "MOTION";

EventGroupHandle_t motionEventGroup;
const int MOTION_GPIO_INTR_BIT = BIT0;
const int MOTION_TIMER_BIT = BIT1;

bool motion_detected = false;
TimerHandle_t motion_detection_timer = NULL;


static void gpio_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = false;
    xEventGroupSetBitsFromISR (motionEventGroup, MOTION_GPIO_INTR_BIT, &xHigherPriorityTaskWoken);

    if( xHigherPriorityTaskWoken )
        {
            /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context
            switch should be requested.  The macro used is port specific and will
            be either portYIELD_FROM_ISR() or portEND_SWITCHING_ISR() - refer to
            the documentation page for the port being used. */
            portYIELD_FROM_ISR();
        }
}

void motion_detection_timer_callback( TimerHandle_t xTimer )
{
    ESP_LOGI(TAG, "timer expired, should disable motion status now");
    xEventGroupSetBits (motionEventGroup, MOTION_TIMER_BIT);
}

void publish_motion_data()
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/motion/hc-sr501";

  char data[16];
  memset(data,0,16);
  sprintf(data, motion_detected ? "true" : "false");
  publish_non_persistent_data(topic, data);

#ifdef CONFIG_PRESENCE_AUTOMATION_SUPPORT
  extern EventGroupHandle_t presenceEventGroup;
  extern const int PRESENCE_NEW_DATA_BIT;
  if( presenceEventGroup != NULL ) {
    xEventGroupSetBits (presenceEventGroup, PRESENCE_NEW_DATA_BIT);
  }
#endif // CONFIG_PRESENCE_AUTOMATION_SUPPORT
}

void app_motion_task(void* pvParameters)
{
    ESP_LOGI(TAG, "app_motion task started");
    motionEventGroup = xEventGroupCreate();
    /* Was the event group created successfully? */
    if( motionEventGroup == NULL )
    {
        ESP_LOGE(TAG, "The event group was not created because there was insufficient \
                        FreeRTOS heap available");
        return;
    }

    motion_detection_timer = xTimerCreate( "motion_detection_timer",           /* Text name. */
                    pdMS_TO_TICKS(CONFIG_MOTION_SENSOR_DISABLE_TIMER * 1000),  /* Period. */
                    pdFALSE,                /* Autoreload. */
                    (void *)0,                  /* no ID. */
                    motion_detection_timer_callback );  /* Callback function. */
    if (motion_detection_timer == NULL) {
        ESP_LOGE(TAG, "No motion_detection_timer found");
        return;
    }

    ESP_LOGI(TAG, "event group created, waiting sensor initialization");
    gpio_config_t io_conf;
    //interrupt of any edge

    // Sensor module is powered up after a minute, in this initialization time intervals
    // during this module will output 0-3 times, a minute later enters the standby state.
    // only 10 seconds delay seems to be enough
    vTaskDelay(10 * 1000 / portTICK_PERIOD_MS);

    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = (1ULL << CONFIG_MOTION_SENSOR_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf);

    //install gpio isr service
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(CONFIG_MOTION_SENSOR_GPIO, gpio_isr_handler, (void *) NULL);
    ESP_LOGI(TAG, "gpio %d configured", CONFIG_MOTION_SENSOR_GPIO);

    EventBits_t mBits;
    while(1) {
        mBits = xEventGroupWaitBits(motionEventGroup, MOTION_GPIO_INTR_BIT | MOTION_TIMER_BIT,
                                    true, false, portMAX_DELAY);
        if (mBits & MOTION_GPIO_INTR_BIT) {
            bool new_motion_detected = gpio_get_level(CONFIG_MOTION_SENSOR_GPIO);
            ESP_LOGI(TAG, "new_motion_detected: %d", new_motion_detected);

            if (new_motion_detected){
                if (! motion_detected) {
                    motion_detected = true;
                    publish_motion_data();
                } else {
                    if( xTimerStop(motion_detection_timer, 0 ) != pdPASS )
                    {
                        ESP_LOGE(TAG, "Cannot stop motion_detection_timer");
                        continue;
                    }
                    ESP_LOGI(TAG, "Stopped motion_detection_timer");
                }
            } else if (motion_detected) {
                if( xTimerStart(motion_detection_timer, 0 ) != pdPASS )
                {
                    ESP_LOGE(TAG, "Cannot start motion_detection_timer");
                    continue;
                }
                ESP_LOGI(TAG, "Started motion_detection_timer");
            }
        } else if (mBits & MOTION_TIMER_BIT) {
            motion_detected = false;
            publish_motion_data();
        }
    }
}

#endif // CONFIG_MOTION_SENSOR_SUPPORT
