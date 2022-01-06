#ifndef APP_COAP_CLIENT_H
#define APP_COAP_CLIENT_H

#define COAP_QUEUE_TIMEOUT (60 * 1000 / portTICK_PERIOD_MS)

void coap_client_thread(void *p);
void coap_publish_data(const char * topic,
                       const char * data);

struct CoapMessage
{
  char resource[48];
  char data[16];
};

#endif /* APP_COAP_CLIENT_H */
