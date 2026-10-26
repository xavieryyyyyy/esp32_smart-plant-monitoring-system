/*
 * This file reads the actual hardware for the sensor task in app_tasks.c.
 * Potentiometer: middle pin to GPIO1, outer pins to 3V3 and GND.
 * DHT11: S to GPIO2, VCC to 3V3, minus to GND.
 * Return the measured values here; monitoring.c decides DRY/NORMAL/WET later.
 */
#include "sensors.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "dht.h"
#include <math.h>

/* Keep the ADC connection here so the sensor task can reuse it for every reading. */
static adc_oneshot_unit_handle_t adc;

void sensors_init(void)
{
    /* Prepare ADC unit 1, which turns the potentiometer voltage into a number. */
    adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_config, &adc));

    /* Use channel 0 on GPIO1. Twelve-bit readings range from 0 to 4095. */
    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, ADC_CHANNEL_0, &channel_config));

    /*
     * Set the DHT signal high before changing it to an input.
     * This order helped the first read in our tests. The suspected brief low pulse
     * has not been checked with a logic analyzer.
     */
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_2, 1));
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_2, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY));

    /* Give the DHT11 two seconds to settle before the first reading. */
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI("DHT11", "Idle signal level: %d", gpio_get_level(GPIO_NUM_2));
}

esp_err_t sensors_read_potentiometer(int *raw)
{
    /* The caller must provide a variable where we can store the result. */
    if (raw == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * Read into a temporary variable first. If the read fails, leave the caller's value alone.
     */
    int measured_raw;

    esp_err_t result = adc_oneshot_read(adc, ADC_CHANNEL_0, &measured_raw);

    if (result != ESP_OK) {
        return result;
    }

    if (measured_raw < 0 || measured_raw > 4095) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* The read worked and is in range. Now copy it into the caller's variable. */
    *raw = measured_raw;
    return ESP_OK;
}

esp_err_t sensors_read_dht(float *humidity, float *temperature)
{
    if (humidity == NULL || temperature == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * Read into temporary variables so we can check both values before giving them back.
     */
    float measured_humidity;
    float measured_temperature;

    esp_err_t result =
        dht_read_float_data(DHT_TYPE_DHT11, GPIO_NUM_2, &measured_humidity, &measured_temperature);

    if (result != ESP_OK) {
        return result;
    }

    /*
     * Reject NaN/infinity and humidity below 0% or above 100%.
     * These checks do not test sensor accuracy or enforce a physical temperature range.
     */
    if (!isfinite(measured_humidity) || !isfinite(measured_temperature) ||
        measured_humidity < 0.0f || measured_humidity > 100.0f) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* Both checks passed. Give the caller the new humidity and temperature together. */
    *humidity = measured_humidity;
    *temperature = measured_temperature;

    return ESP_OK;
}