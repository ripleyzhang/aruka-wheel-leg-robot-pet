
#include "behavior/behavior.h"
#include "esp_err.h"
#include "esp_log.h"
#include "expression/expression.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "voice/voice.h"

static const char * TAG = "ARUKA";

// Set to 1 for the local INT-02 test.
// Set to 0 to run the normal voice application.
#ifndef INT02_MOCK_TEST
#define INT02_MOCK_TEST 0
#endif

#if INT02_MOCK_TEST

static void post_test_event(aruka_event_type_t type) {
  esp_err_t ret = aruka_behavior_post_event(type);
  ESP_ERROR_CHECK(ret);

  // Let the asynchronous Behavior Task process the event.
  vTaskDelay(pdMS_TO_TICKS(1000));
}

static void confirm_test_task(uint8_t task_id) {
  esp_err_t ret = aruka_behavior_post_task_confirmed(task_id);

  ESP_ERROR_CHECK(ret);

  vTaskDelay(pdMS_TO_TICKS(1000));
}

static void wish_cycle_test_task(void * arg) {
  vTaskDelay(pdMS_TO_TICKS(1000));

  ESP_LOGI(TAG, "=== INT-02 TEST START ===");

  post_test_event(ARUKA_EVENT_WAKE_WORD);
  post_test_event(ARUKA_EVENT_COMMAND_TIMEOUT);
  post_test_event(ARUKA_EVENT_CYCLE_START);

  confirm_test_task(1);
  confirm_test_task(1);  // Duplicate: must be ignored.
  confirm_test_task(2);
  confirm_test_task(3);

  post_test_event(ARUKA_EVENT_WAKE_WORD);
  post_test_event(ARUKA_EVENT_COMMAND_TIMEOUT);
  post_test_event(ARUKA_EVENT_REWARD_CLAIMED);
  post_test_event(ARUKA_EVENT_REWARD_CLAIMED);
  post_test_event(ARUKA_EVENT_WAKE_WORD);
  post_test_event(ARUKA_EVENT_COMMAND_TIMEOUT);

  post_test_event(ARUKA_EVENT_PRAISE_RECEIVED);

  ESP_LOGI(TAG, "=== INT-02 TEST END ===");

  vTaskDelete(NULL);
}

#endif

void app_main(void) {
  ESP_LOGI(TAG, "Starting Aruka...");

  ESP_ERROR_CHECK(aruka_expression_init());
  ESP_ERROR_CHECK(aruka_behavior_init());

#if INT02_MOCK_TEST

  ESP_LOGI(TAG, "INT-02 mock test mode");

  BaseType_t ok =
      xTaskCreate(wish_cycle_test_task, "wish_cycle_test", 4096, NULL, 3, NULL);

  if (ok != pdPASS) {
    ESP_LOGE(TAG, "Cannot create mock test task");
  }

#else

  // Normal mode: Voice events enter the Behavior Queue.
  ESP_ERROR_CHECK(aruka_voice_init());

  ESP_LOGI(TAG, "Voice interaction ready");

#endif

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
