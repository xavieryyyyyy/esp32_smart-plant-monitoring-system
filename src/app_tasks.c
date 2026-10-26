/*
 * This file connects the main parts of the program:
 * 1. Read the sensors using sensors.c.
 * 2. Check the readings using monitoring.c.
 * 3. Send a message using the four notification files.
 * Each step has its own FreeRTOS task: a small job that runs alongside other jobs.
 * The jobs pass copies of their data through queues, which are waiting lists.
 */
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
#include "esp_timer.h"

/*
 * Everything needed for one notification is kept together here.
 * Copy the reading too, so a newer sensor reading cannot change this message.
 */
typedef struct {
    sensor_reading_t reading;
    int moisture_percent;
    char state[8];
} notification_event_t;

static QueueHandle_t reading_queue;
static QueueHandle_t notification_queue;

/*
 * Job 1: read the potentiometer and DHT11 every five seconds.
 * This is the only application task that talks directly to the sensor hardware.
 */
static void sensor_task(void *parameter)
{
    (void)parameter;

    sensors_init();

    uint32_t sequence = 0;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        /* Start with an empty reading, then give it a sequence number: 0, 1, 2, ... */
        sensor_reading_t reading = {0};
        reading.sequence = sequence++;
        /*
         * Save when we started this reading, in microseconds since the ESP32 started.
         * This timer lets us check its age even before Internet time is available.
         * 1,000,000 microseconds = one second.
         */
        reading.sampled_at_us = esp_timer_get_time();

        /* Has the ESP32 already learned the current date and time from the Internet? */
        reading.time_valid = connectivity_time_is_ready();

        if (reading.time_valid) {
            reading.measured_at = time(NULL);
        }

        /* Read the knob value into reading.raw. Save whether that read worked too. */
        reading.pot_result = sensors_read_potentiometer(&reading.raw);

        /*
         * Read humidity and temperature. Keep this result separate from the knob result.
         */
        reading.dht_result = sensors_read_dht(&reading.humidity, &reading.temperature);

        /*
         * Put a copy of this reading in the queue for the monitoring task.
         * The final 0 means do not wait for space. If full, skip this reading and log it.
         * Its missing sequence number will tell monitoring that a reading was skipped.
         */
        if (xQueueSend(reading_queue, &reading, 0) != pdTRUE) {
            ESP_LOGW("SENSORS", "Reading queue full; sample dropped");
        }

        /* Wait until the next five-second reading time, giving other tasks time to run. */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(5000));
    }
}

/*
 * Job 2: receive readings, decide DRY/NORMAL/WET, and check for a confirmed change.
 * Send each allowed change to the notification task through notification_queue.
 */
static void monitoring_task(void *parameter)
{
    (void)parameter;

    monitoring_t monitor;
    monitoring_init(&monitor);

    sensor_reading_t reading;
    uint32_t expected_sequence = 0;

    while (1) {
        /*
         * Wait here until the sensor task puts a reading in the queue. Waiting uses no busy loop.
         */
        if (xQueueReceive(reading_queue, &reading, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        /*
         * Example: if we expected reading 5 but received reading 7, some data is missing.
         * Start the three-reading count again so a gap cannot count as a steady condition.
         */
        if (reading.sequence != expected_sequence) {
            monitoring_reset_streak(&monitor);
            ESP_LOGW("MONITOR", "Sample gap; confirmation streak reset");
        }
        expected_sequence = reading.sequence + 1;

        int64_t now_us = esp_timer_get_time();
        /*
         * Work out how old this reading is now.
         * This includes the time spent reading the sensors and waiting in the queue.
         */
        int64_t age_us = now_us - reading.sampled_at_us;

        /*
         * Skip readings older than ten seconds, or with a time that is in the future.
         * Start the confirmation count again. continue skips the rest of this loop.
         */
        if (age_us < 0 || age_us > 10LL * 1000000LL) {
            monitoring_reset_streak(&monitor);
            ESP_LOGW("MONITOR", "Stale or invalid sample timestamp; streak reset");
            continue;
        }

        /* ESP_OK means this potentiometer read succeeded and passed the sensor checks. */
        if (reading.pot_result == ESP_OK) {
            int percent = monitoring_percent(reading.raw);
            const char *state = monitoring_classify(percent);

            ESP_LOGI("POT", "Raw: %d | Simulated moisture: %d%% | State: %s", reading.raw, percent,
                     state);

            /*
             * Count matching states. changed becomes true only after a new state is confirmed.
             */
            bool changed = monitoring_update(&monitor, state);

            ESP_LOGI("MONITOR", "Candidate: %s | Count: %d/3", monitor.candidate_state,
                     monitor.consecutive_count);

            if (changed) {
                ESP_LOGI("MONITOR", "Confirmed state: %s", monitor.confirmed_state);

                /*
                 * If this DRY/WET state was queued too recently, skip its repeated alert.
                 */
                if (!monitoring_notification_due(&monitor, monitor.confirmed_state, now_us)) {
                    ESP_LOGI("NOTIFY", "Cooldown suppressed repeated %s alert",
                             monitor.confirmed_state);
                } else {
                    /*
                     * Keep the confirmed state and this reading together for the message-sending job.
                     */
                    notification_event_t event = {
                        .reading = reading,
                        .moisture_percent = percent,
                    };

                    snprintf(event.state, sizeof(event.state), "%s", monitor.confirmed_state);

                    /*
                     * Start the cooldown only if the notification queue accepted this event.
                     * This means queued, not delivered: a later send failure still uses the cooldown.
                     * Failed sends are not automatically put back in the queue.
                     */
                    if (xQueueSend(notification_queue, &event, 0) == pdTRUE) {
                        monitoring_record_notification(&monitor, monitor.confirmed_state, now_us);
                    } else {
                        ESP_LOGW("NOTIFY", "Notification queue full; event dropped");
                    }
                }
            }

        } else {
            monitoring_reset_streak(&monitor);
            ESP_LOGE("POT", "Read failed: %s", esp_err_to_name(reading.pot_result));
        }

        /*
         * Check the DHT11 separately. Its failure does not erase a good potentiometer reading.
         * Print temperature/humidity only when that particular read succeeded.
         */
        if (reading.dht_result == ESP_OK) {
            ESP_LOGI("DHT11", "Temperature: %.1f C | Humidity: %.1f %%", reading.temperature,
                     reading.humidity);
        } else {
            ESP_LOGE("DHT11", "Read failed: %s", esp_err_to_name(reading.dht_result));
        }
    }
}

/*
 * Job 3: wait for an event, build its message, then try all four services in order.
 * Network requests can take time; the sensor task can keep running meanwhile.
 */
static void notification_task(void *parameter)
{
    (void)parameter;

    notification_event_t event;

    while (1) {
        /* Sleep until monitoring has a confirmed event ready to send. */
        if (xQueueReceive(notification_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        /*
         * Leave room for the finished message. snprintf below checks the available space.
         */
        char message[384];
        int length;

        char measured_time[48] = "unavailable (clock not synchronized)";

        /*
         * Show when the sensors were read, even if this message waited in the queue.
         * UTC is the shared world-time format used in all four messages.
         */
        if (event.reading.time_valid) {
            struct tm utc;

            gmtime_r(&event.reading.measured_at, &utc);
            strftime(measured_time, sizeof(measured_time), "%Y-%m-%d %H:%M:%S UTC", &utc);
        }

        if (event.reading.dht_result == ESP_OK) {
            length = snprintf(message, sizeof(message),
                              "Plant-01\n"
                              "Measured: %s\n"
                              "Reason: confirmed moisture state\n"
                              "State: %s\n"
                              "Simulated moisture: %d%%\n"
                              "Temperature: %.1f C\n"
                              "Humidity: %.1f%%",
                              measured_time, event.state, event.moisture_percent,
                              event.reading.temperature, event.reading.humidity);
        } else {
            length =
                snprintf(message, sizeof(message),
                         "Plant-01\n"
                         "Reason: confirmed moisture state\n"
                         "State: %s\n"
                         "Simulated moisture: %d%%\n"
                         /* This error message currently leaves out the measurement-time line. */
                         "Temperature/humidity: unavailable",
                         event.state, event.moisture_percent);
        }

        /* Do not send a broken or cut-off message if it did not fit in the array. */
        if (length < 0 || length >= (int)sizeof(message)) {
            ESP_LOGE("NOTIFY", "Message formatting failed");
            continue;
        }

        /*
         * Send the same message to Slack, Discord, Telegram, then Gmail.
         * If one service fails, log that failure and still try the next service.
         */
        esp_err_t result = notifications_send_slack(message);

        if (result == ESP_OK) {
            ESP_LOGI("NOTIFY", "Slack request accepted");
        } else {
            ESP_LOGW("NOTIFY", "Slack send failed: %s", esp_err_to_name(result));
        }

        esp_err_t discord_result = notifications_send_discord(message);

        if (discord_result == ESP_OK) {
            ESP_LOGI("NOTIFY", "Discord request accepted");
        } else {
            ESP_LOGW("NOTIFY", "Discord send failed: %s", esp_err_to_name(discord_result));
        }
        esp_err_t telegram_result = notifications_send_telegram(message);

        if (telegram_result == ESP_OK) {
            ESP_LOGI("NOTIFY", "Telegram request accepted");
        } else {
            ESP_LOGW("NOTIFY", "Telegram send failed: %s", esp_err_to_name(telegram_result));
        }

        esp_err_t gmail_result = notifications_send_gmail(message);

        if (gmail_result == ESP_OK) {
            ESP_LOGI("NOTIFY", "Gmail SMTP submission accepted");
        } else {
            ESP_LOGW("NOTIFY", "Gmail send failed: %s", esp_err_to_name(gmail_result));
        }
    }
}

/*
 * Make the queues first, then start the jobs that use them.
 * That way, no job tries to use a queue before it exists.
 */
void app_tasks_start(void)
{
    /* This waiting list can hold four sensor readings. Each item is a complete copy. */
    reading_queue = xQueueCreate(4, sizeof(sensor_reading_t));

    if (reading_queue == NULL) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }

    /* This second waiting list can hold four messages waiting to be prepared and sent. */
    notification_queue = xQueueCreate(4, sizeof(notification_event_t));

    if (notification_queue == NULL) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }

    /* Give monitoring 4096 bytes of task-stack space and priority 4. */
    BaseType_t result = xTaskCreate(monitoring_task, "monitoring", 4096, NULL, 4, NULL);

    ESP_ERROR_CHECK(result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    /* Give sensing priority 5, higher than monitoring (4) and sending (3). */
    result = xTaskCreate(sensor_task, "sensors", 4096, NULL, 5, NULL);

    ESP_ERROR_CHECK(result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    /* Give sending more stack space: building messages and network calls need room. */
    result = xTaskCreate(notification_task, "notifications", 8192, NULL, 3, NULL);

    ESP_ERROR_CHECK(result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
