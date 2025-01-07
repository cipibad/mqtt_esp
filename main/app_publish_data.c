#include "esp_system.h"

#include "app_publish_data.h"
#ifdef CONFIG_NORTH_INTERFACE_MQTT
#include "app_mqtt.h"
#endif // CONFIG_NORTH_INTERFACE_MQTT

#ifdef CONFIG_NORTH_INTERFACE_COAP
#include "app_coap_client.h"
#endif // CONFIG_NORTH_INTERFACE_COAP

#ifdef CONFIG_NORTH_INTERFACE_UDP
#include "app_udp_common.h"
#endif // CONFIG_NORTH_INTERFACE_UDP

void publish_non_persistent_data(const char * topic, const char * data)
{
#ifdef CONFIG_NORTH_INTERFACE_MQTT
mqtt_publish_data(topic, data, QOS_0, NO_RETAIN);
#endif // CONFIG_NORTH_INTERFACE_MQTT

#ifdef CONFIG_NORTH_INTERFACE_COAP
coap_publish_data(topic, data);
#endif // CONFIG_NORTH_INTERFACE_COAP

#ifdef CONFIG_NORTH_INTERFACE_UDP
udp_publish_data(topic, data, QOS_0, NO_RETAIN);
#endif // CONFIG_NORTH_INTERFACE_UDP
}

void publish_persistent_data(const char * topic, const char * data)
{
#ifdef CONFIG_NORTH_INTERFACE_MQTT
mqtt_publish_data(topic, data, QOS_1, RETAIN);
#endif // CONFIG_NORTH_INTERFACE_MQTT

#ifdef CONFIG_NORTH_INTERFACE_COAP
coap_publish_data(topic, data);
#endif // CONFIG_NORTH_INTERFACE_COAP

#ifdef CONFIG_NORTH_INTERFACE_UDP
udp_publish_data(topic, data, QOS_1, RETAIN);
#endif // CONFIG_NORTH_INTERFACE_UDP
}


