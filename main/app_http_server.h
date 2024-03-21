#ifndef APP_HTTP_SERVER_H
#define APP_HTTP_SERVER_H

#include <esp_http_server.h>

httpd_handle_t start_webserver(void);
void stop_webserver(httpd_handle_t server);

#endif /* APP_HTTP_SERVER_H */
