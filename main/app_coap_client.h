#ifndef APP_COAP_CLIENT_H
#define APP_COAP_CLIENT_H

void coap_client_thread(void *p);
void coap_publish_data(const char * topic,
                       const char * data);



#endif /* APP_COAP_CLIENT_H */
