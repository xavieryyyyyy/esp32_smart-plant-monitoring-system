#include "notifications.h"
#include "config.local.h"
#include "connectivity.h"
#include "mbedtls/platform_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_tls.h"
#include "mbedtls/base64.h"
#include <time.h>

static esp_err_t smtp_write_all(
    esp_tls_t *tls, const char *data, size_t length)
{
    size_t sent = 0;
    int64_t deadline = esp_timer_get_time() + 10000000LL;

    while (sent < length) {
        if (esp_timer_get_time() >= deadline) {
            return ESP_ERR_TIMEOUT;
        }

        ssize_t result = esp_tls_conn_write(
            tls, data + sent, length - sent);

        if (result > 0) {
            sent += (size_t)result;
        } else if (result == ESP_TLS_ERR_SSL_WANT_READ ||
                   result == ESP_TLS_ERR_SSL_WANT_WRITE) {
            vTaskDelay(pdMS_TO_TICKS(10) + 1);
        } else {
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

static esp_err_t smtp_expect(esp_tls_t *tls, int expected_code)
{
    int64_t deadline = esp_timer_get_time() + 10000000LL;

    for (int lines = 0; lines < 64; lines++) {
        char line[513];
        size_t used = 0;

        while (1) {
            if (esp_timer_get_time() >= deadline) {
                return ESP_ERR_TIMEOUT;
            }

            char character;
            ssize_t result = esp_tls_conn_read(
                tls, &character, 1);

            if (result == ESP_TLS_ERR_SSL_WANT_READ ||
                result == ESP_TLS_ERR_SSL_WANT_WRITE) {
                vTaskDelay(pdMS_TO_TICKS(10) + 1);
                continue;
            }

            if (result <= 0) {
                return ESP_FAIL;
            }

            if (used >= sizeof(line) - 1) {
                return ESP_ERR_INVALID_SIZE;
            }

            line[used++] = character;

            if (character == '\n') {
                break;
            }
        }

        line[used] = '\0';

        if (used < 6 ||
            line[used - 2] != '\r' ||
            line[0] < '0' || line[0] > '9' ||
            line[1] < '0' || line[1] > '9' ||
            line[2] < '0' || line[2] > '9') {
            return ESP_FAIL;
        }

        int code = (line[0] - '0') * 100 +
                   (line[1] - '0') * 10 +
                   (line[2] - '0');

        if (code != expected_code) {
            ESP_LOGW("GMAIL", "SMTP reply %d; expected %d",
                     code, expected_code);
            return ESP_FAIL;
        }

        if (line[3] == ' ') {
            ESP_LOGI("GMAIL", "SMTP reply: %d", code);
            return ESP_OK;
        }

        if (line[3] != '-') {
            return ESP_FAIL;
        }
    }

    return ESP_ERR_INVALID_SIZE;
}

static esp_err_t smtp_command(
    esp_tls_t *tls, const char *command, int expected_code)
{
    esp_err_t result = smtp_write_all(
        tls, command, strlen(command));

    if (result != ESP_OK) {
        return result;
    }

    return smtp_expect(tls, expected_code);
}

static esp_err_t smtp_send_base64(
    esp_tls_t *tls, const char *value, int expected_code)
{
    unsigned char encoded[512];
    size_t encoded_length = 0;

    int encoding_result = mbedtls_base64_encode(
        encoded, sizeof(encoded) - 3, &encoded_length,
        (const unsigned char *)value, strlen(value));

    if (encoding_result != 0) {
        mbedtls_platform_zeroize(encoded, sizeof(encoded));
        return ESP_ERR_INVALID_SIZE;
    }

    encoded[encoded_length++] = '\r';
    encoded[encoded_length++] = '\n';

    esp_err_t result = smtp_write_all(
        tls, (const char *)encoded, encoded_length);

    mbedtls_platform_zeroize(encoded, sizeof(encoded));

    if (result != ESP_OK) {
        return result;
    }

    return smtp_expect(tls, expected_code);
}

static esp_err_t smtp_authenticate(esp_tls_t *tls)
{
    esp_err_t result = smtp_expect(tls, 220);

    if (result == ESP_OK) {
        result = smtp_command(
            tls, "EHLO plant-monitor.local\r\n", 250);
    }

    if (result == ESP_OK) {
        result = smtp_command(tls, "AUTH LOGIN\r\n", 334);
    }

    if (result == ESP_OK) {
        result = smtp_send_base64(tls, GMAIL_ADDRESS, 334);
    }

    if (result == ESP_OK) {
        result = smtp_send_base64(tls, GMAIL_APP_PASSWORD, 235);
    }

    return result;
}

static esp_err_t smtp_send_body(
    esp_tls_t *tls, const char *message)
{
    size_t input_length = strlen(message);

    if (input_length == 0 || input_length > 4096) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t capacity = 4 * ((input_length + 2) / 3) + 1;
    unsigned char *encoded = malloc(capacity);

    if (encoded == NULL) {
        return ESP_ERR_NO_MEM;
    }

    size_t encoded_length = 0;
    int encoding_result = mbedtls_base64_encode(
        encoded, capacity, &encoded_length,
        (const unsigned char *)message, input_length);

    if (encoding_result != 0) {
        free(encoded);
        return ESP_FAIL;
    }

    esp_err_t result = ESP_OK;

    for (size_t offset = 0; offset < encoded_length;) {
        size_t chunk = encoded_length - offset;

        if (chunk > 76) {
            chunk = 76;
        }

        result = smtp_write_all(
            tls, (const char *)encoded + offset, chunk);

        if (result == ESP_OK) {
            result = smtp_write_all(tls, "\r\n", 2);
        }

        if (result != ESP_OK) {
            break;
        }

        offset += chunk;
    }

    free(encoded);

    if (result != ESP_OK) {
        return result;
    }

    return smtp_command(tls, ".\r\n", 250);
}

esp_err_t notifications_send_gmail(const char *message)
{
    if (message == NULL || message[0] == '\0' ||
        strlen(message) > 4096) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!connectivity_is_ready() ||
        !connectivity_time_is_ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    const char *sender = GMAIL_ADDRESS;
    const char *recipient = GMAIL_RECIPIENT;

    if (strlen(sender) > 254 || strlen(recipient) > 254 ||
        strchr(sender, '@') == NULL ||
        strchr(recipient, '@') == NULL ||
        strpbrk(sender, "\r\n<> \t") != NULL ||
        strpbrk(recipient, "\r\n<> \t") != NULL ||
        strstr(sender, "REPLACE_ME") != NULL ||
        strstr(recipient, "REPLACE_ME") != NULL ||
        strlen(GMAIL_APP_PASSWORD) != 16) {
        ESP_LOGE("GMAIL", "Check local email configuration");
        return ESP_ERR_INVALID_ARG;
    }

    time_t now = time(NULL);
    struct tm utc;
    char date[64];

    if (gmtime_r(&now, &utc) == NULL ||
        strftime(date, sizeof(date),
                 "%a, %d %b %Y %H:%M:%S +0000", &utc) == 0) {
        return ESP_FAIL;
    }

    char headers[1024];
    int header_length = snprintf(
        headers, sizeof(headers),
        "From: <%s>\r\n"
        "To: <%s>\r\n"
        "Date: %s\r\n"
        "Subject: Plant-01 condition update\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: text/plain; charset=UTF-8\r\n"
        "Content-Transfer-Encoding: base64\r\n"
        "\r\n",
        sender, recipient, date);

    if (header_length < 0 ||
        header_length >= (int)sizeof(headers)) {
        return ESP_ERR_INVALID_SIZE;
    }

    esp_tls_cfg_t config = {
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };

    esp_tls_t *tls = esp_tls_init();
    if (tls == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const char *host = "smtp.gmail.com";

    if (esp_tls_conn_new_sync(
            host, strlen(host), 465, &config, tls) != 1) {
        ESP_LOGE("GMAIL", "TLS connection failed");
        esp_tls_conn_destroy(tls);
        return ESP_FAIL;
    }

    esp_err_t result = smtp_authenticate(tls);
    char command[320];
    int length;

    if (result == ESP_OK) {
        length = snprintf(
            command, sizeof(command),
            "MAIL FROM:<%s>\r\n", sender);

        if (length < 0 || length >= (int)sizeof(command)) {
            result = ESP_ERR_INVALID_SIZE;
        } else {
            result = smtp_command(tls, command, 250);
        }
    }

    if (result == ESP_OK) {
        length = snprintf(
            command, sizeof(command),
            "RCPT TO:<%s>\r\n", recipient);

        if (length < 0 || length >= (int)sizeof(command)) {
            result = ESP_ERR_INVALID_SIZE;
        } else {
            result = smtp_command(tls, command, 250);
        }
    }

    if (result == ESP_OK) {
        result = smtp_command(tls, "DATA\r\n", 354);
    }

    if (result == ESP_OK) {
        result = smtp_write_all(
            tls, headers, (size_t)header_length);
    }

    if (result == ESP_OK) {
        result = smtp_send_body(tls, message);
    }

    if (result == ESP_OK) {
        ESP_LOGI("GMAIL", "SMTP server accepted the email");

        esp_err_t quit_result =
            smtp_command(tls, "QUIT\r\n", 221);

        if (quit_result != ESP_OK) {
            ESP_LOGW("GMAIL",
                     "Session close failed after email acceptance");
        }
    }

    esp_tls_conn_destroy(tls);
    return result;
}