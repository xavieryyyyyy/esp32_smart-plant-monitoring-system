#include "notifications.h"
#include "config.local.h"
#include "connectivity.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

typedef struct {
    int retry_after_seconds;
} slack_response_t;

static esp_err_t slack_http_event(esp_http_client_event_t *event)
{
    if (event->event_id != HTTP_EVENT_ON_HEADER ||
        event->user_data == NULL ||
        event->header_key == NULL ||
        event->header_value == NULL) {
        return ESP_OK;
    }

    if (strcasecmp(event->header_key, "Retry-After") == 0) {
        slack_response_t *response = event->user_data;
        char *end;

        errno = 0;
        long seconds = strtol(event->header_value, &end, 10);

        if (errno == 0 &&
            end != event->header_value &&
            *end == '\0' &&
            seconds >= 0 &&
            seconds <= INT_MAX) {
            response->retry_after_seconds = (int)seconds;
        }
    }

    return ESP_OK;
}

static esp_err_t slack_send_once(
    const char *message, int *http_status, int *retry_after)
{
    *http_status = 0;
    *retry_after = -1;


    if (message == NULL || message[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    if (!connectivity_is_ready() ||
        !connectivity_time_is_ready()) {
        ESP_LOGW("SLACK", "Wi-Fi or synchronized time unavailable");
        return ESP_ERR_INVALID_STATE;
    }

    const char *url = SLACK_WEBHOOK_URL;
    const char *prefix = "https://hooks.slack.com/services/";

    if (strncmp(url, prefix, strlen(prefix)) != 0 ||
        strstr(url, "REPLACE_ME") != NULL ||
        strstr(url, "PASTE_") != NULL) {
        ESP_LOGE("SLACK", "Configure the local webhook URL first");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (cJSON_AddStringToObject(json, "text", message) == NULL) {
        cJSON_Delete(json);
        return ESP_ERR_NO_MEM;
    }

    char *body = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    if (body == NULL) {
        return ESP_ERR_NO_MEM;
    }

    slack_response_t response = {
    .retry_after_seconds = -1,
};

    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
        .disable_auto_redirect = true,
        .event_handler = slack_http_event,
        .user_data = &response,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
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
            *http_status = status;
            ESP_LOGI("SLACK", "HTTP status: %d", status);

            if (status != 200) {
                result = ESP_FAIL;
            }
        }
    }

    *retry_after = response.retry_after_seconds;
    esp_http_client_cleanup(client);
    cJSON_free(body);

    return result;
}

esp_err_t notifications_send_slack(const char *message)
{
    /* Used only by the single notification task. */
    static int64_t next_request_us = 0;
    static bool rate_limit_unknown = false;

    if (rate_limit_unknown) {
        ESP_LOGE("SLACK",
                 "Sending paused until restart: invalid Retry-After");
        return ESP_ERR_INVALID_STATE;
    }

    for (int attempt = 1; attempt <= 3; attempt++) {
        int64_t remaining = next_request_us - esp_timer_get_time();

        if (remaining > 30000000LL) {
            ESP_LOGW("SLACK",
                     "Server wait exceeds 30-second task budget");
            return ESP_ERR_TIMEOUT;
        }

        while (remaining > 0) {
            uint32_t wait_ms = (uint32_t)((remaining + 999) / 1000);
            vTaskDelay(pdMS_TO_TICKS(wait_ms) + 1);
            remaining = next_request_us - esp_timer_get_time();
        }

        int status;
        int retry_after;

        ESP_LOGI("SLACK", "Attempt %d/3", attempt);

        esp_err_t result =
            slack_send_once(message, &status, &retry_after);

        int64_t now = esp_timer_get_time();
        next_request_us = now + 1000000LL;

        if (result == ESP_OK) {
            return ESP_OK;
        }

        if (status == 429) {
            if (retry_after < 0) {
                rate_limit_unknown = true;
                ESP_LOGE("SLACK",
                         "Missing/invalid Retry-After; sending paused");
                return result;
            }

            int seconds = retry_after > 0 ? retry_after : 1;
            next_request_us = now + (int64_t)seconds * 1000000LL;

            ESP_LOGW("SLACK", "Rate limited; wait at least %d seconds",
                     seconds);
        } else {
            bool temporary =
                status == 408 ||
                (status >= 500 && status <= 599) ||
                (status == 0 &&
                 result != ESP_ERR_INVALID_ARG &&
                 result != ESP_ERR_INVALID_STATE &&
                 result != ESP_ERR_NO_MEM);

            if (!temporary) {
                ESP_LOGW("SLACK", "Not retrying this failure");
                return result;
            }

            next_request_us =
                now + (int64_t)(attempt * 2) * 1000000LL;

            ESP_LOGW("SLACK", "Temporary failure: %s",
                     esp_err_to_name(result));
        }

        if (attempt == 3) {
            ESP_LOGW("SLACK", "Attempt limit reached");
            return result;
        }
    }

    return ESP_FAIL;
}