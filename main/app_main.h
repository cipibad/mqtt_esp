#ifndef APP_MAIN_H
#define APP_MAIN_H

#if defined(CONFIG_DEVICE_TYPE_ESP32)
    #define GPIO_HIGH 0
    #define GPIO_LOW 1
#elif defined(CONFIG_DEVICE_TYPE_ESP12F)
    #define GPIO_HIGH 1
    #define GPIO_LOW 0
#else // CONFIG_DEVICE_TYPE_<others>

    #if CONFIG_MQTT_THERMOSTATS_NB > 0
        // this is hack for very specific usage where
        // esp8266 is used to control external relays box (like it is used for thermostat)
        // should dedicate specific device type for it
        #define GPIO_HIGH 0
        #define GPIO_LOW 1
    #else // CONFIG_MQTT_THERMOSTATS_NB > 0
        #define GPIO_HIGH 1
        #define GPIO_LOW 0
    #endif // CONFIG_MQTT_THERMOSTATS_NB > 0
#endif // CONFIG_DEVICE_TYPE_<others>




#define LED_ON 0
#define LED_OFF 1

#endif /* APP_MAIN_H */
