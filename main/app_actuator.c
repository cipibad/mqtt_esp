#include "esp_system.h"
#ifdef CONFIG_ACTUATOR_SUPPORT
#include <string.h>

#include "app_actuator.h"
#include "app_main.h"
#include "app_publish_data.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "rom/gpio.h"

static const char* TAG = "APP_ACTUATOR";

TimerHandle_t actuatorTimer;

ActuatorStatus_t actuatorStatus;
ActuatorLevel_t actuatorLevel;

int actuatorMasterPositivePinStatus;
int actuatorMasterNegativePinStatus;
int actuatorOpenPinStatus;
int actuatorClosePinStatus;

void initControlPin(int pin, int* status) {
  *status = GPIO_LOW;
  gpio_pad_select_gpio(pin);
  gpio_set_direction(pin, GPIO_MODE_OUTPUT);
  gpio_set_level(pin, *status);
}

void update_relay_status(int pin, int* status, int value)
{
  ESP_LOGI(TAG, "updateMotorControlPin: pin: %d, status: %d, value: %d", pin, *status, value);
  if (value != (*status == GPIO_HIGH)) {
    if (value == GPIO_STATUS_ON) {
      *status = GPIO_HIGH;
      ESP_LOGI(TAG, "enabling GPIO %d", pin);
    }
    if (value == GPIO_STATUS_OFF) {
      *status = GPIO_LOW;
      ESP_LOGI(TAG, "disabling GPIO %d", pin);
    }
    gpio_set_level(pin, *status);
  }
}


void actuatorTimerCallback(TimerHandle_t xTimer) {
  const char* pcTimerName = pcTimerGetTimerName(xTimer);
  ESP_LOGI(TAG, "timer %s expired", pcTimerName);
  update_relay_status(CONFIG_ACTUATOR_MASTER_POSITIVE_GPIO, &actuatorMasterPositivePinStatus, GPIO_STATUS_OFF);
  update_relay_status(CONFIG_ACTUATOR_MASTER_NEGATIVE_GPIO, &actuatorMasterNegativePinStatus, GPIO_STATUS_OFF);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  update_relay_status(CONFIG_ACTUATOR_OPEN_GPIO, &actuatorOpenPinStatus, GPIO_STATUS_OFF);
  update_relay_status(CONFIG_ACTUATOR_CLOSE_GPIO, &actuatorClosePinStatus, GPIO_STATUS_OFF);
  actuatorStatus = ACTUATOR_STATUS_OFF;
  //  publish_waterpump_status();
  ESP_LOGI(TAG, "actuator is now off");
}

void openActuator(ActuatorPeriod_t period) {
  if (actuatorTimer == NULL) {
    ESP_LOGE(TAG, "No actuatorTimer found");
    return;
  }

  if (xTimerIsTimerActive(actuatorTimer) != pdFALSE) {
    ESP_LOGE(TAG, "actuatorTimer is active while it should not be");
    return;
  }

  if (xTimerChangePeriod(actuatorTimer, pdMS_TO_TICKS(period),
                         portMAX_DELAY) != pdPASS) {
    ESP_LOGE(TAG, "cannot update actuatorTimer period");
    return;
  }

  if (xTimerStart(actuatorTimer, 0) != pdPASS) {
    ESP_LOGE(TAG, "Cannot start actuatorTimer");
    return;
  }

  update_relay_status(CONFIG_ACTUATOR_OPEN_GPIO, &actuatorOpenPinStatus, GPIO_STATUS_ON);
  update_relay_status(CONFIG_ACTUATOR_CLOSE_GPIO, &actuatorClosePinStatus, GPIO_STATUS_OFF);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  update_relay_status(CONFIG_ACTUATOR_MASTER_NEGATIVE_GPIO, &actuatorMasterNegativePinStatus, GPIO_STATUS_ON);
  update_relay_status(CONFIG_ACTUATOR_MASTER_POSITIVE_GPIO, &actuatorMasterPositivePinStatus, GPIO_STATUS_ON);
  actuatorStatus = ACTUATOR_STATUS_OPEN_TRANSITION;
  actuatorLevel += period / ACTUATOR_PERIOD_UNIT;
  //  publish_waterpump_status();
  ESP_LOGI(TAG, "actuator opening is on-going");
}

void closeActuator(ActuatorPeriod_t period) {
  if (actuatorTimer == NULL) {
    ESP_LOGE(TAG, "No actuatorTimer found");
    return;
  }

  if (xTimerIsTimerActive(actuatorTimer) != pdFALSE) {
    ESP_LOGE(TAG, "actuatorTimer is active while it should not be");
    return;
  }

  if (xTimerChangePeriod(actuatorTimer, pdMS_TO_TICKS(period),
                         portMAX_DELAY) != pdPASS) {
    ESP_LOGE(TAG, "cannot update actuatorTimer period");
    return;
  }

  if (xTimerStart(actuatorTimer, 0) != pdPASS) {
    ESP_LOGE(TAG, "Cannot start actuatorTimer");
    return;
  }

  update_relay_status(CONFIG_ACTUATOR_OPEN_GPIO, &actuatorOpenPinStatus, GPIO_STATUS_OFF);
  update_relay_status(CONFIG_ACTUATOR_CLOSE_GPIO, &actuatorClosePinStatus, GPIO_STATUS_ON);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  update_relay_status(CONFIG_ACTUATOR_MASTER_NEGATIVE_GPIO, &actuatorMasterNegativePinStatus, GPIO_STATUS_ON);
  update_relay_status(CONFIG_ACTUATOR_MASTER_POSITIVE_GPIO, &actuatorMasterPositivePinStatus, GPIO_STATUS_ON);
  actuatorStatus = ACTUATOR_STATUS_CLOSE_TRANSITION;
  actuatorLevel -= period / ACTUATOR_PERIOD_UNIT;

  //  publish_waterpump_status();
  ESP_LOGI(TAG, "actuator closing is on-going");
}

void updateActuatorState(ActuatorCommand_t command, ActuatorPeriod_t period) {
  if (actuatorStatus == ACTUATOR_STATUS_OPEN_TRANSITION) {
    ESP_LOGE(TAG, "another actuator opening command is ongoing");
    return;
  }
  if (actuatorStatus == ACTUATOR_STATUS_CLOSE_TRANSITION) {
    ESP_LOGE(TAG, "another actuator closing command is ongoing");
    return;
  }
  if (actuatorStatus != ACTUATOR_STATUS_OFF) {
    ESP_LOGE(TAG, "another actuator action is on-going");
    return;
  }

  if (command == ACTUATOR_COMMAND_OPEN) {
    switch (actuatorLevel) {
      case ACTUATOR_LEVEL_OPEN:
        ESP_LOGE(TAG, "actuator is already open");
        return;
        break;
      case ACTUATOR_LEVEL_OPEN_3_4:
        if (period > ACTUATOR_PERIOD_QUARTER) {
          period = ACTUATOR_PERIOD_QUARTER;
        }
        break;
      case ACTUATOR_LEVEL_OPEN_1_2:
        if (period > ACTUATOR_PERIOD_HALF) {
          period = ACTUATOR_PERIOD_HALF;
        }
        break;
      case ACTUATOR_LEVEL_OPEN_1_4:
        if (period > ACTUATOR_PERIOD_THREE_QUARTERS) {
          period = ACTUATOR_PERIOD_THREE_QUARTERS;
        }
        break;
      default:
        break;
    }
    openActuator(period);
  } else if (command == ACTUATOR_COMMAND_CLOSE) {
    switch (actuatorLevel) {
      case ACTUATOR_LEVEL_CLOSED:
        ESP_LOGE(TAG, "actuator is already closed");
        return;
        break;
      case ACTUATOR_LEVEL_OPEN_1_4:
        if (period > ACTUATOR_PERIOD_QUARTER) {
          period = ACTUATOR_PERIOD_QUARTER;
        }
        break;
      case ACTUATOR_LEVEL_OPEN_1_2:
        if (period > ACTUATOR_PERIOD_HALF) {
          period = ACTUATOR_PERIOD_HALF;
        }
        break;
      case ACTUATOR_LEVEL_OPEN_3_4:
        if (period > ACTUATOR_PERIOD_THREE_QUARTERS) {
          period = ACTUATOR_PERIOD_THREE_QUARTERS;
        }
        break;
      default:
        break;
    }
    closeActuator(period);
  }
}

void initActuator() {
  ESP_LOGI(TAG, "Initializing");
  actuatorStatus = ACTUATOR_STATUS_INIT;
  actuatorLevel = ACTUATOR_LEVEL_UNSET;
  actuatorTimer =
      xTimerCreate("actuatorTimer",                     /* Text name. */
                   pdMS_TO_TICKS(ACTUATOR_PERIOD_FULL), /* Period. */
                   pdFALSE,                             /* Autoreload. */
                   (void*)1,                            /* ID. */
                   actuatorTimerCallback);              /* Callback function. */

  initControlPin(CONFIG_ACTUATOR_MASTER_POSITIVE_GPIO, &actuatorMasterPositivePinStatus);
  initControlPin(CONFIG_ACTUATOR_MASTER_NEGATIVE_GPIO, &actuatorMasterNegativePinStatus);
  initControlPin(CONFIG_ACTUATOR_OPEN_GPIO, &actuatorOpenPinStatus);
  initControlPin(CONFIG_ACTUATOR_CLOSE_GPIO, &actuatorClosePinStatus);

  vTaskDelay(1000 / portTICK_PERIOD_MS);

  ESP_LOGI(TAG, "Performing close after start-up");
  closeActuator(ACTUATOR_PERIOD_FULL);
  actuatorLevel = ACTUATOR_LEVEL_CLOSED;
}

#endif  // CONFIG_ACTUATOR_SUPPORT
