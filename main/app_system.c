#include "esp_system.h"
#include "app_logging.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "system"

void system_restart(void){
    LOGI(TAG, LOG_MODULE_SYSTEM, "Restarting in 5 seconds following system command");
    vTaskDelay(5 * 1000 / portTICK_PERIOD_MS);
    esp_restart();
}