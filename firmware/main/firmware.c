#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "expression/expression.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "voice/voice.h"
#include "behavior/behavior.h"

static const char * TAG = "ARUKA";

void app_main(void) {
  ESP_LOGI(TAG, "Starting Aruka...");

  ESP_ERROR_CHECK(aruka_expression_init());
  ESP_LOGI(TAG, "Expression system ready");

  ESP_ERROR_CHECK(aruka_behavior_init());
  ESP_LOGI(TAG, "Behavior system ready");

  ESP_ERROR_CHECK(aruka_voice_init());
  ESP_LOGI(TAG, "Voice system ready");

  while(1) {
      vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
