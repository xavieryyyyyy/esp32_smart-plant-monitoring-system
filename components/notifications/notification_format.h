/*
 * Shared message layouts used by the four sender files.
 * These functions only prepare text/JSON; they never connect to a service or read secrets.
 * The offline tests use these same functions to check the displayed values.
 */
#pragma once
/* Use the captured-message structure and the four service-sending functions. */
/*
 * How the formatter interface works:
 * The caller supplies a message plus a writable text array and its byte capacity.
 * A char * names that array. const notification_message_t * lets us read the event
 * without modifying its fields. ESP_OK means the complete text fitted successfully.
 * A cJSON ** is the address of the caller's JSON pointer: a successful builder fills
 * it with one complete tree. The caller then owns that tree and must delete it.
 * Text/JSON creation here does not send anything; the four sender files do that.
 */
#include "notifications.h"
/* Build/read JSON trees; JSON is the named-field text format the chat APIs use. */
#include "cJSON.h"
/* Use size_t for byte counts and NULL for an absent pointer. */
#include <stddef.h>

/* Allow 768 bytes, including the final zero byte, for each bounded plain/Telegram message. */
#define NOTIFICATION_TEXT_CAPACITY 768
/* Allow 6144 bytes, including the final zero byte, for the larger HTML email card. */
#define NOTIFICATION_HTML_CAPACITY 6144

/* Turn the captured UTC time into PhST text. This formats a copy and never resets the clock. */
esp_err_t notification_format_time(const notification_message_t *message, char *text, size_t size);
/* Build Telegram HTML when html is true, or a complete plain-text alternative otherwise. */
esp_err_t notification_format_text(const notification_message_t *message, bool html, char *text,
                                   size_t size);
/* Build the HTML email card. This prepares text only; gmail.c sends it later. */
esp_err_t notification_format_gmail(const notification_message_t *message, char *html, size_t size);

/* The caller owns a successful JSON result and releases it using cJSON_Delete(). */
esp_err_t notification_build_slack(const notification_message_t *message, cJSON **json);
/* Build a colored Discord card and disable automatic mention parsing. */
esp_err_t notification_build_discord(const notification_message_t *message, cJSON **json);
/* Package Telegram HTML, the destination chat ID, and the HTML parse-mode setting. */
esp_err_t notification_build_telegram(const notification_message_t *message, const char *chat_id,
                                      cJSON **json);

/* Small inline email image. It is stored as Base64, not fetched from a website. */
extern const char notification_mark_base64[];
