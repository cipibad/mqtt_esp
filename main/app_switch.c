#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "app_esp8266.h"
#include "app_relay.h"

#include "app_switch.h"

#include "driver/gpio.h"


int switch_value=0;
int switch_gpio=0;


extern QueueHandle_t relayQueue;

static void gpio_isr_handler(void *arg)
{
  switch_value = ! switch_value;
  struct RelayMessage r={0, switch_value};
  xQueueSendFromISR(relayQueue
                    ,( void * )&r
                    ,NULL);
}

void gpio_switch_init (void *arg)
{
  gpio_config_t io_conf;
//interrupt of rising edge
  io_conf.intr_type = GPIO_INTR_POSEDGE;
  //bit mask of the pins, use GPIO4/5 here
  io_conf.pin_bit_mask = (1ULL << switch_gpio);
  //set as input mode
  io_conf.mode = GPIO_MODE_INPUT;
  gpio_config(&io_conf);


  //install gpio isr service
  gpio_install_isr_service(0);
  //hook isr handler for specific gpio pin
  gpio_isr_handler_add(switch_gpio, gpio_isr_handler, (void *) switch_gpio);
  //hook isr handler for specific gpio pin

}

