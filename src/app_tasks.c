/*
 * This file connects the main parts of the program:
 * 1. Read the sensors using sensors.c.
 * 2. Check the readings using monitoring.c.
 * 3. Send a message using the four notification files.
 * Each step has its own FreeRTOS task: a small job that runs alongside other jobs.
 * The jobs pass copies of their data through queues, which are waiting lists.
 */
/*
 * C reading key for the statements below:
 * - A struct keeps related named values together. item.field reads one of those values.
 * - &item passes the address of item, so another function can fill/read the same object.
 * - A pointer stores an address. NULL means that no usable object is at that address.
 * - sizeof(array) gives its total byte capacity, not the length of text stored inside it.
 * - A C text string ends with a zero byte, written as '\0'; reserve space for that byte.
 * - ESP_OK means a function succeeded; result codes describe what happened on failure.
 * - static at file scope keeps a name private to this C file. A task's local variables
 *   remain part of that running task; the tasks do not share the same local reading.
 * - A queue copies sizeof(item) bytes. The &item argument names the source/destination.
 * - portMAX_DELAY waits for a queue item instead of wasting CPU time checking repeatedly.
 * - pdMS_TO_TICKS converts milliseconds to the scheduler's tick units.
 * - int64_t is a wide integer for microseconds; 1,000,000 microseconds equals one second.
 * - if chooses a branch; else handles the other case. continue begins the next loop turn.
 */
#include "app_tasks.h"
/* Use FreeRTOS types and constants for tasks, queues, and waiting. */
#include "freertos/FreeRTOS.h"
/* Create background jobs, pause them, or wake them with task notifications. */
#include "freertos/task.h"
/* Pass copies of readings/events between jobs through waiting lists called queues. */
#include "freertos/queue.h"
/* Print tagged information, warnings, and errors in the serial monitor. */
#include "esp_log.h"
/* Use our sensor functions and the structure that holds one complete reading. */
#include "sensors.h"
/* Use our percentage, state-confirmation, and notification-cooldown rules. */
#include "monitoring.h"
/* Format bounded text with snprintf(), or use file/console I/O in host tests. */
#include <stdio.h>
/* Use the captured-message structure and the four service-sending functions. */
#include "notifications.h"
/* Use the networking component to start Wi-Fi or ask whether Wi-Fi/time is ready. */
#include "connectivity.h"
/* Read the microsecond timer since startup; use it for ages and waiting deadlines. */
#include "esp_timer.h"

/*
 * Everything needed for one notification is kept together here.
 * Copy the reading too, so a newer sensor reading cannot change this message.
 */
typedef struct {
    /* Reserve space for one complete set of sensor values, timestamps, and read-result codes. */
    sensor_reading_t reading;
    /* Keep the simulated knob percentage (0..100) paired with this captured event. */
    int moisture_percent;
    /*
     * Store a copy of the state word plus its final zero byte; eight bytes fit DRY, NORMAL, and
     * WET.
     */
    char state[8];
} notification_event_t;

/* Handle for the waiting list from sensor_task to monitoring_task; items are copied readings. */
static QueueHandle_t reading_queue;
/*
 * Handle for the waiting list from monitoring_task to notification_task; items are copied
 * events.
 */
static QueueHandle_t notification_queue;

/*
 * Job 1: read the potentiometer and DHT11 every five seconds.
 * This is the only application task that talks directly to the sensor hardware.
 */
static void sensor_task(void *parameter)
{
    /*
     * This callback/task must accept the parameter, but this implementation intentionally does
     * not use it.
     */
    (void)parameter;

    /*
     * Prepare the sensor hardware once before entering the repeated five-second acquisition
     * loop.
     */
    sensors_init();

    /* Count this task's sensor samples from zero so a missing sample can be detected later. */
    uint32_t sequence = 0;
    /* Remember the scheduled wake-up tick so sampling targets a steady five-second period. */
    TickType_t last_wake = xTaskGetTickCount();

    /*
     * Keep this task/read loop running; its wait, break, or error path below decides when to
     * proceed.
     */
    while (1) {
        /* Start with an empty reading, then give it a sequence number: 0, 1, 2, ... */
        sensor_reading_t reading = {0};
        /* Store this sample number, then increase the counter for the next acquisition. */
        reading.sequence = sequence++;
        /*
         * Save when we started this reading, in microseconds since the ESP32 started.
         * This timer lets us check its age even before Internet time is available.
         * 1,000,000 microseconds = one second.
         */
        reading.sampled_at_us = esp_timer_get_time();

        /* Has the ESP32 already learned the current date and time from the Internet? */
        reading.time_valid = connectivity_time_is_ready();

        /* Only save wall-clock time after the time service says the clock is synchronized. */
        if (reading.time_valid) {
            /*
             * Capture Unix seconds now; later messages must use this sample time rather than the
             * send time.
             */
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
            /* Print a warning about a recoverable problem or skipped operation. */
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
    /*
     * This callback/task must accept the parameter, but this implementation intentionally does
     * not use it.
     */
    (void)parameter;

    /*
     * Keep this task/test's candidate, confirmed state, match count, and cooldown history
     * together.
     */
    monitoring_t monitor;
    /* Initialize the monitor before counting readings or consulting its cooldown history. */
    monitoring_init(&monitor);

    /* Reserve space for one complete set of sensor values, timestamps, and read-result codes. */
    sensor_reading_t reading;
    /* Remember which sample number should arrive next; a different number breaks the streak. */
    uint32_t expected_sequence = 0;

    /*
     * Keep this task/read loop running; its wait, break, or error path below decides when to
     * proceed.
     */
    while (1) {
        /*
         * Wait here until the sensor task puts a reading in the queue. Waiting uses no busy loop.
         */
        if (xQueueReceive(reading_queue, &reading, portMAX_DELAY) != pdTRUE) {
            /* Skip the rest of this loop iteration and return to its next receive/retry step. */
            continue;
        }

        /*
         * Example: if we expected reading 5 but received reading 7, some data is missing.
         * Start the three-reading count again so a gap cannot count as a steady condition.
         */
        if (reading.sequence != expected_sequence) {
            /*
             * Discard the unfinished candidate streak because its readings are no longer
             * consecutive/valid.
             */
            monitoring_reset_streak(&monitor);
            /* Print a warning about a recoverable problem or skipped operation. */
            ESP_LOGW("MONITOR", "Sample gap; confirmation streak reset");
        }
        /* Expect the sample after this received number on the next queue receive. */
        expected_sequence = reading.sequence + 1;

        /*
         * Read elapsed microseconds since boot; this clock does not depend on Internet
         * date/time.
         */
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
            /*
             * Discard the unfinished candidate streak because its readings are no longer
             * consecutive/valid.
             */
            monitoring_reset_streak(&monitor);
            /* Print a warning about a recoverable problem or skipped operation. */
            ESP_LOGW("MONITOR", "Stale or invalid sample timestamp; streak reset");
            /* Skip the rest of this loop iteration and return to its next receive/retry step. */
            continue;
        }

        /* ESP_OK means this potentiometer read succeeded and passed the sensor checks. */
        if (reading.pot_result == ESP_OK) {
            /* Convert the valid raw knob reading to a simulated 0..100 percentage. */
            int percent = monitoring_percent(reading.raw);
            /*
             * Keep a pointer to the classification word; the word itself remains available in
             * program memory.
             */
            const char *state = monitoring_classify(percent);

            /* Print an informational progress/result line with a tag for the serial monitor. */
            ESP_LOGI("POT", "Raw: %d | Simulated moisture: %d%% | State: %s", reading.raw, percent,
                     state);

            /*
             * Count matching states. changed becomes true only after a new state is confirmed.
             */
            bool changed = monitoring_update(&monitor, state);

            /* Print an informational progress/result line with a tag for the serial monitor. */
            ESP_LOGI("MONITOR", "Candidate: %s | Count: %d/3", monitor.candidate_state,
                     monitor.consecutive_count);

            /*
             * A new state reached three consecutive matches; unchanged states do not enter this
             * branch.
             */
            if (changed) {
                /* Print an informational progress/result line with a tag for the serial monitor. */
                ESP_LOGI("MONITOR", "Confirmed state: %s", monitor.confirmed_state);

                /*
                 * If this DRY/WET state was queued too recently, skip its repeated alert.
                 */
                if (!monitoring_notification_due(&monitor, monitor.confirmed_state, now_us)) {
                    /*
                     * Print an informational progress/result line with a tag for the serial
                     * monitor.
                     */
                    ESP_LOGI("NOTIFY", "Cooldown suppressed repeated %s alert",
                             monitor.confirmed_state);
                } else {
                    /*
                     * Keep the confirmed state and this reading together for the message-sending job.
                     */
                    notification_event_t event = {
                        /* Copy the full captured reading into the queued notification event. */
                        .reading = reading,
                        /*
                         * Copy this captured event's simulated percentage, not a newer live
                         * reading.
                         */
                        .moisture_percent = percent,
                    };

                    /*
                     * Format text into a fixed-size array; check the returned required length
                     * before using it.
                     */
                    snprintf(event.state, sizeof(event.state), "%s", monitor.confirmed_state);

                    /*
                     * Start the cooldown only if the notification queue accepted this event.
                     * This means queued, not delivered: a later send failure still uses the cooldown.
                     * Failed sends are not automatically put back in the queue.
                     */
                    if (xQueueSend(notification_queue, &event, 0) == pdTRUE) {
                        /*
                         * Record when this event was queued; this starts its state's cooldown
                         * before network delivery.
                         */
                        monitoring_record_notification(&monitor, monitor.confirmed_state, now_us);
                    } else {
                        /* Print a warning about a recoverable problem or skipped operation. */
                        ESP_LOGW("NOTIFY", "Notification queue full; event dropped");
                    }
                }
            }

        } else {
            /*
             * Discard the unfinished candidate streak because its readings are no longer
             * consecutive/valid.
             */
            monitoring_reset_streak(&monitor);
            /* Print an error result so the failed step is visible in the serial monitor. */
            ESP_LOGE("POT", "Read failed: %s", esp_err_to_name(reading.pot_result));
        }

        /*
         * Check the DHT11 separately. Its failure does not erase a good potentiometer reading.
         * Print temperature/humidity only when that particular read succeeded.
         */
        if (reading.dht_result == ESP_OK) {
            /* Print an informational progress/result line with a tag for the serial monitor. */
            ESP_LOGI("DHT11", "Temperature: %.1f C | Humidity: %.1f %%", reading.temperature,
                     reading.humidity);
        } else {
            /* Print an error result so the failed step is visible in the serial monitor. */
            ESP_LOGE("DHT11", "Read failed: %s", esp_err_to_name(reading.dht_result));
        }
    }
}

/*
 * Job 3: wait for an event, keep its values together, then try all four services in order.
 * Network requests can take time; the sensor task can keep running meanwhile.
 */
static void notification_task(void *parameter)
{
    /*
     * This callback/task must accept the parameter, but this implementation intentionally does
     * not use it.
     */
    (void)parameter;

    /* Keep a confirmed state and the exact reading that produced it together for notification. */
    notification_event_t event;

    /*
     * Keep this task/read loop running; its wait, break, or error path below decides when to
     * proceed.
     */
    while (1) {
        /* Sleep until monitoring has a confirmed event ready to send. */
        if (xQueueReceive(notification_queue, &event, portMAX_DELAY) != pdTRUE) {
            /* Skip the rest of this loop iteration and return to its next receive/retry step. */
            continue;
        }

        /*
         * Keep one captured reading for all four services. Each sender chooses its own layout.
         * measured_at stays in UTC here; the shared formatter displays a PhST copy of it.
         */
        notification_message_t message = {
            /*
             * Point to the confirmed state word; the event remains alive until every synchronous
             * sender returns.
             */
            .state = event.state,
            /* Copy this captured event's simulated percentage, not a newer live reading. */
            .moisture_percent = event.moisture_percent,
            /*
             * Use temperature captured with this event; environment_valid determines whether it
             * is displayed.
             */
            .temperature = event.reading.temperature,
            /* Use humidity from the same captured reading, keeping the values paired. */
            .humidity = event.reading.humidity,
            /* Mark both environment values usable only if this event's DHT read succeeded. */
            .environment_valid = event.reading.dht_result == ESP_OK,
            /*
             * Keep the original captured Unix time; the formatter converts only its display to
             * PhST.
             */
            .measured_at = event.reading.measured_at,
            /*
             * Carry the acquisition-time validity flag instead of substituting a later
             * synchronized time.
             */
            .time_valid = event.reading.time_valid,
        };

        /*
         * Send the same message to Slack, Discord, Telegram, then Gmail.
         * If one service fails, log that failure and still try the next service.
         */
        esp_err_t result = notifications_send_slack(&message);

        /* Continue to the next step only if the preceding operation succeeded. */
        if (result == ESP_OK) {
            /* Print an informational progress/result line with a tag for the serial monitor. */
            ESP_LOGI("NOTIFY", "Slack request accepted");
        } else {
            /* Print a warning about a recoverable problem or skipped operation. */
            ESP_LOGW("NOTIFY", "Slack send failed: %s", esp_err_to_name(result));
        }

        /* Pass this captured message to the named service; return status is checked below. */
        esp_err_t discord_result = notifications_send_discord(&message);

        /* Continue to the next step only if the preceding operation succeeded. */
        if (discord_result == ESP_OK) {
            /* Print an informational progress/result line with a tag for the serial monitor. */
            ESP_LOGI("NOTIFY", "Discord request accepted");
        } else {
            /* Print a warning about a recoverable problem or skipped operation. */
            ESP_LOGW("NOTIFY", "Discord send failed: %s", esp_err_to_name(discord_result));
        }
        /* Pass this captured message to the named service; return status is checked below. */
        esp_err_t telegram_result = notifications_send_telegram(&message);

        /* Continue to the next step only if the preceding operation succeeded. */
        if (telegram_result == ESP_OK) {
            /* Print an informational progress/result line with a tag for the serial monitor. */
            ESP_LOGI("NOTIFY", "Telegram request accepted");
        } else {
            /* Print a warning about a recoverable problem or skipped operation. */
            ESP_LOGW("NOTIFY", "Telegram send failed: %s", esp_err_to_name(telegram_result));
        }

        /* Pass this captured message to the named service; return status is checked below. */
        esp_err_t gmail_result = notifications_send_gmail(&message);

        /* Continue to the next step only if the preceding operation succeeded. */
        if (gmail_result == ESP_OK) {
            /* Print an informational progress/result line with a tag for the serial monitor. */
            ESP_LOGI("NOTIFY", "Gmail SMTP submission accepted");
        } else {
            /* Print a warning about a recoverable problem or skipped operation. */
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

    /* Check for an absent pointer or failed allocation before dereferencing/using it. */
    if (reading_queue == NULL) {
        /* Check this required setup result; ESP-IDF stops/aborts if it is not ESP_OK. */
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }

    /* This second waiting list can hold four messages waiting to be prepared and sent. */
    notification_queue = xQueueCreate(4, sizeof(notification_event_t));

    /* Check for an absent pointer or failed allocation before dereferencing/using it. */
    if (notification_queue == NULL) {
        /* Check this required setup result; ESP-IDF stops/aborts if it is not ESP_OK. */
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }

    /* Give monitoring 4096 bytes of task-stack space and priority 4. */
    BaseType_t result = xTaskCreate(monitoring_task, "monitoring", 4096, NULL, 4, NULL);

    /* Check this required setup result; ESP-IDF stops/aborts if it is not ESP_OK. */
    ESP_ERROR_CHECK(result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    /* Give sensing priority 5, higher than monitoring (4) and sending (3). */
    result = xTaskCreate(sensor_task, "sensors", 4096, NULL, 5, NULL);

    /* Check this required setup result; ESP-IDF stops/aborts if it is not ESP_OK. */
    ESP_ERROR_CHECK(result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    /* Give sending more stack space: building messages and network calls need room. */
    result = xTaskCreate(notification_task, "notifications", 8192, NULL, 3, NULL);

    /* Check this required setup result; ESP-IDF stops/aborts if it is not ESP_OK. */
    ESP_ERROR_CHECK(result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
