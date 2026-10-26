/*
 * app_tasks.c calls these functions from its notification task.
 * Each function uses the supplied message while it sends, then returns a result.
 * ESP_OK means the service accepted the request; check the app/inbox to see the message.
 * Senders read their passwords, tokens, and addresses from the ignored config.local.h file.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <time.h>

/*
 * Keep the values from one captured event together. Each sender chooses its own layout.
 * measured_at remains the original UTC Unix time; only its displayed text becomes PhST.
 * state points to the caller's word and must stay available until the send returns.
 */
typedef struct {
    const char *state;
    int moisture_percent;
    float temperature;
    float humidity;
    bool environment_valid;
    time_t measured_at;
    bool time_valid;
} notification_message_t;

esp_err_t notifications_send_slack(const notification_message_t *message);
esp_err_t notifications_send_discord(const notification_message_t *message);
esp_err_t notifications_send_telegram(const notification_message_t *message);
esp_err_t notifications_send_gmail(const notification_message_t *message);
