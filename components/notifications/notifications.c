#include "notifications.h"
#include "config.local.h"
#include "connectivity.h"

#include <string.h>
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

esp_err_t notifications_send_slack(const char *message)
{
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

    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
        .disable_auto_redirect = true,
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
            ESP_LOGI("SLACK", "HTTP status: %d", status);

            if (status != 200) {
                result = ESP_FAIL;
            }
        }
    }

    esp_http_client_cleanup(client);
    cJSON_free(body);

    return result;
}