/*
 * app_tasks.c calls these functions from its notification task.
 * Each function uses the supplied message while it sends, then returns a result.
 * ESP_OK means the service accepted the request; check the app/inbox to see the message.
 * Senders read their passwords, tokens, and addresses from the ignored config.local.h file.
 */
#pragma once

#include "esp_err.h"

esp_err_t notifications_send_slack(const char *message);
esp_err_t notifications_send_discord(const char *message);
esp_err_t notifications_send_telegram(const char *message);
esp_err_t notifications_send_gmail(const char *message);