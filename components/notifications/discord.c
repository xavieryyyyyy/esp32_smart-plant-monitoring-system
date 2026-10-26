/*
 * This file sends the notification task's message to a Discord webhook over HTTPS.
 * A webhook is the private address where Discord accepts our message.
 * Check Wi-Fi and the clock first, then make one send attempt per function call.
 */
#include "notifications.h"
#include "notification_format.h"
#include "config.local.h"
#include "connectivity.h"

#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

esp_err_t notifications_send_discord(const notification_message_t *message)
{
    if (message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!connectivity_is_ready() || !connectivity_time_is_ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    const char *url = DISCORD_WEBHOOK_URL;
    const char *prefix = "https://discord.com/api/webhooks/";

    if (strncmp(url, prefix, strlen(prefix)) != 0 || strstr(url, "REPLACE_ME") != NULL ||
        strchr(url, '?') != NULL || strchr(url, '#') != NULL) {
        ESP_LOGE("DISCORD", "Configure a plain Discord webhook URL");
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * Add wait=true so Discord returns a response to our send.
     * The check above rejects URLs that already have ? or # parts.
     */
    char request_url[512];
    int length = snprintf(request_url, sizeof(request_url), "%s?wait=true", url);

    if (length < 0 || length >= (int)sizeof(request_url)) {
        return ESP_ERR_INVALID_SIZE;
    }

    /* Build the native Discord embed. The shared formatter also disables mentions. */
    cJSON *json = NULL;
    esp_err_t format_result = notification_build_discord(message, &json);
    if (format_result != ESP_OK) {
        return format_result;
    }

    char *body = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    if (body == NULL) {
        return ESP_ERR_NO_MEM;
    }

    /* Check Discord's certificate and stop this request if it takes too long. */
    esp_http_client_config_t config = {
        .url = request_url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
        .disable_auto_redirect = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (client == NULL) {
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
            ESP_LOGI("DISCORD", "HTTP status: %d", status);

            if (status != 200) {
                result = ESP_FAIL;
            }
        }
    }

    /*
     * Close the connection and free the temporary JSON text before returning the result.
     */
    esp_http_client_cleanup(client);
    cJSON_free(body);

    return result;
}