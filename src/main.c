#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sensors.h"
#include "monitoring.h"


void app_main(void)
{
    sensors_init();

    monitoring_t monitor;
    monitoring_init(&monitor);

    while (1) {
        int raw;

        esp_err_t result = sensors_read_potentiometer(&raw);

       if (result == ESP_OK) {
        int moisture_percent = monitoring_percent(raw);
        const char *state = monitoring_classify(moisture_percent);

        ESP_LOGI("POT", "Raw: %d | Simulated moisture: %d%% | State: %s",
             raw, moisture_percent, state);

/* Step 4: count consecutive matching readings. */
bool state_changed = monitoring_update(&monitor, state);

ESP_LOGI("MONITOR", "Candidate: %s | Count: %d/3",
         monitor.candidate_state, monitor.consecutive_count);

if (state_changed) {
    ESP_LOGI("MONITOR", "Confirmed state: %s",
             monitor.confirmed_state);
}

} else {
    /* Step 5: an ADC error breaks the streak. */
   monitoring_reset_streak(&monitor);


    ESP_LOGE("POT", "Read failed: %s",
             esp_err_to_name(result));
}

        float humidity;
        float temperature;

        esp_err_t dht_result = sensors_read_dht(&humidity, &temperature);

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