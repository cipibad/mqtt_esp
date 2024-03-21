#include "esp_system.h"
#ifdef CONFIG_NORTH_INTERFACE_HTTP

#include <esp_log.h>

#include "app_actuator.h"
#include "app_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "http_server";
httpd_handle_t server = NULL;

extern const char index_css_start[] asm("_binary_index_css_start");
extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_js_start[] asm("_binary_index_js_start");

esp_err_t index_html_get_handler(httpd_req_t *req) {
  httpd_resp_send(req, index_html_start, strlen(index_html_start));
  return ESP_OK;
}
httpd_uri_t get_index_html = {
    .uri = "/", .method = HTTP_GET, .handler = index_html_get_handler};

esp_err_t index_css_get_handler(httpd_req_t *req) {
  httpd_resp_send(req, index_css_start, strlen(index_css_start));
  return ESP_OK;
}
httpd_uri_t get_index_css = {
    .uri = "/index.css", .method = HTTP_GET, .handler = index_css_get_handler};

esp_err_t index_js_get_handler(httpd_req_t *req) {
  httpd_resp_send(req, index_js_start, strlen(index_js_start));
  return ESP_OK;
}
httpd_uri_t get_index_js = {
    .uri = "/index.js", .method = HTTP_GET, .handler = index_js_get_handler};

#ifdef CONFIG_DHT22_SENSOR_SUPPORT
extern short dht22_mean_temperature;
esp_err_t temperature_dht22_get_handler(httpd_req_t *req) {
  char data[16];
  memset(data, 0, 16);
  sprintf(data, "%d.%d", dht22_mean_temperature / 10,
          abs(dht22_mean_temperature % 10));
  httpd_resp_send(req, data, strlen(data));
  return ESP_OK;
}
httpd_uri_t get_temperature_dht22 = {.uri = "/temperature/dht22",
                                     .method = HTTP_GET,
                                     .handler = temperature_dht22_get_handler};

extern short dht22_mean_humidity;
esp_err_t humidity_dht22_get_handler(httpd_req_t *req) {
  char data[16];
  memset(data, 0, 16);
  sprintf(data, "%d.%d", dht22_mean_humidity / 10,
          abs(dht22_mean_humidity % 10));
  httpd_resp_send(req, data, strlen(data));
  return ESP_OK;
}
httpd_uri_t get_humidity_dht22 = {.uri = "/humidity/dht22",
                                  .method = HTTP_GET,
                                  .handler = humidity_dht22_get_handler};
#endif  // CONFIG_DHT22_SENSOR_SUPPORT

#ifdef CONFIG_ACTUATOR_SUPPORT

esp_err_t actuator_action_post_handler(httpd_req_t *req) {
  char buf[100];
  int ret, remaining = req->content_len;
  if (remaining > 100) {
    ESP_LOGI(TAG, "actuator_action_post_handler: payload too big");
    return ESP_FAIL;
  }
  while (remaining > 0) {
    /* Read the data for the request */
    if ((ret = httpd_req_recv(req, buf, remaining)) <= 0) {
      if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
        /* Retry receiving if timeout occurred */
        continue;
      }
      ESP_LOGI(TAG, "actuator_action_post_handler: unexpected error");
      return ESP_FAIL;
    }
    remaining -= ret;

    /* Log data received */
    ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
    ESP_LOGI(TAG, "%.*s", ret, buf);
    ESP_LOGI(TAG, "====================================");
  }

  // TODO parse command and period (similar to MQTT context of somehow)
  updateActuatorState(ACTUATOR_COMMAND_OPEN, ACTUATOR_PERIOD_FULL);
  const char *result_ok = "{\"result\": \"ok\"}";
  httpd_resp_send(req, result_ok, strlen(result_ok));
  return ESP_OK;
}

httpd_uri_t post_action_actuator = {.uri = "/action/actuator",
                                    .method = HTTP_POST,
                                    .handler = actuator_action_post_handler,
                                    .user_ctx = NULL};

extern ActuatorStatus_t actuatorStatus;
esp_err_t actuator_status_get_handler(httpd_req_t *req) {
  char data[16];
  memset(data, 0, 16);
  sprintf(data, "%s",
          actuatorStatus == ACTUATOR_STATUS_INIT               ? "Init"
          : actuatorStatus == ACTUATOR_STATUS_OFF              ? "Off"
          : actuatorStatus == ACTUATOR_STATUS_OPEN_TRANSITION  ? "Openning"
          : actuatorStatus == ACTUATOR_STATUS_CLOSE_TRANSITION ? "Closing"
                                                               : "Unknown");
  httpd_resp_send(req, data, strlen(data));
  return ESP_OK;
}
httpd_uri_t get_status_actuator = {.uri = "/status/actuator",
                                   .method = HTTP_GET,
                                   .handler = actuator_status_get_handler};

extern ActuatorStatus_t actuatorLevel;
esp_err_t actuator_level_get_handler(httpd_req_t *req) {
  char data[16];
  memset(data, 0, 16);
  sprintf(data, "%d", actuatorLevel);
  httpd_resp_send(req, data, strlen(data));
  return ESP_OK;
}
httpd_uri_t get_level_actuator = {.uri = "/level/actuator",
                                   .method = HTTP_GET,
                                   .handler = actuator_level_get_handler};

#endif  // CONFIG_ACTUATOR_SUPPORT

httpd_handle_t start_webserver(void) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  // Start the httpd server
  ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK) {
    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &get_index_css);
    httpd_register_uri_handler(server, &get_index_html);
    httpd_register_uri_handler(server, &get_index_js);
#ifdef CONFIG_DHT22_SENSOR_SUPPORT
    httpd_register_uri_handler(server, &get_temperature_dht22);
    httpd_register_uri_handler(server, &get_humidity_dht22);
#endif  // CONFIG_DHT22_SENSOR_SUPPORT
#ifdef CONFIG_ACTUATOR_SUPPORT
    httpd_register_uri_handler(server, &post_action_actuator);
    httpd_register_uri_handler(server, &get_status_actuator);
    httpd_register_uri_handler(server, &get_level_actuator);
#endif  // CONFIG_ACTUATOR_SUPPORT

    return server;
  }

  ESP_LOGI(TAG, "Error starting server!");
  return NULL;
}

void stop_webserver(httpd_handle_t server) {
  // Stop the httpd server
  httpd_stop(server);
}

void http_thread(void *p) {
  while (1) {
  }

  vTaskDelete(NULL);
}
#endif  // CONFIG_NORTH_INTERFACE_HTTP
