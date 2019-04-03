#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


#include "app_switch.h"

#include "driver/gpio.h"

extern QueueHandle_t smartconfigQueue;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */

#if CONFIG_MQTT_SWITCHES_NB

static void gpio_isr_handler(void *arg)
{
  int n=xTaskGetTickCountFromISR();
  xQueueSendFromISR(smartconfigQueue
                    ,( void * )&n
                    ,NULL);

}

void gpio_switch_init (void *arg)
{
  gpio_config_t io_conf;
//interrupt of rising edge
  io_conf.intr_type = GPIO_INTR_ANYEDGE;
  //bit mask of the pins, use GPIO4/5 here
  io_conf.pin_bit_mask = (1ULL << CONFIG_MQTT_SWITCHES_NB0_GPIO);
  //set as input mode
  io_conf.mode = GPIO_MODE_INPUT;
  gpio_config(&io_conf);

  //install gpio isr service
  gpio_install_isr_service(0);
  //hook isr handler for specific gpio pin
  gpio_isr_handler_add(CONFIG_MQTT_SWITCHES_NB0_GPIO, gpio_isr_handler, (void *) CONFIG_MQTT_SWITCHES_NB0_GPIO);
  //hook isr handler for specific gpio pin
}

#endif //CONFIG_MQTT_SWITCHES_NB

