#include "notifications.h"
#include "config.local.h"
#include "connectivity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

typedef struct {
    char body[4096];
    size_t used;
    bool overflow;
} telegram_response_t;

static esp_err_t telegram_http_event(
    esp_http_client_event_t *event)
{
    if (event->event_id != HTTP_EVENT_ON_DATA ||
        event->user_data == NULL ||
        event->data_len <= 0) {
        return ESP_OK;
    }

    telegram_response_t *response = event->user_data;
    size_t length = (size_t)event->data_len;

    if (length > sizeof(response->body) - 1 - response->used) {
        response->overflow = true;
        return ESP_OK;
    }

    memcpy(response->body + response->used,
           event->data, length);

    response->used += length;
    response->body[response->used] = '\0';

    return ESP_OK;
}

esp_err_t notifications_send_telegram(const char *message)
{
    if (message == NULL || message[0] == '\0' ||
        strlen(message) > 4096) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!connectivity_is_ready() ||
        !connectivity_time_is_ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (strchr(TELEGRAM_BOT_TOKEN, ':') == NULL ||
        TELEGRAM_CHAT_ID[0] == '\0' ||
        strstr(TELEGRAM_CHAT_ID, "REPLACE_ME") != NULL) {
        ESP_LOGE("TELEGRAM", "Configure the local token and chat ID");
        return ESP_ERR_INVALID_ARG;
    }

    char url[256];
    int length = snprintf(
        url, sizeof(url),
        "https://api.telegram.org/bot%s/sendMessage",
        TELEGRAM_BOT_TOKEN);

    if (length < 0 || length >= (int)sizeof(url)) {
        return ESP_ERR_INVALID_SIZE;
    }

    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (cJSON_AddStringToObject(
            json, "chat_id", TELEGRAM_CHAT_ID) == NULL ||
        cJSON_AddStringToObject(json, "text", message) == NULL) {
        cJSON_Delete(json);
        return ESP_ERR_NO_MEM;
    }

    char *body = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    if (body == NULL) {
        return ESP_ERR_NO_MEM;
    }

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

    esp_err_t result = esp_http_client_set_method(
        client, HTTP_METHOD_POST);

    if (result == ESP_OK) {
        result = esp_http_client_set_header(
            client, "Content-Type", "application/json");
    }

    if (result == ESP_OK) {
        result = esp_http_client_set_post_field(
            client, body, strlen(body));
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
                    const cJSON *ok =
                        cJSON_GetObjectItemCaseSensitive(reply, "ok");

                    if (!cJSON_IsTrue(ok)) {
                        result = ESP_FAIL;
                    }

                    cJSON_Delete(reply);
                }
            }
        }
    }

    esp_http_client_cleanup(client);
    free(response);
    cJSON_free(body);

    return result;
}