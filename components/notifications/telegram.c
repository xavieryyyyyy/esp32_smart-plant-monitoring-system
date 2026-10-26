/*
 * This file sends the notification task's message to the configured Telegram chat.
 * The bot token is part of the request address, so never print that address.
 * Success requires HTTP 200 and Telegram's JSON reply saying ok=true.
 * This function makes one send attempt per call.
 */
#include "notifications.h"
/* Use the shared text, PhST, and JSON builders instead of duplicating layouts. */
#include "notification_format.h"
/* Read private settings from the ignored local file; never print its secret values. */
#include "config.local.h"
/* Use the networking component to start Wi-Fi or ask whether Wi-Fi/time is ready. */
#include "connectivity.h"

/* Format bounded text with snprintf(), or use file/console I/O in host tests. */
#include <stdio.h>
/* Allocate/free temporary memory, and provide standard utility functions. */
#include <stdlib.h>
/* Measure, compare, copy, or search text and byte arrays. */
#include <string.h>
/* Build/read JSON trees; JSON is the named-field text format the chat APIs use. */
#include "cJSON.h"
/* Use the trusted root certificates when checking a server certificate. */
#include "esp_crt_bundle.h"
/* Use ESP-IDF to prepare and perform an HTTP/HTTPS request. */
#include "esp_http_client.h"
/* Print tagged information, warnings, and errors in the serial monitor. */
#include "esp_log.h"

/*
 * Keep the reply text, how much we received, and whether it was too big.
 * There is room for 4095 text bytes plus the final string-ending character.
 */
typedef struct {
    /* Collect Telegram response bytes here; reserve the final byte for the C string terminator. */
    char body[4096];
    /* Track how many bytes have been stored; leave space for the final string-ending byte. */
    size_t used;
    /*
     * Remember that a reply chunk exceeded capacity; the sender must reject this incomplete
     * response.
     */
    bool overflow;
} telegram_response_t;

/*
 * Telegram's reply may arrive in several pieces. Add each piece to the saved reply.
 * Leave one byte for '\0', the character that marks the end of a C string.
 */
static esp_err_t telegram_http_event(esp_http_client_event_t *event)
{
    /* Check for an absent pointer or failed allocation before dereferencing/using it. */
    if (event->event_id != HTTP_EVENT_ON_DATA || event->user_data == NULL || event->data_len <= 0) {
        /* Finish successfully; the caller may now use this function's output. */
        return ESP_OK;
    }

    /* Keep the reply data that the HTTP event callback fills during this request. */
    telegram_response_t *response = event->user_data;
    /*
     * Convert this received data-chunk length to the unsigned byte-count type after checking it
     * is positive.
     */
    size_t length = (size_t)event->data_len;

    /*
     * If this piece will not fit, mark the reply as too big instead of writing past the array.
     */
    if (length > sizeof(response->body) - 1 - response->used) {
        /*
         * Remember that the reply exceeded capacity; the sender will reject it after the
         * request.
         */
        response->overflow = true;
        /* Finish successfully; the caller may now use this function's output. */
        return ESP_OK;
    }

    /* Copy exactly this many bytes; capacity is checked before reaching this copy. */
    memcpy(response->body + response->used, event->data, length);

    /* Advance the saved reply length by the number of bytes just copied. */
    response->used += length;
    /* Place the final zero byte so the collected HTTP reply is a valid C string. */
    response->body[response->used] = '\0';

    /* Finish successfully; the caller may now use this function's output. */
    return ESP_OK;
}

/* Send one Telegram message and require both HTTP 200 and the reply field ok=true. */
esp_err_t notifications_send_telegram(const notification_message_t *message)
{
    /* Check for an absent pointer or failed allocation before dereferencing/using it. */
    if (message == NULL) {
        /* Reject an absent, unsafe, or out-of-range input before using it. */
        return ESP_ERR_INVALID_ARG;
    }

    /* Require both ready Wi-Fi and a synchronized clock before attempting this secure send. */
    if (!connectivity_is_ready() || !connectivity_time_is_ready()) {
        /* Stop because required setup/readiness is missing or sending is deliberately paused. */
        return ESP_ERR_INVALID_STATE;
    }

    /* Check for an absent pointer or failed allocation before dereferencing/using it. */
    if (strchr(TELEGRAM_BOT_TOKEN, ':') == NULL || TELEGRAM_CHAT_ID[0] == '\0' ||
        strstr(TELEGRAM_CHAT_ID, "REPLACE_ME") != NULL) {
        /* Print an error result so the failed step is visible in the serial monitor. */
        ESP_LOGE("TELEGRAM", "Configure the local token and chat ID");
        /* Reject an absent, unsafe, or out-of-range input before using it. */
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * Reserve bounded space for Telegram's API address; it contains the bot token and must not
     * be logged.
     */
    char url[256];
    /* Format text into a fixed-size array; check the returned required length before using it. */
    int length = snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMessage",
                          TELEGRAM_BOT_TOKEN);

    /* Reject a formatting error or text that would not fit, including its string-ending byte. */
    if (length < 0 || length >= (int)sizeof(url)) {
        /* Report that the available buffer/format limit could not safely hold this data. */
        return ESP_ERR_INVALID_SIZE;
    }

    /* Format the captured event with bold labels and Telegram HTML parsing. */
    cJSON *json = NULL;
    /* Build Telegram chat_id/text/parse_mode fields from this captured event. */
    esp_err_t format_result = notification_build_telegram(message, TELEGRAM_CHAT_ID, &json);
    /* Handle/return the earlier failure instead of continuing with invalid output. */
    if (format_result != ESP_OK) {
        /*
         * Pass the formatting failure to the caller; no network request should use an incomplete
         * payload.
         */
        return format_result;
    }

    /* Turn the JSON tree into compact request text with correctly escaped string values. */
    char *body = cJSON_PrintUnformatted(json);
    /*
     * Free this JSON tree and all children already attached to it; this does not free serialized
     * text.
     */
    cJSON_Delete(json);

    /* Check for an absent pointer or failed allocation before dereferencing/using it. */
    if (body == NULL) {
        /*
         * Report that temporary storage could not be allocated; the caller must handle the
         * failure.
         */
        return ESP_ERR_NO_MEM;
    }

    /*
     * Reserve memory for this reply outside the task's stack to leave stack space for calls.
     * Release that memory at the end of this request.
     */
    telegram_response_t *response = calloc(1, sizeof(*response));
    /* Check for an absent pointer or failed allocation before dereferencing/using it. */
    if (response == NULL) {
        /*
         * Free the separately allocated JSON request text after the client has finished using
         * it.
         */
        cJSON_free(body);
        /*
         * Report that temporary storage could not be allocated; the caller must handle the
         * failure.
         */
        return ESP_ERR_NO_MEM;
    }

    /* Group the settings passed to this ESP-IDF driver, service, or protected connection. */
    esp_http_client_config_t config = {
        /*
         * Tell the HTTP client which service address to contact; keep token/webhook addresses
         * private.
         */
        .url = url,
        /* Check the server certificate against the trusted root certificate bundle. */
        .crt_bundle_attach = esp_crt_bundle_attach,
        /* Set this connection/request timeout to 10,000 milliseconds (ten seconds). */
        .timeout_ms = 10000,
        /* Do not automatically follow redirects to another address with this request. */
        .disable_auto_redirect = true,
        /* Collect the relevant reply header/body pieces using this callback during the request. */
        .event_handler = telegram_http_event,
        /* Give the callback this request's response storage so it can save received data. */
        .user_data = response,
    };

    /* Create an HTTP client using these connection settings; NULL means allocation/setup failed. */
    esp_http_client_handle_t client = esp_http_client_init(&config);
    /* Check for an absent pointer or failed allocation before dereferencing/using it. */
    if (client == NULL) {
        /*
         * Release this temporary heap allocation; free(NULL) is safe for partial-allocation
         * cleanup.
         */
        free(response);
        /*
         * Free the separately allocated JSON request text after the client has finished using
         * it.
         */
        cJSON_free(body);
        /*
         * Report that temporary storage could not be allocated; the caller must handle the
         * failure.
         */
        return ESP_ERR_NO_MEM;
    }

    /* Choose HTTP POST because this request submits a message to the service. */
    esp_err_t result = esp_http_client_set_method(client, HTTP_METHOD_POST);

    /* Continue to the next step only if the preceding operation succeeded. */
    if (result == ESP_OK) {
        /* Tell the server that the outgoing body is JSON text. */
        result = esp_http_client_set_header(client, "Content-Type", "application/json");
    }

    /* Continue to the next step only if the preceding operation succeeded. */
    if (result == ESP_OK) {
        /*
         * Give the HTTP client the outgoing text and its byte count; keep it alive through
         * perform().
         */
        result = esp_http_client_set_post_field(client, body, strlen(body));
    }

    /* Continue to the next step only if the preceding operation succeeded. */
    if (result == ESP_OK) {
        /*
         * Carry out the HTTPS request; the callback may collect headers/reply pieces during this
         * call.
         */
        result = esp_http_client_perform(client);

        /* Continue to the next step only if the preceding operation succeeded. */
        if (result == ESP_OK) {
            /*
             * Read/store the HTTP status number; a completed connection alone does not prove
             * acceptance.
             */
            int status = esp_http_client_get_status_code(client);
            /* Print an informational progress/result line with a tag for the serial monitor. */
            ESP_LOGI("TELEGRAM", "HTTP status: %d", status);

            /* Reject an oversized Telegram reply rather than parsing incomplete text. */
            if (response->overflow) {
                /* Mark this content/command too large rather than using a truncated buffer. */
                result = ESP_ERR_INVALID_SIZE;
                /* Treat any other HTTP response as rejection/failure for this service. */
            } else if (status != 200) {
                /*
                 * Keep the operation marked failed so the caller will not log service
                 * acceptance.
                 */
                result = ESP_FAIL;
            } else {
                /*
                 * Parse Telegram's received JSON text so its application-level ok field can be
                 * checked.
                 */
                cJSON *reply = cJSON_Parse(response->body);

                /*
                 * Check for an absent pointer or failed allocation before dereferencing/using
                 * it.
                 */
                if (reply == NULL) {
                    /*
                     * Keep the operation marked failed so the caller will not log service
                     * acceptance.
                     */
                    result = ESP_FAIL;
                } else {
                    /*
                     * Look up Telegram's exact lowercase ok field; it must be JSON true, not
                     * just present.
                     */
                    const cJSON *ok = cJSON_GetObjectItemCaseSensitive(reply, "ok");

                    /*
                     * Check Telegram's own ok field too. HTTP 200 alone does not finish this check.
                     */
                    if (!cJSON_IsTrue(ok)) {
                        /*
                         * Keep the operation marked failed so the caller will not log service
                         * acceptance.
                         */
                        result = ESP_FAIL;
                    }

                    /*
                     * Free this JSON tree and all children already attached to it; this does not
                     * free serialized text.
                     */
                    cJSON_Delete(reply);
                }
            }
        }
    }

    /* Close the request, then free both the received reply and the outgoing JSON text. */
    esp_http_client_cleanup(client);
    /* Release this temporary heap allocation; free(NULL) is safe for partial-allocation cleanup. */
    free(response);
    /* Free the separately allocated JSON request text after the client has finished using it. */
    cJSON_free(body);

    /* Pass the actual success/error code back to the caller; do not replace it with success. */
    return result;
}