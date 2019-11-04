#ifndef APP_MQTT_H
#define APP_MQTT_H

/* some useful values for relay Json exchanges */
#define MAX_MQTT_DATA_LEN_RELAY 32
#define MAX_MQTT_DATA_THERMOSTAT 64
#define MAX_MQTT_DATA_SCHEDULER 96
#define MAX_MQTT_DATA_SENSORS 256
#define JSON_BAD_RELAY_VALUE 255
#define JSON_BAD_TOPIC_ID 255

void mqtt_init_and_start();
void handle_mqtt_sub_pub(void* pvParameters);


#define MQTT_TIMEOUT 10
#define MQTT_FLAG_TIMEOUT (MQTT_TIMEOUT * 1000 / portTICK_PERIOD_MS)
#define MQTT_QUEUE_TIMEOUT (MQTT_TIMEOUT * 1000 / portTICK_PERIOD_MS)
#define MQTT_MAX_TOPIC_LEN 64


#define QOS_0 0
#define QOS_1 1
#define NO_RETAIN 0
#define RETAIN 1

void mqtt_publish_data(const char * topic,
                       const char * data,
                       int qos, int retain);




#endif /* APP_MQTT_H */
