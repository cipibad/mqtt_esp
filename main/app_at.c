#include "esp_system.h"
#ifdef CONFIG_AT_SERVER
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/uart.h"

#include "app_at.h"
#include "app_relay.h"

#include <string.h>

#define BUF_SIZE (256)

static const char *TAG = "AT";
static const char* ready = "\r\nready\r\n";
static const char* ok = "OK\r\n";
static const char* wifi_connected = "WIFI CONNECTED\r\n";
static const char* wifi_got_ip = "WIFI GOT IP\r\n";
static const char* connect = "0,CONNECT\r\n";

bool serial_relays_enabled = true;

void disable_serial_relays()
{
    if (serial_relays_enabled) {
        serial_relays_enabled = false;
        publish_all_relays_availability();
    }
}

void enable_serial_relays()
{
    if (!serial_relays_enabled) {
        serial_relays_enabled = true;
        publish_all_relays_availability();
    }
}

bool is_serial_interface_online()
{
    return serial_relays_enabled;
}

void handle_at_cmd(char* at_cmd) {
    char data[BUF_SIZE];
    sprintf(data, "%s\r\n", at_cmd);
    uart_write_bytes(UART_NUM_0, data, strlen(data));

    if (strcmp("AT+CWSTARTSMART", at_cmd) == 0) {
        uart_write_bytes(UART_NUM_0, ok, strlen(ok));
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        uart_write_bytes(UART_NUM_0, ready, strlen(ready));
        disable_serial_relays();
    }
    else if (strcmp("AT+RST", at_cmd) == 0) {
        uart_write_bytes(UART_NUM_0, ok, strlen(ok));
        vTaskDelay(100 / portTICK_PERIOD_MS);
        uart_write_bytes(UART_NUM_0, ready, strlen(ready));
        vTaskDelay(100 / portTICK_PERIOD_MS);
        uart_write_bytes(UART_NUM_0, wifi_connected, strlen(wifi_connected));
        vTaskDelay(100 / portTICK_PERIOD_MS);
        uart_write_bytes(UART_NUM_0, wifi_got_ip, strlen(wifi_got_ip));
        disable_serial_relays();
    }
    else if (strcmp("AT+CWMODE=1", at_cmd) == 0) {
        uart_write_bytes(UART_NUM_0, ok, strlen(ok));
        disable_serial_relays();
    }
    else if (strcmp("AT+CWMODE=2", at_cmd) == 0) {
        uart_write_bytes(UART_NUM_0, ok, strlen(ok));
        disable_serial_relays();
    }
    else if (strcmp("AT+CIPMUX=1", at_cmd) == 0) {
        uart_write_bytes(UART_NUM_0, ok, strlen(ok));
        disable_serial_relays();
    }
    else if (strcmp("AT+CIPSERVER=1,8080", at_cmd) == 0) {
        uart_write_bytes(UART_NUM_0, ok, strlen(ok));
        disable_serial_relays();
    }
    else if (strcmp("AT+CIPSTO=360", at_cmd) == 0) {
        uart_write_bytes(UART_NUM_0, ok, strlen(ok));
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        uart_write_bytes(UART_NUM_0, connect, strlen(connect));
        enable_serial_relays();
    }
}
void app_at_task(void* pvParameters)
{
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

    uart_write_bytes(UART_NUM_0, ready, strlen(ready));
    vTaskDelay(100 / portTICK_PERIOD_MS);
    uart_write_bytes(UART_NUM_0, connect, strlen(connect));

    char data[BUF_SIZE];
    while (1) {
        int len = uart_read_bytes(UART_NUM_0, (uint8_t*) data, BUF_SIZE, 100 / portTICK_RATE_MS);
        if (len > 0) {
            data[len] = 0;
            ESP_LOGI(TAG, "Read: %d bytes: >%.*s<", len, len, data);
            char *token = strtok(data, "\r\n");
            while(token) {
                handle_at_cmd(token);
                token = strtok(NULL, "\r\n");
            }
        }
    }
}

#endif // CONFIG_AT_SERVER