#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "expression/expression.h"

static const char *TAG = "ARUKA";

void app_main(void)
{
    ESP_LOGI(TAG, "Aruka system starting...");
    esp_err_t ret = aruka_expression_init();

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display startup failed: %s",
                esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Aruka is online!");

    while (1) {
        ESP_LOGI(TAG, "Aruka heartbeat");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
