#include "esp_system.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "system"

void system_restart(void){
    ESP_LOGI(TAG, "Restarting in 5 seconds following system command");
    vTaskDelay(5 * 1000 / portTICK_PERIOD_MS);
    esp_restart();
}