#include "app_tasks.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "sensors.h"
#include "monitoring.h"
#include <stdio.h>

typedef struct {
    sensor_reading_t reading;
    int moisture_percent;
    char state[8];
} notification_event_t;

static QueueHandle_t reading_queue;
static QueueHandle_t notification_queue;

static void sensor_task(void *parameter)
{
    (void)parameter;

    sensors_init();

    uint32_t sequence = 0;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        sensor_reading_t reading = {0};
        reading.sequence = sequence++;

        reading.pot_result =
            sensors_read_potentiometer(&reading.raw);

        reading.dht_result =
            sensors_read_dht(&reading.humidity,
                             &reading.temperature);

        if (xQueueSend(reading_queue, &reading, 0) != pdTRUE) {
            ESP_LOGW("SENSORS",
                     "Reading queue full; sample dropped");
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(5000));
    }
}

static void monitoring_task(void *parameter)
{
    (void)parameter;

    monitoring_t monitor;
    monitoring_init(&monitor);

    sensor_reading_t reading;
    uint32_t expected_sequence = 0;

    while (1) {
        if (xQueueReceive(reading_queue, &reading,
                          portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (reading.sequence != expected_sequence) {
            monitoring_reset_streak(&monitor);
            ESP_LOGW("MONITOR",
                     "Sample gap; confirmation streak reset");
        }
        expected_sequence = reading.sequence + 1;

        if (reading.pot_result == ESP_OK) {
            int percent = monitoring_percent(reading.raw);
            const char *state = monitoring_classify(percent);

            ESP_LOGI("POT",
                     "Raw: %d | Simulated moisture: %d%% | State: %s",
                     reading.raw, percent, state);

            bool changed = monitoring_update(&monitor, state);

            ESP_LOGI("MONITOR", "Candidate: %s | Count: %d/3",
                     monitor.candidate_state,
                     monitor.consecutive_count);

           if (changed) {
    ESP_LOGI("MONITOR", "Confirmed state: %s",
             monitor.confirmed_state);

    notification_event_t event = {
        .reading = reading,
        .moisture_percent = percent,
    };

    snprintf(event.state, sizeof(event.state),
             "%s", monitor.confirmed_state);

    if (xQueueSend(notification_queue, &event, 0) != pdTRUE) {
        ESP_LOGW("NOTIFY",
                 "Notification queue full; event dropped");
    }
}
        } else {
            monitoring_reset_streak(&monitor);
            ESP_LOGE("POT", "Read failed: %s",
                     esp_err_to_name(reading.pot_result));
        }

        if (reading.dht_result == ESP_OK) {
            ESP_LOGI("DHT11",
                     "Temperature: %.1f C | Humidity: %.1f %%",
                     reading.temperature, reading.humidity);
        } else {
            ESP_LOGE("DHT11", "Read failed: %s",
                     esp_err_to_name(reading.dht_result));
        }
    }
}

static void notification_task(void *parameter)
{
    (void)parameter;

    notification_event_t event;

    while (1) {
        if (xQueueReceive(notification_queue, &event,
                          portMAX_DELAY) != pdTRUE) {
            continue;
        }

        ESP_LOGI("NOTIFY",
                 "Local event only | State: %s | Simulated moisture: %d%%",
                 event.state, event.moisture_percent);

        if (event.reading.dht_result == ESP_OK) {
            ESP_LOGI("NOTIFY",
                     "Temperature: %.1f C | Humidity: %.1f %%",
                     event.reading.temperature,
                     event.reading.humidity);
        } else {
            ESP_LOGW("NOTIFY",
                     "Temperature/humidity unavailable: %s",
                     esp_err_to_name(event.reading.dht_result));
        }
    }
}

void app_tasks_start(void)
{
    reading_queue = xQueueCreate(4, sizeof(sensor_reading_t));

    if (reading_queue == NULL) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }

    notification_queue = xQueueCreate(
    4, sizeof(notification_event_t));

if (notification_queue == NULL) {
    ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
}

    BaseType_t result = xTaskCreate(
        monitoring_task, "monitoring", 4096, NULL, 4, NULL);

    ESP_ERROR_CHECK(result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    result = xTaskCreate(
        sensor_task, "sensors", 4096, NULL, 5, NULL);

    ESP_ERROR_CHECK(result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    result = xTaskCreate(
    notification_task, "notifications", 4096, NULL, 3, NULL);

ESP_ERROR_CHECK(result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}