#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


#include "app_switch.h"
#include "app_smart_config.h"

#include "driver/gpio.h"

extern QueueHandle_t smartconfigQueue;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
#if CONFIG_MQTT_SWITCHES_NB

static void gpio_isr_handler(void *arg)
{
  struct SmartConfigMessage scm;
  scm.ticks = xTaskGetTickCountFromISR();
  scm.relayId = (int) arg;
  xQueueSendFromISR(smartconfigQueue
                    ,( void * )&scm
                    ,NULL);

}

void gpio_switch_init (void *arg)
{
  gpio_config_t io_conf;
//interrupt of rising edge
  io_conf.intr_type = GPIO_INTR_ANYEDGE;
  //bit mask of the pins, use GPIO4/5 here
  io_conf.pin_bit_mask = (1ULL << CONFIG_MQTT_SWITCHES_NB0_GPIO);
#if CONFIG_MQTT_SWITCHES_NB > 1
  io_conf.pin_bit_mask |= (1ULL << CONFIG_MQTT_SWITCHES_NB1_GPIO);
#if CONFIG_MQTT_SWITCHES_NB > 2
  io_conf.pin_bit_mask |= (1ULL << CONFIG_MQTT_SWITCHES_NB2_GPIO);
#if CONFIG_MQTT_SWITCHES_NB > 3
  io_conf.pin_bit_mask |= (1ULL << CONFIG_MQTT_SWITCHES_NB3_GPIO);
#endif
#endif
#endif
  //set as input mode
  io_conf.mode = GPIO_MODE_INPUT;
  gpio_config(&io_conf);

  //install gpio isr service
  gpio_install_isr_service(0);
  //hook isr handler for specific gpio pin
  gpio_isr_handler_add(CONFIG_MQTT_SWITCHES_NB0_GPIO, gpio_isr_handler, (void *) 0);
#if CONFIG_MQTT_RELAYS_NB > 1 && CONFIG_MQTT_SWITCHES_NB > 1
  gpio_isr_handler_add(CONFIG_MQTT_SWITCHES_NB1_GPIO, gpio_isr_handler, (void *) 1);
#if CONFIG_MQTT_RELAYS_NB > 2 && CONFIG_MQTT_SWITCHES_NB > 2
  gpio_isr_handler_add(CONFIG_MQTT_SWITCHES_NB2_GPIO, gpio_isr_handler, (void *) 2);
#if CONFIG_MQTT_RELAYS_NB > 3 && CONFIG_MQTT_SWITCHES_NB > 3
  gpio_isr_handler_add(CONFIG_MQTT_SWITCHES_NB3_GPIO, gpio_isr_handler, (void *) 3);
#endif
#endif
#endif
  //hook isr handler for specific gpio pin
}

#endif //CONFIG_MQTT_SWITCHES_NB

