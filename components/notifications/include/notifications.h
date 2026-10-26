/*
 * app_tasks.c calls these functions from its notification task.
 * Each function uses the supplied message while it sends, then returns a result.
 * ESP_OK means the service accepted the request; check the app/inbox to see the message.
 * Senders read their passwords, tokens, and addresses from the ignored config.local.h file.
 */
#pragma once

/* Use ESP-IDF result codes: ESP_OK means success; other codes describe failures. */
#include "esp_err.h"
/* Use bool, true, and false for yes/no values. */
#include <stdbool.h>
/* Store Unix timestamps and convert them into calendar date/time fields. */
#include <time.h>

/*
 * Keep the values from one captured event together. Each sender chooses its own layout.
 * measured_at remains the original UTC Unix time; only its displayed text becomes PhST.
 * state points to the caller's word and must stay available until the send returns.
 */
typedef struct {
    /* Borrow the caller's state word; keep that word alive until the sending call returns. */
    const char *state;
    /* Keep the simulated knob percentage (0..100) paired with this captured event. */
    int moisture_percent;
    /*
     * Store air temperature in degrees Celsius; its read-validity flag determines whether it is
     * usable.
     */
    float temperature;
    /*
     * Store relative air humidity as a percentage; this is separate from simulated soil
     * moisture.
     */
    float humidity;
    /*
     * True means both captured DHT values are usable; false makes the formatter show
     * Unavailable.
     */
    bool environment_valid;
    /* Store the original captured Unix timestamp in seconds; format a copy when displaying PhST. */
    time_t measured_at;
    /* Remember whether the clock was already synchronized when this measurement began. */
    bool time_valid;
} notification_message_t;

/* Apply Slack wait/retry rules around at most three attempts for this one queued event. */
esp_err_t notifications_send_slack(const notification_message_t *message);
/* Send one native Discord embed through the private webhook and check HTTP acceptance. */
esp_err_t notifications_send_discord(const notification_message_t *message);
/* Send one Telegram message and require both HTTP 200 and the reply field ok=true. */
esp_err_t notifications_send_telegram(const notification_message_t *message);
/* Submit one complete email using certificate-checked TLS and the SMTP command/reply sequence. */
esp_err_t notifications_send_gmail(const notification_message_t *message);
