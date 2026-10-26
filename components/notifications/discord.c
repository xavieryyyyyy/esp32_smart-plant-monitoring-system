/*
 * This file sends the notification task's message to a Discord webhook over HTTPS.
 * A webhook is the private address where Discord accepts our message.
 * Check Wi-Fi and the clock first, then make one send attempt per function call.
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

/* Send one native Discord embed through the private webhook and check HTTP acceptance. */
esp_err_t notifications_send_discord(const notification_message_t *message)
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

    /* Use the private service address; never print a webhook/token-bearing URL. */
    const char *url = DISCORD_WEBHOOK_URL;
    /* Define the allowed service-address prefix so placeholders or wrong endpoints are rejected. */
    const char *prefix = "https://discord.com/api/webhooks/";

    /* Check whether this pointer/search result exists before relying on it. */
    if (strncmp(url, prefix, strlen(prefix)) != 0 || strstr(url, "REPLACE_ME") != NULL ||
        strchr(url, '?') != NULL || strchr(url, '#') != NULL) {
        /* Print an error result so the failed step is visible in the serial monitor. */
        ESP_LOGE("DISCORD", "Configure a plain Discord webhook URL");
        /* Reject an absent, unsafe, or out-of-range input before using it. */
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * Add wait=true so Discord returns a response to our send.
     * The check above rejects URLs that already have ? or # parts.
     */
    char request_url[512];
    /* Format text into a fixed-size array; check the returned required length before using it. */
    int length = snprintf(request_url, sizeof(request_url), "%s?wait=true", url);

    /* Reject a formatting error or text that would not fit, including its string-ending byte. */
    if (length < 0 || length >= (int)sizeof(request_url)) {
        /* Report that the available buffer/format limit could not safely hold this data. */
        return ESP_ERR_INVALID_SIZE;
    }

    /* Build the native Discord embed. The shared formatter also disables mentions. */
    cJSON *json = NULL;
    /* Build the Discord embed from this captured event; the caller owns a successful tree. */
    esp_err_t format_result = notification_build_discord(message, &json);
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

    /* Check Discord's certificate and stop this request if it takes too long. */
    esp_http_client_config_t config = {
        /*
         * Tell the HTTP client which service address to contact; keep token/webhook addresses
         * private.
         */
        .url = request_url,
        /* Check the server certificate against the trusted root certificate bundle. */
        .crt_bundle_attach = esp_crt_bundle_attach,
        /* Set this connection/request timeout to 10,000 milliseconds (ten seconds). */
        .timeout_ms = 10000,
        /* Do not automatically follow redirects to another address with this request. */
        .disable_auto_redirect = true,
    };

    /* Create an HTTP client using these connection settings; NULL means allocation/setup failed. */
    esp_http_client_handle_t client = esp_http_client_init(&config);

    /* Check for an absent pointer or failed allocation before dereferencing/using it. */
    if (client == NULL) {
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
            ESP_LOGI("DISCORD", "HTTP status: %d", status);

            /* Treat any other HTTP response as rejection/failure for this service. */
            if (status != 200) {
                /*
                 * Keep the operation marked failed so the caller will not log service
                 * acceptance.
                 */
                result = ESP_FAIL;
            }
        }
    }

    /*
     * Close the connection and free the temporary JSON text before returning the result.
     */
    esp_http_client_cleanup(client);
    /* Free the separately allocated JSON request text after the client has finished using it. */
    cJSON_free(body);

    /* Pass the actual success/error code back to the caller; do not replace it with success. */
    return result;
}