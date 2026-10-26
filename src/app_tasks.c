#include "app_tasks.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "sensors.h"
#include "monitoring.h"
#include <stdio.h>
#include "notifications.h"
#include "connectivity.h"

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

        reading.time_valid = connectivity_time_is_ready();

if (reading.time_valid) {
    reading.measured_at = time(NULL);
}

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

        char message[384];
        int length;

        char measured_time[48] = "unavailable (clock not synchronized)";

if (event.reading.time_valid) {
    struct tm utc;

    gmtime_r(&event.reading.measured_at, &utc);
    strftime(measured_time, sizeof(measured_time),
             "%Y-%m-%d %H:%M:%S UTC", &utc);
}

        if (event.reading.dht_result == ESP_OK) {
            length = snprintf(
                message, sizeof(message),
                "Plant-01\n"
                "Measured: %s\n"
                "Reason: confirmed moisture state\n"
                "State: %s\n"
                "Simulated moisture: %d%%\n"
                "Temperature: %.1f C\n"
                "Humidity: %.1f%%",
                measured_time, event.state, event.moisture_percent,
                event.reading.temperature,
                event.reading.humidity);
        } else {
            length = snprintf(
                message, sizeof(message),
                "Plant-01\n"
                "Reason: confirmed moisture state\n"
                "State: %s\n"
                "Simulated moisture: %d%%\n"
                "Temperature/humidity: unavailable",
                event.state, event.moisture_percent);
        }

        if (length < 0 || length >= (int)sizeof(message)) {
            ESP_LOGE("NOTIFY", "Message formatting failed");
            continue;
        }

        esp_err_t result = notifications_send_slack(message);

        if (result == ESP_OK) {
            ESP_LOGI("NOTIFY", "Slack request accepted");
        } else {
            ESP_LOGW("NOTIFY", "Slack send failed: %s",
                     esp_err_to_name(result));
        }

        esp_err_t discord_result = notifications_send_discord(message);

if (discord_result == ESP_OK) {
    ESP_LOGI("NOTIFY", "Discord request accepted");
} else {
    ESP_LOGW("NOTIFY", "Discord send failed: %s",
             esp_err_to_name(discord_result));
}
esp_err_t telegram_result = notifications_send_telegram(message);

if (telegram_result == ESP_OK) {
    ESP_LOGI("NOTIFY", "Telegram request accepted");
} else {
    ESP_LOGW("NOTIFY", "Telegram send failed: %s",
             esp_err_to_name(telegram_result));
}

esp_err_t gmail_result = notifications_send_gmail(message);

if (gmail_result == ESP_OK) {
    ESP_LOGI("NOTIFY", "Gmail SMTP submission accepted");
} else {
    ESP_LOGW("NOTIFY", "Gmail send failed: %s",
             esp_err_to_name(gmail_result));
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
    notification_task, "notifications", 8192, NULL, 3, NULL);

ESP_ERROR_CHECK(result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}