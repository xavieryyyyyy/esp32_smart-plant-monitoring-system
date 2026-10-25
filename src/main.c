#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "dht.h"


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
    
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_2, 1));
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_2, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY));
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_2, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY));  

    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI("DHT11", "Idle signal level: %d",
         gpio_get_level(GPIO_NUM_2));

    while (1) {
        int raw;

        esp_err_t result = adc_oneshot_read(
            adc, ADC_CHANNEL_0, &raw);

        if (result == ESP_OK) {
            int moisture_percent = (raw * 100) / 4095;
            
            const char *state;

            if (moisture_percent < 30) {
                state = "DRY";
            } else if (moisture_percent > 70) {
                state = "WET";
            } else {
                state = "NORMAL";
            }
            ESP_LOGI("POT", "Raw: %d | Simulated moisture: %d%% | State: %s",
            raw, moisture_percent, state);
        } else {
            ESP_LOGE("POT", "Read failed: %s",
                     esp_err_to_name(result));
        }

        float humidity;
        float temperature;

        esp_err_t dht_result = dht_read_float_data(
            DHT_TYPE_DHT11, GPIO_NUM_2, &humidity, &temperature);

        if (dht_result == ESP_OK) {
        ESP_LOGI("DHT11", "Temperature: %.1f C | Humidity: %.1f %%",
             temperature, humidity);
        } else {
             ESP_LOGE("DHT11", "Read failed: %s",
             esp_err_to_name(dht_result));
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}