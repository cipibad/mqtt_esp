#ifndef APP_COAP_CLIENT_H
#define APP_COAP_CLIENT_H

#include "app_coap.h"

#define COAP_QUEUE_TIMEOUT (60 * 1000 / portTICK_PERIOD_MS)

void coap_client_thread(void *p);
void coap_publish_data(const char * topic,
                       const char * data);

struct CoapMessage
{
  char resource[COAP_MAX_RESOURCE_SIZE];
  char data[COAP_MAX_DATA_SIZE];
};

#endif /* APP_COAP_CLIENT_H */
