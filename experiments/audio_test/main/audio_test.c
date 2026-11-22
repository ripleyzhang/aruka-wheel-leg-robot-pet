#include <stdint.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"

#include "bsp/esp-box-3.h"
#include "esp_codec_dev.h"

#define SAMPLE_RATE 16000
#define BUFFER_SAMPLES 1024

static const char *TAG = "ARUKA_MIC";
static int16_t audio_buffer[BUFFER_SAMPLES];

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing Aruka microphone...");

    // Initialize onboard microphone.
    // BSP automatically initializes required I2C/I2S
    // interfaces if they have not been initialized.
    esp_codec_dev_handle_t mic =
        bsp_audio_codec_microphone_init();

    if (mic == NULL) {
        ESP_LOGE(TAG, "Microphone initialization failed!");
        return;
    }

    // Configure PCM audio format.
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = SAMPLE_RATE,
        .channel = 1,
        .bits_per_sample = 16,
    };

    int ret = esp_codec_dev_open(mic, &fs);

    if (ret != 0) {
        ESP_LOGE(TAG, "Cannot open microphone: %d", ret);
        return;
    }

    // Optional microphone input gain.
    esp_codec_dev_set_in_gain(mic, 30.0f);

    ESP_LOGI(TAG, "Microphone ready!");
    ESP_LOGI(TAG, "Speak or clap near Aruka.");

    while (1) {
        ret = esp_codec_dev_read(
            mic,
            audio_buffer,
            sizeof(audio_buffer)
        );

        if (ret != 0) {
            ESP_LOGE(TAG, "Audio read failed: %d", ret);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Mean absolute amplitude.
        int64_t sum = 0;
        int32_t peak = 0;

        for (int i = 0; i < BUFFER_SAMPLES; i++) {
            int32_t sample = audio_buffer[i];

            if (sample < 0) {
                sample = -sample;
            }

            sum += sample;

            if (sample > peak) {
                peak = sample;
            }
        }

        int32_t average = (int32_t)(sum / BUFFER_SAMPLES);

        ESP_LOGI(TAG,
                 "Audio level: %ld | Peak: %ld",
                 (long)average,
                 (long)peak);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

