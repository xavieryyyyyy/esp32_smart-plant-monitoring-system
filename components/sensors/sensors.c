#include "sensors.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "dht.h"

static adc_oneshot_unit_handle_t adc;

void sensors_init(void)
{
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

    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI("DHT11", "Idle signal level: %d",
             gpio_get_level(GPIO_NUM_2));
}

esp_err_t sensors_read_potentiometer(int *raw)
{
    if (raw == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return adc_oneshot_read(adc, ADC_CHANNEL_0, raw);
}

esp_err_t sensors_read_dht(float *humidity, float *temperature)
{
    if (humidity == NULL || temperature == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return dht_read_float_data(
        DHT_TYPE_DHT11, GPIO_NUM_2, humidity, temperature);
}