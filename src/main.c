#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_log.h"

void app_main(void)
{
    adc_oneshot_unit_handle_t adc;

    adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_config, &adc));

    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(
        adc, ADC_CHANNEL_0, &channel_config));

    while (1) {
        int raw;

        esp_err_t result = adc_oneshot_read(
            adc, ADC_CHANNEL_0, &raw);

        if (result == ESP_OK) {
            ESP_LOGI("POT", "Raw reading: %d", raw);
        } else {
            ESP_LOGE("POT", "Read failed: %s",
                     esp_err_to_name(result));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}