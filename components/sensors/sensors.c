/*
 * This file reads the actual hardware for the sensor task in app_tasks.c.
 * Potentiometer: middle pin to GPIO1, outer pins to 3V3 and GND.
 * DHT11: S to GPIO2, VCC to 3V3, minus to GND.
 * Return the measured values here; monitoring.c decides DRY/NORMAL/WET later.
 */
/*
 * Pointer/result key:
 * int *raw and float *humidity/temperature identify variables owned by the caller.
 * &temporary gives the driver somewhere safe to put a reading before we validate it.
 * *raw = measured_raw writes through that address into the caller's variable.
 * On an error path we return the error and leave those output variables unchanged.
 */
#include "sensors.h"

/* Use FreeRTOS types and constants for tasks, queues, and waiting. */
#include "freertos/FreeRTOS.h"
/* Create background jobs, pause them, or wake them with task notifications. */
#include "freertos/task.h"
/* Take individual analog-to-digital readings from the potentiometer. */
#include "esp_adc/adc_oneshot.h"
/* Configure the GPIO2 signal pin used by the DHT11. */
#include "driver/gpio.h"
/* Print tagged information, warnings, and errors in the serial monitor. */
#include "esp_log.h"
/* Use the installed DHT driver to read air humidity and temperature. */
#include "dht.h"
/* Check for invalid floating-point values, including NaN and infinity. */
#include <math.h>

/* Keep the ADC connection here so the sensor task can reuse it for every reading. */
static adc_oneshot_unit_handle_t adc;

/* Prepare ADC1/GPIO1 and the GPIO2 DHT11 signal before sensor_task starts sampling. */
void sensors_init(void)
{
    /* Prepare ADC unit 1, which turns the potentiometer voltage into a number. */
    adc_oneshot_unit_init_cfg_t unit_config = {
        /* Select ADC unit 1, the peripheral connected to GPIO1/channel 0 on this board. */
        .unit_id = ADC_UNIT_1,
    };
    /*
     * Create ADC unit 1 and save its handle for repeated potentiometer reads. Stop on a setup
     * error.
     */
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_config, &adc));

    /* Use channel 0 on GPIO1. Twelve-bit readings range from 0 to 4095. */
    adc_oneshot_chan_cfg_t channel_config = {
        /* Use 12 dB ADC attenuation; do not exceed the board/pin's electrical voltage limits. */
        .atten = ADC_ATTEN_DB_12,
        /* Select 12-bit conversion, whose raw output range is 0..4095. */
        .bitwidth = ADC_BITWIDTH_12,
    };
    /*
     * Apply the selected attenuation and twelve-bit resolution to ADC channel 0 (GPIO1). Stop on
     * a setup error.
     */
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, ADC_CHANNEL_0, &channel_config));

    /*
     * Set the DHT signal high before changing it to an input.
     * This order helped the first read in our tests. The suspected brief low pulse
     * has not been checked with a logic analyzer.
     */
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_2, 1));
    /*
     * Make GPIO2 an input so the DHT11 can drive its response on the signal wire. Stop on a
     * setup error.
     */
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_2, GPIO_MODE_INPUT));
    /*
     * Enable the internal pull-up so an undriven DHT signal tends toward HIGH. Stop on a setup
     * error.
     */
    ESP_ERROR_CHECK(gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY));

    /* Give the DHT11 two seconds to settle before the first reading. */
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* Print an informational progress/result line with a tag for the serial monitor. */
    ESP_LOGI("DHT11", "Idle signal level: %d", gpio_get_level(GPIO_NUM_2));
}

/* Read one knob value, validate it, then write the result through the caller's pointer. */
esp_err_t sensors_read_potentiometer(int *raw)
{
    /* The caller must provide a variable where we can store the result. */
    if (raw == NULL) {
        /* Reject an absent, unsafe, or out-of-range input before using it. */
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * Read into a temporary variable first. If the read fails, leave the caller's value alone.
     */
    int measured_raw;

    /* Keep this operation's success/error code so later steps can stop safely on failure. */
    esp_err_t result = adc_oneshot_read(adc, ADC_CHANNEL_0, &measured_raw);

    /* Handle/return the earlier failure instead of continuing with invalid output. */
    if (result != ESP_OK) {
        /* Pass the actual success/error code back to the caller; do not replace it with success. */
        return result;
    }

    /* A 12-bit reading must be within 0..4095 before it is published. */
    if (measured_raw < 0 || measured_raw > 4095) {
        /* Reject a sensor value that the driver returned but our plausibility check disallows. */
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* The read worked and is in range. Now copy it into the caller's variable. */
    *raw = measured_raw;
    /* Finish successfully; the caller may now use this function's output. */
    return ESP_OK;
}

/*
 * Read both air measurements into temporary variables, then publish them together after
 * validation.
 */
esp_err_t sensors_read_dht(float *humidity, float *temperature)
{
    /* Check for an absent pointer or failed allocation before dereferencing/using it. */
    if (humidity == NULL || temperature == NULL) {
        /* Reject an absent, unsafe, or out-of-range input before using it. */
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * Read into temporary variables so we can check both values before giving them back.
     */
    float measured_humidity;
    /* Temporarily store the driver's temperature value until both DHT values pass validation. */
    float measured_temperature;

    /* Keep this operation's success/error code so later steps can stop safely on failure. */
    esp_err_t result =
        /*
         * Ask the DHT11 driver for both values; the & arguments let it fill our temporary
         * variables.
         */
        dht_read_float_data(DHT_TYPE_DHT11, GPIO_NUM_2, &measured_humidity, &measured_temperature);

    /* Handle/return the earlier failure instead of continuing with invalid output. */
    if (result != ESP_OK) {
        /* Pass the actual success/error code back to the caller; do not replace it with success. */
        return result;
    }

    /*
     * Reject NaN/infinity and humidity below 0% or above 100%.
     * These checks do not test sensor accuracy or enforce a physical temperature range.
     */
    if (!isfinite(measured_humidity) || !isfinite(measured_temperature) ||
        measured_humidity < 0.0f || measured_humidity > 100.0f) {
        /* Reject a sensor value that the driver returned but our plausibility check disallows. */
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* Both checks passed. Give the caller the new humidity and temperature together. */
    *humidity = measured_humidity;
    /* Publish the matching validated temperature from the same successful DHT read. */
    *temperature = measured_temperature;

    /* Finish successfully; the caller may now use this function's output. */
    return ESP_OK;
}