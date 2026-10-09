#ifndef FREERTOS_H
#define FREERTOS_H
#include <stdbool.h>
#include <stdio.h>

typedef int esp_err_t;

//matches the esp32 target value, sizes are irrelevant on host, the
//stack is never allocated because xTaskCreate is stubbed
#define configMINIMAL_STACK_SIZE 1536

void ESP_ERROR_CHECK(int a);


#endif /* FREERTOS_H */

