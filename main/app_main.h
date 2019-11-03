#ifndef APP_MAIN_H
#define APP_MAIN_H

#if CONFIG_ESPTOOLPY_FLASHSIZE_1MB
#define RELAY_ON 1
#define RELAY_OFF 0
#else
#define RELAY_ON 0
#define RELAY_OFF 1
#endif//CONFIG_ESPTOOLPY_FLASHSIZE_1MB

#define LED_ON 0
#define LED_OFF 1

//FIXME remove mqtt references when mqtt refactoring done
#define MQTT_TIMEOUT 30
#define MQTT_FLAG_TIMEOUT (MQTT_TIMEOUT * 1000 / portTICK_PERIOD_MS)
#define MQTT_QUEUE_TIMEOUT (MQTT_TIMEOUT * 1000 / portTICK_PERIOD_MS)
#define MQTT_MAX_TOPIC_LEN 64


#endif /* APP_MAIN_H */
