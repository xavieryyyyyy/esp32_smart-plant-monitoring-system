/*
 * This file sends the notification task's message to the configured Telegram chat.
 * The bot token is part of the request address, so never print that address.
 * Success requires HTTP 200 and Telegram's JSON reply saying ok=true.
 * This function makes one send attempt per call.
 */
#include "notifications.h"
#include "notification_format.h"
#include "config.local.h"
#include "connectivity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

/*
 * Keep the reply text, how much we received, and whether it was too big.
 * There is room for 4095 text bytes plus the final string-ending character.
 */
typedef struct {
    char body[4096];
    size_t used;
    bool overflow;
} telegram_response_t;

/*
 * Telegram's reply may arrive in several pieces. Add each piece to the saved reply.
 * Leave one byte for '\0', the character that marks the end of a C string.
 */
static esp_err_t telegram_http_event(esp_http_client_event_t *event)
{
    if (event->event_id != HTTP_EVENT_ON_DATA || event->user_data == NULL || event->data_len <= 0) {
        return ESP_OK;
    }

    telegram_response_t *response = event->user_data;
    size_t length = (size_t)event->data_len;

    /*
     * If this piece will not fit, mark the reply as too big instead of writing past the array.
     */
    if (length > sizeof(response->body) - 1 - response->used) {
        response->overflow = true;
        return ESP_OK;
    }

    memcpy(response->body + response->used, event->data, length);

    response->used += length;
    response->body[response->used] = '\0';

    return ESP_OK;
}

esp_err_t notifications_send_telegram(const notification_message_t *message)
{
    if (message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!connectivity_is_ready() || !connectivity_time_is_ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (strchr(TELEGRAM_BOT_TOKEN, ':') == NULL || TELEGRAM_CHAT_ID[0] == '\0' ||
        strstr(TELEGRAM_CHAT_ID, "REPLACE_ME") != NULL) {
        ESP_LOGE("TELEGRAM", "Configure the local token and chat ID");
        return ESP_ERR_INVALID_ARG;
    }

    char url[256];
    int length = snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMessage",
                          TELEGRAM_BOT_TOKEN);

    if (length < 0 || length >= (int)sizeof(url)) {
        return ESP_ERR_INVALID_SIZE;
    }

    /* Format the captured event with bold labels and Telegram HTML parsing. */
    cJSON *json = NULL;
    esp_err_t format_result = notification_build_telegram(message, TELEGRAM_CHAT_ID, &json);
    if (format_result != ESP_OK) {
        return format_result;
    }

    char *body = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    if (body == NULL) {
        return ESP_ERR_NO_MEM;
    }

    /*
     * Reserve memory for this reply outside the task's stack to leave stack space for calls.
     * Release that memory at the end of this request.
     */
    telegram_response_t *response = calloc(1, sizeof(*response));
    if (response == NULL) {
        cJSON_free(body);
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
        .disable_auto_redirect = true,
        .event_handler = telegram_http_event,
        .user_data = response,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        free(response);
        cJSON_free(body);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result = esp_http_client_set_method(client, HTTP_METHOD_POST);

    if (result == ESP_OK) {
        result = esp_http_client_set_header(client, "Content-Type", "application/json");
    }

    if (result == ESP_OK) {
        result = esp_http_client_set_post_field(client, body, strlen(body));
    }

    if (result == ESP_OK) {
        result = esp_http_client_perform(client);

        if (result == ESP_OK) {
            int status = esp_http_client_get_status_code(client);
            ESP_LOGI("TELEGRAM", "HTTP status: %d", status);

            if (response->overflow) {
                result = ESP_ERR_INVALID_SIZE;
            } else if (status != 200) {
                result = ESP_FAIL;
            } else {
                cJSON *reply = cJSON_Parse(response->body);

                if (reply == NULL) {
                    result = ESP_FAIL;
                } else {
                    const cJSON *ok = cJSON_GetObjectItemCaseSensitive(reply, "ok");

                    /*
                     * Check Telegram's own ok field too. HTTP 200 alone does not finish this check.
                     */
                    if (!cJSON_IsTrue(ok)) {
                        result = ESP_FAIL;
                    }

                    cJSON_Delete(reply);
                }
            }
        }
    }

    /* Close the request, then free both the received reply and the outgoing JSON text. */
    esp_http_client_cleanup(client);
    free(response);
    cJSON_free(body);

    return result;
}