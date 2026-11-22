#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "ARUKA";

void app_main(void)
{
    ESP_LOGI(TAG, "Aruka initialized!");
    ESP_LOGI(TAG, "ESP32-S3-BOX-3 is ready.");

    while (1) {
        ESP_LOGI(TAG, "Aruka heartbeat");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
