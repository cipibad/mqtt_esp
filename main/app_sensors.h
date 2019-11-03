#ifndef APP_SENSORS_H
#define APP_SENSORS_H

void sensors_task(void* pvParameters);
void publish_sensors_data();

enum SensorMsgType {
  SENSOR_MQTT_CONNECTED=1,
  SENSOR_READ
};

struct SensorMessage
{
  enum SensorMsgType msgType;
};

#endif /* APP_SENSORS_H */
