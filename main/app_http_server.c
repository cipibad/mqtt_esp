#include "esp_system.h"
#ifdef CONFIG_NORTH_INTERFACE_HTTP

#include <esp_log.h>

#include "app_actuator.h"
#include "app_http_server.h"
#include "cJSON.h"
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
httpd_uri_t get_index_html = {.uri = CONFIG_HTTP_BASE_URL "/",
                              .method = HTTP_GET,
                              .handler = index_html_get_handler};

esp_err_t index_css_get_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/css");
  httpd_resp_send(req, index_css_start, strlen(index_css_start));
  return ESP_OK;
}
httpd_uri_t get_index_css = {.uri = CONFIG_HTTP_BASE_URL "/index.css",
                             .method = HTTP_GET,
                             .handler = index_css_get_handler};

esp_err_t index_js_get_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/javascript");
  httpd_resp_send(req, index_js_start, strlen(index_js_start));
  return ESP_OK;
}
httpd_uri_t get_index_js = {.uri = CONFIG_HTTP_BASE_URL "/index.js",
                            .method = HTTP_GET,
                            .handler = index_js_get_handler};

#ifdef CONFIG_DHT22_SENSOR_SUPPORT
extern short dht22_mean_temperature;
esp_err_t temperature_dht22_get_handler(httpd_req_t *req) {
  char data[16];
  memset(data, 0, 16);
  sprintf(data, "{\"value\":%d.%d}", dht22_mean_temperature / 10,
          abs(dht22_mean_temperature % 10));
  httpd_resp_set_type(req, HTTPD_TYPE_JSON);
  httpd_resp_send(req, data, strlen(data));
  return ESP_OK;
}
httpd_uri_t get_temperature_dht22 = {.uri = CONFIG_HTTP_BASE_URL "/temperature/dht22",
                                     .method = HTTP_GET,
                                     .handler = temperature_dht22_get_handler};

extern short dht22_mean_humidity;
esp_err_t humidity_dht22_get_handler(httpd_req_t *req) {
  char data[16];
  memset(data, 0, 16);
  sprintf(data, "{\"value\":%d.%d}", dht22_mean_humidity / 10,
          abs(dht22_mean_humidity % 10));
  httpd_resp_set_type(req, HTTPD_TYPE_JSON);
  httpd_resp_send(req, data, strlen(data));
  return ESP_OK;
}
httpd_uri_t get_humidity_dht22 = {.uri = CONFIG_HTTP_BASE_URL "/humidity/dht22",
                                  .method = HTTP_GET,
                                  .handler = humidity_dht22_get_handler};
#endif  // CONFIG_DHT22_SENSOR_SUPPORT

#ifdef CONFIG_ACTUATOR_SUPPORT

esp_err_t actuator_action_post_handler(httpd_req_t *req) {
  char buf[64];
  if (req->content_len > 63) {
    ESP_LOGI(TAG, "actuator_action_post_handler: payload too big");
    return ESP_FAIL;
  }

  int ret, remaining = req->content_len;
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
    ESP_LOGI(TAG, "received: %.*s", ret, buf);
  }
  buf[req->content_len] = 0;

  ActuatorCommand_t command = ACTUATOR_COMMAND_UNSET;
  ActuatorPeriod_t period = ACTUATOR_PERIOD_UNSET;
  cJSON *json = cJSON_Parse(buf);
  if (json == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      ESP_LOGE(TAG, "Error before: %s\n", error_ptr);
    }
    const char *result_ok =
        "{\"result\": \"nok\", \"error\": \"Cannot parse json\"}";
    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    httpd_resp_send(req, result_ok, strlen(result_ok));
    return ESP_OK;
  }

  const cJSON *json_cmd = cJSON_GetObjectItemCaseSensitive(json, "command");
  if (cJSON_IsString(json_cmd) && (json_cmd->valuestring != NULL)) {
    ESP_LOGI(TAG, "JSON cmd: %s", json_cmd->valuestring);
    if (strncmp(json_cmd->valuestring, "open", strlen("open")) == 0) {
      command = ACTUATOR_COMMAND_OPEN;
    } else if (strncmp(json_cmd->valuestring, "close", strlen("close")) == 0) {
      command = ACTUATOR_COMMAND_CLOSE;
    }
  }

  const cJSON *json_period = cJSON_GetObjectItemCaseSensitive(json, "period");
  if (cJSON_IsString(json_period) && (json_period->valuestring != NULL)) {
    ESP_LOGI(TAG, "JSON period: %s", json_period->valuestring);
    if (strncmp(json_period->valuestring, "full", strlen("full")) == 0) {
      period = ACTUATOR_PERIOD_FULL;
    } else if (strncmp(json_period->valuestring, "half", strlen("half")) == 0) {
      period = ACTUATOR_PERIOD_HALF;
    } else if (strncmp(json_period->valuestring, "quarter",
                       strlen("quarter")) == 0) {
      period = ACTUATOR_PERIOD_QUARTER;
    }
  }
  cJSON_Delete(json);

  if (command == ACTUATOR_COMMAND_UNSET || period == ACTUATOR_PERIOD_UNSET) {
    const char *result_ok =
        "{\"result\": \"nok\", \"error\": \"Cannot parse known command and "
        "period\"}";
    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    httpd_resp_send(req, result_ok, strlen(result_ok));
    return ESP_OK;
  }

  ESP_LOGI(TAG, "command: %d, period: %d", command, period);

  updateActuatorState(command, period);

  const char *result_ok = "{\"result\": \"ok\"}";
  httpd_resp_set_type(req, HTTPD_TYPE_JSON);
  httpd_resp_send(req, result_ok, strlen(result_ok));
  return ESP_OK;
}

httpd_uri_t post_action_actuator = {.uri = CONFIG_HTTP_BASE_URL "/action/actuator",
                                    .method = HTTP_POST,
                                    .handler = actuator_action_post_handler,
                                    .user_ctx = NULL};

extern ActuatorStatus_t actuatorStatus;
esp_err_t actuator_status_get_handler(httpd_req_t *req) {
  char data[16];
  memset(data, 0, 16);
  sprintf(data, "{\"value\":\"%s\"}",
          actuatorStatus == ACTUATOR_STATUS_INIT               ? "Init"
          : actuatorStatus == ACTUATOR_STATUS_OFF              ? "Off"
          : actuatorStatus == ACTUATOR_STATUS_OPEN_TRANSITION  ? "Opening"
          : actuatorStatus == ACTUATOR_STATUS_CLOSE_TRANSITION ? "Closing"
                                                               : "Unknown");
  httpd_resp_set_type(req, HTTPD_TYPE_JSON);
  httpd_resp_send(req, data, strlen(data));
  return ESP_OK;
}
httpd_uri_t get_status_actuator = {.uri = CONFIG_HTTP_BASE_URL "/status/actuator",
                                   .method = HTTP_GET,
                                   .handler = actuator_status_get_handler};

extern ActuatorStatus_t actuatorLevel;
esp_err_t actuator_level_get_handler(httpd_req_t *req) {
  char data[16];
  memset(data, 0, 16);
  sprintf(data, "{\"value\":%d}", actuatorLevel);
  httpd_resp_set_type(req, HTTPD_TYPE_JSON);
  httpd_resp_send(req, data, strlen(data));
  return ESP_OK;
}
httpd_uri_t get_level_actuator = {.uri = CONFIG_HTTP_BASE_URL "/level/actuator",
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
