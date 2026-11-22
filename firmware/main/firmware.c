#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "expression/expression.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char * TAG = "ARUKA";

void app_main(void) {
  ESP_LOGI(TAG, "Starting Aruka...");

  ESP_ERROR_CHECK(aruka_expression_init());

  ESP_LOGI(TAG, "Expression test started!");

  aruka_expression_t expressions[] = {
      ARUKA_EXPR_IDLE,
      ARUKA_EXPR_HAPPY,
      ARUKA_EXPR_ANGRY,
      ARUKA_EXPR_SLEEPY,
      ARUKA_EXPR_LISTENING,
  };

  const int num_expressions = sizeof(expressions) / sizeof(expressions[0]);

  int index = 0;

  while (1) {
    ESP_LOGI(TAG, "Switching expression: %d", index);

    esp_err_t ret = aruka_set_expression(expressions[index]);

    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Expression switch failed: %s", esp_err_to_name(ret));
    }

    index = (index + 1) % num_expressions;

    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}
