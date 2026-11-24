#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "expression/expression.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "voice/voice.h"

static const char * TAG = "ARUKA";

void app_main(void) {
  ESP_LOGI(TAG, "Starting Aruka...");

  ESP_ERROR_CHECK(aruka_expression_init());

  ESP_ERROR_CHECK(aruka_set_expression(ARUKA_EXPR_IDLE));
  ESP_LOGI(TAG, "Expression system ready");

  ESP_LOGI(TAG, "Initializing voice system...");
  ESP_ERROR_CHECK(aruka_voice_init());
  ESP_LOGI(TAG, "WakeNet initialized");
  ESP_LOGI(TAG, "Say 'Hi ESP' to wake Aruka!");

  while(1) {
      vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
