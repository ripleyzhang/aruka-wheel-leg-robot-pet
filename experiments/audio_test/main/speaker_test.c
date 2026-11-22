
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "bsp/esp-box-3.h"
#include "esp_codec_dev.h"
#include "driver/gpio.h"

#define SAMPLE_RATE 16000
#define TONE_FREQ 400
#define TONE_DURATION_MS 400
#define BUFFER_SAMPLES 256
#define AMPLITUDE 5000

static const char *TAG = "ARUKA_SPEAKER";

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing speaker...");

    esp_codec_dev_handle_t speaker =
        bsp_audio_codec_speaker_init();

    if (speaker == NULL) {
        ESP_LOGE(TAG, "Speaker initialization failed!");
        return;
    }

    // Enable the onboard speaker amplifier
gpio_config_t amp_config = {
    .pin_bit_mask = 1ULL << BSP_POWER_AMP_IO,
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
};

ESP_ERROR_CHECK(gpio_config(&amp_config));
ESP_ERROR_CHECK(gpio_set_level(BSP_POWER_AMP_IO, 1));

ESP_LOGI(TAG, "Speaker amplifier enabled");

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = SAMPLE_RATE,
        .channel = 1,
        .bits_per_sample = 16,
    };

    int ret = esp_codec_dev_open(speaker, &fs);

    if (ret != 0) {
        ESP_LOGE(TAG, "Cannot open speaker: %d", ret);
        return;
    }

    // Start with a low volume.
    ret = esp_codec_dev_set_out_vol(speaker, 60);

    if (ret != 0) {
        ESP_LOGW(TAG, "Volume setting failed: %d", ret);
    }

    ESP_LOGI(TAG, "Speaker ready!");
    ESP_LOGI(TAG, "Playing a short test tone...");

    int16_t buffer[BUFFER_SAMPLES];

    // 400 Hz at 16 kHz = 40 samples per cycle.
    const int period = SAMPLE_RATE / TONE_FREQ;
    const int total_samples =
        SAMPLE_RATE * TONE_DURATION_MS / 1000;

    int sample_index = 0;

    while (sample_index < total_samples) {
        int count = BUFFER_SAMPLES;

        if (total_samples - sample_index < count) {
            count = total_samples - sample_index;
        }

        for (int i = 0; i < count; i++) {
            int phase = (sample_index + i) % period;

            // Generate a low-amplitude triangle wave.
            int value;

            if (phase < period / 2) {
                value = -AMPLITUDE +
                    (4 * AMPLITUDE * phase) / period;
            } else {
                value = 3 * AMPLITUDE -
                    (4 * AMPLITUDE * phase) / period;
            }

            buffer[i] = (int16_t)value;
        }

        ret = esp_codec_dev_write(
            speaker,
            buffer,
            count * sizeof(int16_t)
        );

        if (ret != 0) {
            ESP_LOGE(TAG, "Playback failed: %d", ret);
            break;
        }

        sample_index += count;
    }

    ESP_LOGI(TAG, "Playback finished.");

    // Allow buffered output to finish before closing.
    vTaskDelay(pdMS_TO_TICKS(200));

    esp_codec_dev_close(speaker);

    ESP_LOGI(TAG, "Speaker test completed.");
}

