/*
 * This file sends the notification task's message to Slack over HTTPS.
 * It builds JSON, a text format with named fields such as "text".
 * TLS checks Slack's certificate and protects the network connection.
 * For selected temporary failures, it waits and tries again, up to three attempts.
 * The webhook URL is a secret. Keep it out of logs and public firmware files.
 */
#include "notifications.h"
/* Use the shared text, PhST, and JSON builders instead of duplicating layouts. */
#include "notification_format.h"
/* Read private settings from the ignored local file; never print its secret values. */
#include "config.local.h"
/* Use the networking component to start Wi-Fi or ask whether Wi-Fi/time is ready. */
#include "connectivity.h"
/* Check whether converting a received number failed or overflowed. */
#include <errno.h>
/* Use integer limits to reject arithmetic that would overflow. */
#include <limits.h>
/* Allocate/free temporary memory, and provide standard utility functions. */
#include <stdlib.h>
/* Compare header names without treating upper/lower case as different. */
#include <strings.h>
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
/* Use FreeRTOS types and constants for tasks, queues, and waiting. */
#include "freertos/FreeRTOS.h"
/* Create background jobs, pause them, or wake them with task notifications. */
#include "freertos/task.h"
/* Read the microsecond timer since startup; use it for ages and waiting deadlines. */
#include "esp_timer.h"

/* Group related fields into one value so callers/queues keep the correct pieces together. */
typedef struct {
    /* Save a validated Slack Retry-After number of seconds; -1 means no valid hint. */
    int retry_after_seconds;
} slack_response_t;

/*
 * Slack may send Retry-After: a number of seconds to wait before another request.
 * Save it only if it is a valid whole number. -1 means we do not know the wait.
 */
static esp_err_t slack_http_event(esp_http_client_event_t *event)
{
    /* Check for an absent pointer or failed allocation before dereferencing/using it. */
    if (event->event_id != HTTP_EVENT_ON_HEADER || event->user_data == NULL ||
        event->header_key == NULL || event->header_value == NULL) {
        /* Finish successfully; the caller may now use this function's output. */
        return ESP_OK;
    }

    /*
     * Recognize this header regardless of capitalization; unrelated response headers need no
     * parsing.
     */
    if (strcasecmp(event->header_key, "Retry-After") == 0) {
        /* Keep the reply data that the HTTP event callback fills during this request. */
        slack_response_t *response = event->user_data;
        /* Receive where number parsing stopped so trailing junk can be rejected. */
        char *end;

        /* Clear the prior conversion error before parsing the received Retry-After header. */
        errno = 0;
        /*
         * Parse decimal Retry-After seconds; end and errno let the next check detect junk or
         * overflow.
         */
        long seconds = strtol(event->header_value, &end, 10);

        /* Accept only a complete, nonnegative decimal Retry-After value that fits an int. */
        if (errno == 0 && end != event->header_value && *end == '\0' && seconds >= 0 &&
            seconds <= INT_MAX) {
            /* Remember the validated wait hint so Slack's retry wrapper can honor it. */
            response->retry_after_seconds = (int)seconds;
        }
    }

    /* Finish successfully; the caller may now use this function's output. */
    return ESP_OK;
}

/*
 * Try one Slack request. Return its HTTP status and any server-supplied wait time.
 * notifications_send_slack() below uses these values to decide whether to try again.
 */
static esp_err_t slack_send_once(const notification_message_t *message, int *http_status,
                                 int *retry_after)
{
    /*
     * Return the observed HTTP status to the Slack retry wrapper; zero means no response status
     * yet.
     */
    *http_status = 0;
    /* Return the parsed server wait, or -1 when no valid wait was received. */
    *retry_after = -1;

    /* Check for an absent pointer or failed allocation before dereferencing/using it. */
    if (message == NULL) {
        /* Reject an absent, unsafe, or out-of-range input before using it. */
        return ESP_ERR_INVALID_ARG;
    }

    /* Skip this send if Wi-Fi or the clock is not ready for a secure request. */
    if (!connectivity_is_ready() || !connectivity_time_is_ready()) {
        /* Print a warning about a recoverable problem or skipped operation. */
        ESP_LOGW("SLACK", "Wi-Fi or synchronized time unavailable");
        /* Stop because required setup/readiness is missing or sending is deliberately paused. */
        return ESP_ERR_INVALID_STATE;
    }

    /* Use the private service address; never print a webhook/token-bearing URL. */
    const char *url = SLACK_WEBHOOK_URL;
    /* Define the allowed service-address prefix so placeholders or wrong endpoints are rejected. */
    const char *prefix = "https://hooks.slack.com/services/";

    /* Check whether this pointer/search result exists before relying on it. */
    if (strncmp(url, prefix, strlen(prefix)) != 0 || strstr(url, "REPLACE_ME") != NULL ||
        strstr(url, "PASTE_") != NULL) {
        /* Print an error result so the failed step is visible in the serial monitor. */
        ESP_LOGE("SLACK", "Configure the local webhook URL first");
        /* Reject an absent, unsafe, or out-of-range input before using it. */
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * Build Slack's full-width sections and a complete plain-text fallback for notifications.
     */
    cJSON *json = NULL;
    /* Build Slack fields/fallback from this captured event; check its result before sending. */
    esp_err_t format_result = notification_build_slack(message, &json);
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

    /* Keep the reply data that the HTTP event callback fills during this request. */
    slack_response_t response = {
        /* Begin with no valid wait hint; the response callback may replace -1 later. */
        .retry_after_seconds = -1,
    };

    /*
     * Use trusted certificates, a ten-second request timeout, and no automatic redirects.
     */
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
        .event_handler = slack_http_event,
        /* Give the callback this request's response storage so it can save received data. */
        .user_data = &response,
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
            /*
             * Return the observed HTTP status to the Slack retry wrapper; zero means no response
             * status yet.
             */
            *http_status = status;
            /* Print an informational progress/result line with a tag for the serial monitor. */
            ESP_LOGI("SLACK", "HTTP status: %d", status);

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
     * Save the server wait value, close the connection, and release temporary message memory.
     */
    *retry_after = response.retry_after_seconds;
    /* Close/free the HTTP client after the request, including when a later step failed. */
    esp_http_client_cleanup(client);
    /* Free the separately allocated JSON request text after the client has finished using it. */
    cJSON_free(body);

    /* Pass the actual success/error code back to the caller; do not replace it with success. */
    return result;
}

/*
 * The notification task calls this function for each message.
 * It remembers Slack's wait rules between calls, but does not save messages across restarts.
 */
esp_err_t notifications_send_slack(const notification_message_t *message)
{
    /*
     * static keeps these values between calls. Only the one notification task uses them.
     */
    static int64_t next_request_us = 0;
    /* Remember that Slack requested a wait we could not parse; pause until restart if set. */
    static bool rate_limit_unknown = false;

    /* Do not send again after an unparseable rate-limit response until the device restarts. */
    if (rate_limit_unknown) {
        /* Print an error result so the failed step is visible in the serial monitor. */
        ESP_LOGE("SLACK", "Sending paused until restart: invalid Retry-After");
        /* Stop because required setup/readiness is missing or sending is deliberately paused. */
        return ESP_ERR_INVALID_STATE;
    }

    /*
     * Try at most three times. Return earlier if it works or the error should not be retried.
     */
    for (int attempt = 1; attempt <= 3; attempt++) {
        /* Measure how much of Slack's saved wait is still left before a request may start. */
        int64_t remaining = next_request_us - esp_timer_get_time();

        /*
         * Do not spend more than thirty seconds on this wait. A later call still sees the deadline.
         * This limit applies to this wait, not to the combined time for all services.
         */
        if (remaining > 30000000LL) {
            /* Print a warning about a recoverable problem or skipped operation. */
            ESP_LOGW("SLACK", "Server wait exceeds 30-second task budget");
            /* Report that this operation exceeded its allowed wait. */
            return ESP_ERR_TIMEOUT;
        }

        /* Keep waiting until Slack's saved earliest-request time has passed. */
        while (remaining > 0) {
            /* Round remaining microseconds upward to milliseconds so we do not retry too early. */
            uint32_t wait_ms = (uint32_t)((remaining + 999) / 1000);
            /*
             * Pause this task and let other tasks run; pdMS_TO_TICKS converts milliseconds to
             * scheduler ticks.
             */
            vTaskDelay(pdMS_TO_TICKS(wait_ms) + 1);
            /*
             * Recheck the actual remaining wait after sleeping because scheduler ticks may round
             * the delay.
             */
            remaining = next_request_us - esp_timer_get_time();
        }

        /*
         * Read/store the HTTP status number; a completed connection alone does not prove
         * acceptance.
         */
        int status;
        /* Receive Slack's wait hint; -1 means the header was absent or not a valid number. */
        int retry_after;

        /* Print an informational progress/result line with a tag for the serial monitor. */
        ESP_LOGI("SLACK", "Attempt %d/3", attempt);

        /* Keep this operation's success/error code so later steps can stop safely on failure. */
        esp_err_t result = slack_send_once(message, &status, &retry_after);

        /*
         * Read the current time for this operation; displayed measurements still use their
         * captured time.
         */
        int64_t now = esp_timer_get_time();
        /* Save the earliest allowed next Slack request using the monotonic uptime clock. */
        next_request_us = now + 1000000LL;

        /* Continue to the next step only if the preceding operation succeeded. */
        if (result == ESP_OK) {
            /* Finish successfully; the caller may now use this function's output. */
            return ESP_OK;
        }

        /*
         * 429 means Slack asked us to slow down. Use its Retry-After time.
         * If that time is missing/invalid, pause Slack sending until the ESP32 restarts.
         */
        if (status == 429) {
            /* A missing/invalid server wait cannot safely be guessed; pause Slack until restart. */
            if (retry_after < 0) {
                /*
                 * Remember the unsafe-to-guess server wait so future calls stay paused until
                 * restart.
                 */
                rate_limit_unknown = true;
                /* Print an error result so the failed step is visible in the serial monitor. */
                ESP_LOGE("SLACK", "Missing/invalid Retry-After; sending paused");
                /*
                 * Pass the actual success/error code back to the caller; do not replace it with
                 * success.
                 */
                return result;
            }

            /* Copy captured Unix seconds into a wide integer before adding the PhST offset. */
            int seconds = retry_after > 0 ? retry_after : 1;
            /* Save the earliest allowed next Slack request using the monotonic uptime clock. */
            next_request_us = now + (int64_t)seconds * 1000000LL;

            /* Print a warning about a recoverable problem or skipped operation. */
            ESP_LOGW("SLACK", "Rate limited; wait at least %d seconds", seconds);
        } else {
            /*
             * Retry selected timeouts, server errors, and connection failures. Stop on other errors.
             */
            bool temporary = status == 408 || (status >= 500 && status <= 599) ||
                             /*
                              * When no HTTP status arrived, exclude
                              * configuration/readiness/memory errors from connection retries.
                              */
                             (status == 0 && result != ESP_ERR_INVALID_ARG &&
                              result != ESP_ERR_INVALID_STATE && result != ESP_ERR_NO_MEM);

            /* Stop retrying errors this wrapper does not classify as temporary. */
            if (!temporary) {
                /* Print a warning about a recoverable problem or skipped operation. */
                ESP_LOGW("SLACK", "Not retrying this failure");
                /*
                 * Pass the actual success/error code back to the caller; do not replace it with
                 * success.
                 */
                return result;
            }

            /* Save the earliest allowed next Slack request using the monotonic uptime clock. */
            next_request_us = now + (int64_t)(attempt * 2) * 1000000LL;

            /* Print a warning about a recoverable problem or skipped operation. */
            ESP_LOGW("SLACK", "Temporary failure: %s", esp_err_to_name(result));
        }

        /* The third attempt used the retry budget; return the failure instead of looping again. */
        if (attempt == 3) {
            /* Print a warning about a recoverable problem or skipped operation. */
            ESP_LOGW("SLACK", "Attempt limit reached");
            /*
             * Pass the actual success/error code back to the caller; do not replace it with
             * success.
             */
            return result;
        }
    }

    /* Report a failed operation; do not treat this path as successful delivery. */
    return ESP_FAIL;
}