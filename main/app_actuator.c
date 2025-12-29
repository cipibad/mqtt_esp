#include "esp_system.h"
#ifdef CONFIG_ACTUATOR_SUPPORT
#include <string.h>

#include "app_actuator.h"
#include "app_main.h"
#include "app_publish_data.h"
#include "driver/gpio.h"
#include "app_logging.h"
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
  LOGI(TAG, LOG_MODULE_THERMOSTAT, "updateMotorControlPin: pin: %d, status: %d, value: %d", pin, *status, value);
  if (value != (*status == GPIO_HIGH)) {
    if (value == GPIO_STATUS_ON) {
      *status = GPIO_HIGH;
      LOGI(TAG, LOG_MODULE_THERMOSTAT, "enabling GPIO %d", pin);
    }
    if (value == GPIO_STATUS_OFF) {
      *status = GPIO_LOW;
      LOGI(TAG, LOG_MODULE_THERMOSTAT, "disabling GPIO %d", pin);
    }
    gpio_set_level(pin, *status);
  }
}


void actuatorTimerCallback(TimerHandle_t xTimer) {
  const char* pcTimerName = pcTimerGetTimerName(xTimer);
  LOGI(TAG, LOG_MODULE_THERMOSTAT, "timer %s expired", pcTimerName);
  update_relay_status(CONFIG_ACTUATOR_MASTER_POSITIVE_GPIO, &actuatorMasterPositivePinStatus, GPIO_STATUS_OFF);
  update_relay_status(CONFIG_ACTUATOR_MASTER_NEGATIVE_GPIO, &actuatorMasterNegativePinStatus, GPIO_STATUS_OFF);
  vTaskDelay(ACTUATOR_PERIOD_INTER_RELAY_OPERATION / portTICK_PERIOD_MS);
  update_relay_status(CONFIG_ACTUATOR_OPEN_GPIO, &actuatorOpenPinStatus, GPIO_STATUS_OFF);
  update_relay_status(CONFIG_ACTUATOR_CLOSE_GPIO, &actuatorClosePinStatus, GPIO_STATUS_OFF);
  actuatorStatus = ACTUATOR_STATUS_OFF;
  //  publish_waterpump_status();
  LOGI(TAG, LOG_MODULE_THERMOSTAT, "actuator is now off");
}

void openActuator(ActuatorPeriod_t period) {
  if (actuatorTimer == NULL) {
    LOGE(TAG, LOG_MODULE_THERMOSTAT, "No actuatorTimer found");
    return;
  }

  if (xTimerIsTimerActive(actuatorTimer) != pdFALSE) {
    LOGE(TAG, LOG_MODULE_THERMOSTAT, "actuatorTimer is active while it should not be");
    return;
  }

  if (xTimerChangePeriod(actuatorTimer, pdMS_TO_TICKS(period + ACTUATOR_PERIOD_INTER_RELAY_OPERATION),
                         portMAX_DELAY) != pdPASS) {
    LOGE(TAG, LOG_MODULE_THERMOSTAT, "cannot update actuatorTimer period");
    return;
  }

  if (xTimerStart(actuatorTimer, 0) != pdPASS) {
    LOGE(TAG, LOG_MODULE_THERMOSTAT, "Cannot start actuatorTimer");
    return;
  }

  update_relay_status(CONFIG_ACTUATOR_OPEN_GPIO, &actuatorOpenPinStatus, GPIO_STATUS_ON);
  update_relay_status(CONFIG_ACTUATOR_CLOSE_GPIO, &actuatorClosePinStatus, GPIO_STATUS_OFF);
  vTaskDelay(ACTUATOR_PERIOD_INTER_RELAY_OPERATION / portTICK_PERIOD_MS);
  update_relay_status(CONFIG_ACTUATOR_MASTER_NEGATIVE_GPIO, &actuatorMasterNegativePinStatus, GPIO_STATUS_ON);
  update_relay_status(CONFIG_ACTUATOR_MASTER_POSITIVE_GPIO, &actuatorMasterPositivePinStatus, GPIO_STATUS_ON);
  actuatorStatus = ACTUATOR_STATUS_OPEN_TRANSITION;
  actuatorLevel += period / ACTUATOR_PERIOD_UNIT;
  //  publish_waterpump_status();
  LOGI(TAG, LOG_MODULE_THERMOSTAT, "actuator opening is on-going");
}

void closeActuator(ActuatorPeriod_t period) {
  if (actuatorTimer == NULL) {
    LOGE(TAG, LOG_MODULE_THERMOSTAT, "No actuatorTimer found");
    return;
  }

  if (xTimerIsTimerActive(actuatorTimer) != pdFALSE) {
    LOGE(TAG, LOG_MODULE_THERMOSTAT, "actuatorTimer is active while it should not be");
    return;
  }

  ActuatorPeriod_t closeExtraPeriod = ACTUATOR_PERIOD_NONE;
  if ((actuatorLevel - period / ACTUATOR_PERIOD_UNIT) == ACTUATOR_LEVEL_CLOSED) {
    closeExtraPeriod = ACTUATOR_PERIOD_CLOSE_RESERVE;
  }

  if (xTimerChangePeriod(actuatorTimer, pdMS_TO_TICKS(period + ACTUATOR_PERIOD_INTER_RELAY_OPERATION + closeExtraPeriod),
                         portMAX_DELAY) != pdPASS) {
    LOGE(TAG, LOG_MODULE_THERMOSTAT, "cannot update actuatorTimer period");
    return;
  }

  if (xTimerStart(actuatorTimer, 0) != pdPASS) {
    LOGE(TAG, LOG_MODULE_THERMOSTAT, "Cannot start actuatorTimer");
    return;
  }

  update_relay_status(CONFIG_ACTUATOR_OPEN_GPIO, &actuatorOpenPinStatus, GPIO_STATUS_OFF);
  update_relay_status(CONFIG_ACTUATOR_CLOSE_GPIO, &actuatorClosePinStatus, GPIO_STATUS_ON);
  vTaskDelay(ACTUATOR_PERIOD_INTER_RELAY_OPERATION / portTICK_PERIOD_MS);
  update_relay_status(CONFIG_ACTUATOR_MASTER_NEGATIVE_GPIO, &actuatorMasterNegativePinStatus, GPIO_STATUS_ON);
  update_relay_status(CONFIG_ACTUATOR_MASTER_POSITIVE_GPIO, &actuatorMasterPositivePinStatus, GPIO_STATUS_ON);
  actuatorStatus = ACTUATOR_STATUS_CLOSE_TRANSITION;
  actuatorLevel -= period / ACTUATOR_PERIOD_UNIT;

  //  publish_waterpump_status();
  LOGI(TAG, LOG_MODULE_THERMOSTAT, "actuator closing is on-going");
}

void updateActuatorState(ActuatorCommand_t command, ActuatorPeriod_t period) {
  if (actuatorStatus == ACTUATOR_STATUS_OPEN_TRANSITION) {
    LOGE(TAG, LOG_MODULE_THERMOSTAT, "another actuator opening command is ongoing");
    return;
  }
  if (actuatorStatus == ACTUATOR_STATUS_CLOSE_TRANSITION) {
    LOGE(TAG, LOG_MODULE_THERMOSTAT, "another actuator closing command is ongoing");
    return;
  }
  if (actuatorStatus != ACTUATOR_STATUS_OFF) {
    LOGE(TAG, LOG_MODULE_THERMOSTAT, "another actuator action is on-going");
    return;
  }

  if (command == ACTUATOR_COMMAND_OPEN) {
    switch (actuatorLevel) {
      case ACTUATOR_LEVEL_OPEN:
        LOGE(TAG, LOG_MODULE_THERMOSTAT, "actuator is already open");
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
        LOGE(TAG, LOG_MODULE_THERMOSTAT, "actuator is already closed");
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
  LOGI(TAG, LOG_MODULE_THERMOSTAT, "Initializing");
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

  vTaskDelay(ACTUATOR_PERIOD_INTER_RELAY_OPERATION / portTICK_PERIOD_MS);

  LOGI(TAG, LOG_MODULE_THERMOSTAT, "Performing close after start-up");
  closeActuator(ACTUATOR_PERIOD_FULL);
  actuatorLevel = ACTUATOR_LEVEL_CLOSED;
}

#endif  // CONFIG_ACTUATOR_SUPPORT
