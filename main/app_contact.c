#include "esp_system.h"
#ifdef CONFIG_CONTACT_SENSOR_SUPPORT

#include <string.h>

#include "app_contact.h"
#include "app_publish_data.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define portYIELD_FROM_ISR() taskYIELD()
static const char *TAG = "CONTACT";

EventGroupHandle_t contactEventGroup;
const int CONTACT_GPIO_INTR_BIT = BIT0;

bool contact_open = false;

static void gpio_isr_handler(void *arg) {
  BaseType_t xHigherPriorityTaskWoken = false;
  xEventGroupSetBitsFromISR(contactEventGroup, CONTACT_GPIO_INTR_BIT,
                            &xHigherPriorityTaskWoken);

  if (xHigherPriorityTaskWoken) {
    /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context
    switch should be requested.  The macro used is port specific and will
    be either portYIELD_FROM_ISR() or portEND_SWITCHING_ISR() - refer to
    the documentation page for the port being used. */
    portYIELD_FROM_ISR();
  }
}
void publish_contact_data() {
  const char *topic =
      CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/contact/magnet";

  char data[16];
  memset(data, 0, 16);
  sprintf(data, contact_open ? "OFF" : "ON");
  publish_non_persistent_data(topic, data);
}

void app_contact_task(void *pvParameters) {
  ESP_LOGI(TAG, "app_contact_task started");
  contactEventGroup = xEventGroupCreate();
  /* Was the event group created successfully? */
  if (contactEventGroup == NULL) {
    ESP_LOGE(TAG,
             "The event group was not created because there was insufficient \
                        FreeRTOS heap available");
    return;
  }

  gpio_config_t io_conf;
  // interrupt of any edge
  io_conf.intr_type = GPIO_INTR_ANYEDGE;
  // bit mask of the pins, use GPIO4/5 here
  io_conf.pin_bit_mask = (1ULL << CONFIG_CONTACT_SENSOR_GPIO);
  io_conf.mode = GPIO_MODE_INPUT;

#ifndef CONFIG_CONTACT_SENSOR_GPIO_PULL_NONE
  gpio_pulldown_t config_gpio_pull_down = GPIO_PULLDOWN_DISABLE;
#ifdef CONFIG_CONTACT_SENSOR_GPIO_PULL_DOWN
  config_gpio_pull_down = GPIO_PULLDOWN_ENABLE;
#endif  // CONFIG_CONTACT_SENSOR_GPIO_PULL_DOWN
  io_conf.pull_down_en = config_gpio_pull_down;

  gpio_pullup_t config_gpio_pull_up = GPIO_PULLUP_DISABLE;
#ifdef CONFIG_CONTACT_SENSOR_GPIO_PULL_UP
  config_gpio_pull_up = GPIO_PULLUP_ENABLE;
#endif  // CONFIG_CONTACT_SENSOR_GPIO_PULL_UP
  io_conf.pull_up_en = config_gpio_pull_up;
#endif  // CONFIG_CONTACT_SENSOR_GPIO_PULL_NONE

  gpio_config(&io_conf);

  // install gpio isr service
  gpio_install_isr_service(0);
  // hook isr handler for specific gpio pin
  gpio_isr_handler_add(CONFIG_CONTACT_SENSOR_GPIO, gpio_isr_handler,
                       (void *)NULL);
  ESP_LOGI(TAG, "gpio %d configured", CONFIG_CONTACT_SENSOR_GPIO);

  contact_open = gpio_get_level(CONFIG_CONTACT_SENSOR_GPIO);

  EventBits_t mBits;
  while (1) {
    mBits = xEventGroupWaitBits(contactEventGroup, CONTACT_GPIO_INTR_BIT, true,
                                false, portMAX_DELAY);
    if (mBits & CONTACT_GPIO_INTR_BIT) {
      bool new_contact_open = gpio_get_level(CONFIG_CONTACT_SENSOR_GPIO);
      if (new_contact_open != contact_open) {
        contact_open = new_contact_open;
        ESP_LOGI(TAG, "contact change detection: %d", contact_open);
        publish_contact_data();
      }
    }
  }
}
#endif  // CONFIG_CONTACT_SENSOR_SUPPORT
