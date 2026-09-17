#ifndef APP_BELL_H
#define APP_BELL_H

#include "mqtt_client.h"
#include <stdbool.h>

/*
 * School bell module. bell_init() creates the (tiny) command queue early;
 * the bell task and its buffers are created on the first MQTT connect
 * (post TLS handshake) to protect the boot-time heap watermark.
 */

void bell_init(void);
void bell_on_mqtt_connected(void);

/* Returns true when the event was a bell topic and has been consumed. */
bool bell_handle_mqtt_event(esp_mqtt_event_handle_t event);

#endif /* APP_BELL_H */
