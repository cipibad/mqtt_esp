#include "app_sensors_bh1750.h"
#include "app_publish_data.h"
#include "app_logging.h"
#include "app_bh1750.h"
#include "app_i2c.h"

static uint16_t illuminance;

void bh1750_init(void)
{
}

void bh1750_read(void)
{
  esp_err_t ret = i2c_master_BH1750_read(&illuminance);
  if (ret == ESP_ERR_TIMEOUT) {
    LOGE(LOG_MODULE_BH1750, "I2C Timeout");
  } else if (ret == ESP_OK) {
    LOGI(LOG_MODULE_BH1750, "Illuminance: %d.%02d lx", illuminance/100, illuminance%100);
  } else {
    LOGW(LOG_MODULE_BH1750, "%s: No ack, sensor not connected...skip...", esp_err_to_name(ret));
  }
}

void bh1750_publish(void)
{
  const char * topic = CONFIG_DEVICE_TYPE "/" CONFIG_CLIENT_ID "/evt/illuminance/bh1750";

  if (illuminance == 0) {
    LOGE(LOG_MODULE_BH1750, "Invalid illuminance value (0), skipping publish");
    return;
  }

  char data[16];
  memset(data,0,16);
  sprintf(data, "%d.%02d", illuminance / 100, abs(illuminance % 100));
  publish_non_persistent_data(topic, data);

#ifdef CONFIG_PRESENCE_AUTOMATION_SUPPORT
  extern EventGroupHandle_t presenceEventGroup;
  extern const int PRESENCE_NEW_DATA_BIT;
  if( presenceEventGroup != NULL ) {
    xEventGroupSetBits (presenceEventGroup, PRESENCE_NEW_DATA_BIT);
  }
#endif
}

void bh1750_publish_ha(void)
{
  const char * topic = CONFIG_HOME_NAME "/" CONFIG_ROOM_NAME "/sensor";
  char payload[128];

  if (illuminance == 0) {
    return;
  }

  sprintf(payload, "{\"source\":\"bh1750\",\"illuminance\":%d.%02d}",
          illuminance / 100, abs(illuminance % 100));

  publish_non_persistent_data(topic, payload);
}
