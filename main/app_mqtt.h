#ifndef APP_MQTT_H
#define APP_MQTT_H

#define MQTT_TIMEOUT 30
#define MQTT_FLAG_TIMEOUT (MQTT_TIMEOUT * 1000 / portTICK_PERIOD_MS)
#define MQTT_QUEUE_TIMEOUT (MQTT_TIMEOUT * 1000 / portTICK_PERIOD_MS)

#define MQTT_MAX_TOPIC_LEN 64
#define MAX_MQTT_PUBLISH_DATA 256

//FIXME removed others MAX data or rename to receive
#define MAX_MQTT_DATA_LEN_RELAY 32
#define MAX_MQTT_DATA_THERMOSTAT 64
#define MAX_MQTT_DATA_SCHEDULER 96
#define MAX_MQTT_DATA_SENSORS 256

/* some useful values for relay Json exchanges */
#define JSON_BAD_RELAY_VALUE 255
#define JSON_BAD_TOPIC_ID 255

void mqtt_init_and_start();
void handle_mqtt_sub_pub(void* pvParameters);

#define QOS_0 0
#define QOS_1 1
#define RETAIN 1

enum MqttMsgType {
  MQTT_CONNECTED = 1,
  MQTT_PUBLISH = 2
};

struct MqttPublishData {
  char topic[MQTT_MAX_TOPIC_LEN];
  char data[MAX_MQTT_PUBLISH_DATA];
  int qos;
  int retain;
};

struct MqttMessage {
  enum MqttMsgType msgType;
  struct MqttPublishData publishData;
};

#endif /* APP_MQTT_H */
