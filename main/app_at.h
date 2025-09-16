#ifndef APP_AT_H
#define APP_AT_H

#define UART_BUF_SIZE 256

void app_at_task(void* pvParameters);
bool is_serial_interface_online();

#endif /* APP_AT_H */
