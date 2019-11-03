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

#endif /* APP_MAIN_H */
