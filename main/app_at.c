#include "esp_system.h"
#define CONFIG_AT_SERVER 1

#ifdef CONFIG_AT_SERVER
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/uart.h"

#include "app_at.h"

#include <string.h>

static const char *TAG = "AT";

void app_at_task(void* pvParameters)
{

    #define BUF_SIZE (256)
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
    ESP_LOGI(TAG, "UART AT server ready");

    const char* ready = "ready\r\n";
    const char* ok = "OK\r\n";
    const char* wifi_connected = "WIFI CONNECTED\r\n";
    const char* wifi_got_ip = "WIFI GOT IP\r\n";
    const char* connect = "0,CONNECT\r\n";

    uart_write_bytes(UART_NUM_0, ready, strlen(ready));

    char data[BUF_SIZE];
    while (1) {
        int len = uart_read_bytes(UART_NUM_0, (uint8_t*) data, BUF_SIZE, 100 / portTICK_RATE_MS);
        if (len > 0) {
            data[len] = 0;
            ESP_LOGI(TAG, "Read: %d bytes: >%.*s<", len, len, data);

            if (strcmp("AT+CWSTARTSMART\r\n", data) == 0) {
                uart_write_bytes(UART_NUM_0, ok, strlen(ok));
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                uart_write_bytes(UART_NUM_0, ready, strlen(ready));
            }
            else if (strcmp("AT+RST\r\n", data) == 0) {
                uart_write_bytes(UART_NUM_0, ok, strlen(ok));
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                uart_write_bytes(UART_NUM_0, ready, strlen(ready));
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                uart_write_bytes(UART_NUM_0, wifi_connected, strlen(wifi_connected));
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                uart_write_bytes(UART_NUM_0, wifi_got_ip, strlen(wifi_got_ip));
            }
            else if (strcmp("AT+CWMODE=1\r\n", data) == 0) {
                uart_write_bytes(UART_NUM_0, ok, strlen(ok));
            }
            else if (strcmp("AT+CIPMUX=1\r\n", data) == 0) {
                uart_write_bytes(UART_NUM_0, ok, strlen(ok));
            }
            else if (strcmp("AT+CIPSERVER=1,8080\r\n", data) == 0) {
                uart_write_bytes(UART_NUM_0, ok, strlen(ok));
            }
            else if (strcmp("AT+CIPSTO=360\r\n", data) == 0) {
                uart_write_bytes(UART_NUM_0, ok, strlen(ok));
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                uart_write_bytes(UART_NUM_0, connect, strlen(connect));
            }
        }
    }
}

#endif // CONFIG_AT_SERVER