#include "app_ota.h"

/* OTA example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

extern QueueHandle_t otaQueue;

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "app_esp8266.h"

#define EXAMPLE_SERVER_IP "sw.iot.cipex.ro"
#define EXAMPLE_SERVER_PORT "8910"
#define EXAMPLE_FILENAME "/"CONFIG_MQTT_CLIENT_ID".bin"
#define BUFFSIZE 1500
#define TEXT_BUFFSIZE 1024

typedef enum esp_ota_firm_state {
  ESP_OTA_INIT = 0,
  ESP_OTA_PREPARE,
  ESP_OTA_START,
  ESP_OTA_RECVED,
  ESP_OTA_FINISH,
} esp_ota_firm_state_t;

typedef struct esp_ota_firm {
  uint8_t             ota_num;
  uint8_t             update_ota_num;

  esp_ota_firm_state_t    state;

  size_t              content_len;

  size_t              read_bytes;
  size_t              write_bytes;

  size_t              ota_size;
  size_t              ota_offset;

  const char          *buf;
  size_t              bytes;
} esp_ota_firm_t;

static const char *TAG = "ota";
/*an ota data write buffer ready to write to the flash*/
static char ota_write_data[BUFFSIZE + 1] = { 0 };
/*an packet receive buffer*/
static char text[BUFFSIZE + 1] = { 0 };
/* an image total length*/
static int binary_file_length = 0;
/*socket id*/
static int socket_id = -1;

extern EventGroupHandle_t mqtt_event_group;
extern const int MQTT_PUBLISHED_BIT;
extern const int MQTT_INIT_FINISHED_BIT;


int hostname_to_ip(char * hostname , char* ip)
{
	struct hostent *he;
	struct in_addr **addr_list;
	int i;
		
	if ( (he = gethostbyname( hostname ) ) == NULL)
    {
      // get the host info
      ESP_LOGI(TAG, "gethostbyname");
      return 1;
    }

	addr_list = (struct in_addr **) he->h_addr_list;
	
	for(i = 0; addr_list[i] != NULL; i++)
    {
      //Return the first one;
      strcpy(ip , inet_ntoa(*addr_list[i]) );
      return 0;
    }
	
	return 1;
}

/*read buffer by byte still delim ,return read bytes counts*/
static int read_until(const char *buffer, char delim, int len)
{
  //  /*TODO: delim check,buffer check,further: do an buffer length limited*/
  int i = 0;
  while (buffer[i] != delim && i < len) {
    ++i;
  }
  return i + 1;
}

static bool connect_to_http_server()
{
  ESP_LOGI(TAG, "Server IP: %s Server Port:%s", EXAMPLE_SERVER_IP, EXAMPLE_SERVER_PORT);

  int  http_connect_flag = -1;
  struct sockaddr_in sock_info;

  socket_id = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_id == -1) {
    ESP_LOGE(TAG, "Create socket failed!");
    return false;
  }

  char ip[16];
  hostname_to_ip(EXAMPLE_SERVER_IP, ip);

  // set connect info
  memset(&sock_info, 0, sizeof(struct sockaddr_in));
  sock_info.sin_family = AF_INET;
  sock_info.sin_addr.s_addr = inet_addr(ip);
  sock_info.sin_port = htons(atoi(EXAMPLE_SERVER_PORT));

  // connect to http server
  http_connect_flag = connect(socket_id, (struct sockaddr *)&sock_info, sizeof(sock_info));
  if (http_connect_flag == -1) {
    ESP_LOGE(TAG, "Connect to server failed! errno=%d", errno);
    close(socket_id);
    return false;
  } else {
    ESP_LOGI(TAG, "Connected to server");
    return true;
  }
  return false;
}

static void __attribute__((noreturn)) task_fatal_error()
{
  ESP_LOGE(TAG, "Exiting task due to fatal error...");
  close(socket_id);
  (void)vTaskDelete(NULL);

  while (1) {
    ;
  }
}

bool _esp_ota_firm_parse_http(esp_ota_firm_t *ota_firm, const char *text, size_t total_len, size_t *parse_len)
{
  /* i means current position */
  int i = 0, i_read_len = 0;
  char *ptr = NULL, *ptr2 = NULL;
  char length_str[32];

  while (text[i] != 0 && i < total_len) {
    if (ota_firm->content_len == 0 && (ptr = (char *)strstr(text, "Content-Length")) != NULL) {
      ptr += 16;
      ptr2 = (char *)strstr(ptr, "\r\n");
      memset(length_str, 0, sizeof(length_str));
      memcpy(length_str, ptr, ptr2 - ptr);
      ota_firm->content_len = atoi(length_str);
#if defined(CONFIG_ESPTOOLPY_FLASHSIZE_1MB) && !defined(CONFIG_ESP8266_BOOT_COPY_APP)
      ota_firm->ota_size = ota_firm->content_len / ota_firm->ota_num;
      ota_firm->ota_offset = ota_firm->ota_size * ota_firm->update_ota_num;
#else
      ota_firm->ota_size = ota_firm->content_len;
      ota_firm->ota_offset = 0;
#endif
      ESP_LOGI(TAG, "parse Content-Length:%d, ota_size %d", ota_firm->content_len, ota_firm->ota_size);
    }

    i_read_len = read_until(&text[i], '\n', total_len - i);

    if (i_read_len > total_len - i) {
      ESP_LOGE(TAG, "recv malformed http header");
      task_fatal_error();
    }

    // if resolve \r\n line, http header is finished
    if (i_read_len == 2) {
      if (ota_firm->content_len == 0) {
        ESP_LOGE(TAG, "did not parse Content-Length item");
        task_fatal_error();
      }

      *parse_len = i + 2;

      return true;
    }

    i += i_read_len;
  }

  return false;
}

static size_t esp_ota_firm_do_parse_msg(esp_ota_firm_t *ota_firm, const char *in_buf, size_t in_len)
{
  size_t tmp;
  size_t parsed_bytes = in_len;

  switch (ota_firm->state) {
  case ESP_OTA_INIT:
    if (_esp_ota_firm_parse_http(ota_firm, in_buf, in_len, &tmp)) {
      ota_firm->state = ESP_OTA_PREPARE;
      ESP_LOGD(TAG, "Http parse %d bytes", tmp);
      parsed_bytes = tmp;
    }
    break;
  case ESP_OTA_PREPARE:
    ota_firm->read_bytes += in_len;

    if (ota_firm->read_bytes >= ota_firm->ota_offset) {
      ota_firm->buf = &in_buf[in_len - (ota_firm->read_bytes - ota_firm->ota_offset)];
      ota_firm->bytes = ota_firm->read_bytes - ota_firm->ota_offset;
      ota_firm->write_bytes += ota_firm->read_bytes - ota_firm->ota_offset;
      ota_firm->state = ESP_OTA_START;
      ESP_LOGD(TAG, "Receive %d bytes and start to update", ota_firm->read_bytes);
      ESP_LOGD(TAG, "Write %d total %d", ota_firm->bytes, ota_firm->write_bytes);
    }

    break;
  case ESP_OTA_START:
    if (ota_firm->write_bytes + in_len > ota_firm->ota_size) {
      ota_firm->bytes = ota_firm->ota_size - ota_firm->write_bytes;
      ota_firm->state = ESP_OTA_RECVED;
    } else
      ota_firm->bytes = in_len;

    ota_firm->buf = in_buf;

    ota_firm->write_bytes += ota_firm->bytes;

    ESP_LOGD(TAG, "Write %d total %d", ota_firm->bytes, ota_firm->write_bytes);

    break;
  case ESP_OTA_RECVED:
    parsed_bytes = 0;
    ota_firm->state = ESP_OTA_FINISH;
    break;
  default:
    parsed_bytes = 0;
    ESP_LOGD(TAG, "State is %d", ota_firm->state);
    break;
  }

  return parsed_bytes;
}

static void esp_ota_firm_parse_msg(esp_ota_firm_t *ota_firm, const char *in_buf, size_t in_len)
{
  size_t parse_bytes = 0;

  ESP_LOGD(TAG, "Input %d bytes", in_len);

  do {
    size_t bytes = esp_ota_firm_do_parse_msg(ota_firm, in_buf + parse_bytes, in_len - parse_bytes);
    ESP_LOGD(TAG, "Parse %d bytes", bytes);
    if (bytes)
      parse_bytes += bytes;
  } while (parse_bytes != in_len);
}

static inline int esp_ota_firm_is_finished(esp_ota_firm_t *ota_firm)
{
  return (ota_firm->state == ESP_OTA_FINISH || ota_firm->state == ESP_OTA_RECVED);
}

static inline int esp_ota_firm_can_write(esp_ota_firm_t *ota_firm)
{
  return (ota_firm->state == ESP_OTA_START || ota_firm->state == ESP_OTA_RECVED);
}

static inline const char* esp_ota_firm_get_write_buf(esp_ota_firm_t *ota_firm)
{
  return ota_firm->buf;
}

static inline size_t esp_ota_firm_get_write_bytes(esp_ota_firm_t *ota_firm)
{
  return ota_firm->bytes;
}

static void esp_ota_firm_init(esp_ota_firm_t *ota_firm, const esp_partition_t *update_partition)
{
  memset(ota_firm, 0, sizeof(esp_ota_firm_t));
  ota_firm->state = ESP_OTA_INIT;
  ota_firm->ota_num = get_ota_partition_count();
  ota_firm->update_ota_num = update_partition->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_0;

  ESP_LOGI(TAG, "Totoal OTA number %d update to %d part", ota_firm->ota_num, ota_firm->update_ota_num);

}


void handle_ota_update_task(void *pvParameters)
{

  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;

  esp_err_t err;
  /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
  esp_ota_handle_t update_handle = 0 ;
  const esp_partition_t *update_partition = NULL;

  ESP_LOGI(TAG, "Starting OTA update task... @ %p flash %s", handle_ota_update_task, CONFIG_ESPTOOLPY_FLASHSIZE);

  const esp_partition_t *configured = esp_ota_get_boot_partition();
  const esp_partition_t *running = esp_ota_get_running_partition();

  if (configured != running) {
    ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
             configured->address, running->address);
    ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
  }
  ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
           running->type, running->subtype, running->address);
  struct OtaMessage o;
  while(1) {

    if( xQueueReceive( otaQueue, &o , portMAX_DELAY) )
      {
        ESP_LOGI(TAG, "OTA cmd received....");
        publish_ota_data(client, OTA_ONGOING);

        /*connect to http server*/
        if (connect_to_http_server()) {
          ESP_LOGI(TAG, "Connected to http server");
        } else {
          ESP_LOGE(TAG, "Connect to http server failed!");
          publish_ota_data(client, OTA_FAILED);
          continue;
        }

        /*send GET request to http server*/
        const char *GET_FORMAT =
          "GET %s HTTP/1.0\r\n"
          "Host: %s:%s\r\n"
          "User-Agent: esp-idf/1.0 esp32\r\n\r\n";

        char *http_request = NULL;
        int get_len = asprintf(&http_request, GET_FORMAT, EXAMPLE_FILENAME, EXAMPLE_SERVER_IP, EXAMPLE_SERVER_PORT);
        if (get_len < 0) {
          ESP_LOGE(TAG, "Failed to allocate memory for GET request buffer");
          close(socket_id);
          publish_ota_data(client, OTA_FAILED);
          continue;
        }
        int res = send(socket_id, http_request, get_len, 0);
        free(http_request);

        if (res < 0) {
          ESP_LOGE(TAG, "Send GET request to server failed");
          close(socket_id);
          publish_ota_data(client, OTA_FAILED);
          continue;
        } else {
          ESP_LOGI(TAG, "Send GET request to server succeeded, file: %s", EXAMPLE_FILENAME);
        }

        update_partition = esp_ota_get_next_update_partition(NULL);
        ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
                 update_partition->subtype, update_partition->address);
        assert(update_partition != NULL);

        err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
        if (err != ESP_OK) {
          ESP_LOGE(TAG, "esp_ota_begin failed, error=%d", err);
          close(socket_id);
          publish_ota_data(client, OTA_FAILED);
          continue;
        }
        ESP_LOGI(TAG, "esp_ota_begin succeeded");

        bool flag = true;
        esp_ota_firm_t ota_firm;

        esp_ota_firm_init(&ota_firm, update_partition);


        while (flag) {
          memset(text, 0, TEXT_BUFFSIZE);
          memset(ota_write_data, 0, BUFFSIZE);
          int buff_len = recv(socket_id, text, TEXT_BUFFSIZE, 0);
          if (buff_len < 0) { /*receive error*/
            ESP_LOGE(TAG, "Error: receive data error! errno=%d", errno);
            close(socket_id);
            publish_ota_data(client, OTA_FAILED);
            flag=false;
            continue;
          } else if (buff_len > 0) { /*deal with response body*/
            esp_ota_firm_parse_msg(&ota_firm, text, buff_len);

            if (!esp_ota_firm_can_write(&ota_firm))
              continue;

            memcpy(ota_write_data, esp_ota_firm_get_write_buf(&ota_firm), esp_ota_firm_get_write_bytes(&ota_firm));
            buff_len = esp_ota_firm_get_write_bytes(&ota_firm);

            err = esp_ota_write( update_handle, (const void *)ota_write_data, buff_len);
            if (err != ESP_OK) {
              ESP_LOGE(TAG, "Error: esp_ota_write failed! err=0x%x", err);
              close(socket_id);
              publish_ota_data(client, OTA_FAILED);
              flag=false;
              continue;
            }
            binary_file_length += buff_len;
            ESP_LOGI(TAG, "Have written image length %d", binary_file_length);
          } else if (buff_len == 0) {  /*packet over*/
            flag = false;
            ESP_LOGI(TAG, "Connection closed, all packets received");
            close(socket_id);
          } else {
            ESP_LOGE(TAG, "Unexpected recv result");
          }

          if (esp_ota_firm_is_finished(&ota_firm))
            break;
        }

        ESP_LOGI(TAG, "Total Write binary data length : %d", binary_file_length);

        if (esp_ota_end(update_handle) != ESP_OK) {
          ESP_LOGE(TAG, "esp_ota_end failed!");
          close(socket_id);
          publish_ota_data(client, OTA_FAILED);
          continue;
        }
        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK) {
          ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
          close(socket_id);
          publish_ota_data(client, OTA_FAILED);
          continue;
        }
        ESP_LOGI(TAG, "Prepare to restart system in 10 seconds!");
        publish_ota_data(client, OTA_SUCCESFULL);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        esp_restart();
      }
  }
}


void publish_ota_data(esp_mqtt_client_handle_t client, int status)
{
  if (xEventGroupGetBits(mqtt_event_group) & MQTT_INIT_FINISHED_BIT)
    {
      const char * connect_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/ota";
      char data[256];
      memset(data,0,256);

      sprintf(data, "{\"status\":%d}", status);
      xEventGroupClearBits(mqtt_event_group, MQTT_PUBLISHED_BIT);
      int msg_id = esp_mqtt_client_publish(client, connect_topic, data,strlen(data), 1, 0);
      if (msg_id > 0) {
        ESP_LOGI(TAG, "sent publish ota data successful, msg_id=%d", msg_id);
        EventBits_t bits = xEventGroupWaitBits(mqtt_event_group, MQTT_PUBLISHED_BIT, false, true, MQTT_FLAG_TIMEOUT);
        if (bits & MQTT_PUBLISHED_BIT) {
          ESP_LOGI(TAG, "publish ack received, msg_id=%d", msg_id);
        } else {
          ESP_LOGW(TAG, "publish ack not received, msg_id=%d", msg_id);
        }
      } else {
        ESP_LOGI(TAG, "failed to publish ota data, msg_id=%d", msg_id);
      }
    }
}
