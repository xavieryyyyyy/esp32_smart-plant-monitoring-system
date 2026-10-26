/*
 * Shared message layouts used by the four sender files.
 * These functions only prepare text/JSON; they never connect to a service or read secrets.
 * The offline tests use these same functions to check the displayed values.
 */
#pragma once
#include "notifications.h"
#include "cJSON.h"
#include <stddef.h>

#define NOTIFICATION_TEXT_CAPACITY 768
#define NOTIFICATION_HTML_CAPACITY 6144

esp_err_t notification_format_time(const notification_message_t *message, char *text, size_t size);
esp_err_t notification_format_text(const notification_message_t *message, bool html, char *text,
                                   size_t size);
esp_err_t notification_format_gmail(const notification_message_t *message, char *html, size_t size);

/* The caller owns a successful JSON result and releases it using cJSON_Delete(). */
esp_err_t notification_build_slack(const notification_message_t *message, cJSON **json);
esp_err_t notification_build_discord(const notification_message_t *message, cJSON **json);
esp_err_t notification_build_telegram(const notification_message_t *message, const char *chat_id,
                                      cJSON **json);

/* Small inline email image. It is stored as Base64, not fetched from a website. */
extern const char notification_mark_base64[];
