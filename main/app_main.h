#ifndef APP_MAIN_H
#define APP_MAIN_H

#if CONFIG_ESPTOOLPY_FLASHSIZE_1MB
#if CONFIG_MQTT_THERMOSTATS_NB > 0
// this is hack for very specific usage where
// esp8266 is used to control external relays box (like it is used for thermostat)
#define RELAY_ON 0
#define RELAY_OFF 1
#else // CONFIG_MQTT_THERMOSTATS_NB > 0
#define RELAY_ON 1
#define RELAY_OFF 0
#endif // CONFIG_MQTT_THERMOSTATS_NB > 0


#else //CONFIG_ESPTOOLPY_FLASHSIZE_1MB
#define RELAY_ON 0
#define RELAY_OFF 1
#endif//CONFIG_ESPTOOLPY_FLASHSIZE_1MB

#define LED_ON 0
#define LED_OFF 1

#endif /* APP_MAIN_H */
