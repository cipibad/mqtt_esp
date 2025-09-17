#ifndef APP_AT_H
#define APP_AT_H

#define UART_BUF_SIZE 256

typedef enum at_status {
    AT_STATUS_INIT = 0,
    AT_STATUS_ONLINE,
    AT_STATUS_OFFLINE
} at_status_t;

void app_at_task(void* pvParameters);
bool is_serial_interface_online();

#endif /* APP_AT_H */
