/*
 * This file sends the notification task's message as an email through Gmail.
 * SMTP is the conversation used to submit email; TLS protects the connection.
 * The steps are: connect, say hello, sign in, name sender/recipient, send text, close.
 * Only reply codes/errors are printed; the password is not printed.
 * Each call tries one email session. Failed emails are not automatically resent.
 */
#include "notifications.h"
#include "notification_format.h"
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

/*
 * A network write may send only part of the text. Keep sending the part left over.
 * Check a ten-second deadline while doing this; return an error if the connection fails.
 */
static esp_err_t smtp_write_all(esp_tls_t *tls, const char *data, size_t length)
{
    size_t sent = 0;
    int64_t deadline = esp_timer_get_time() + 10000000LL;

    while (sent < length) {
        if (esp_timer_get_time() >= deadline) {
            return ESP_ERR_TIMEOUT;
        }

        ssize_t result = esp_tls_conn_write(tls, data + sent, length - sent);

        if (result > 0) {
            sent += (size_t)result;
        } else if (result == ESP_TLS_ERR_SSL_WANT_READ || result == ESP_TLS_ERR_SSL_WANT_WRITE) {
            vTaskDelay(pdMS_TO_TICKS(10) + 1);
        } else {
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

/*
 * Read Gmail's reply and check the expected three-digit number.
 * For example, 250- means another reply line follows; 250 plus a space is the final line.
 * Limit line size, line count, and waiting time. These limits are for this reply,
 * not one total deadline for the whole email conversation.
 */
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
            ssize_t result = esp_tls_conn_read(tls, &character, 1);

            if (result == ESP_TLS_ERR_SSL_WANT_READ || result == ESP_TLS_ERR_SSL_WANT_WRITE) {
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

        if (used < 6 || line[used - 2] != '\r' || line[0] < '0' || line[0] > '9' || line[1] < '0' ||
            line[1] > '9' || line[2] < '0' || line[2] > '9') {
            return ESP_FAIL;
        }

        int code = (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');

        if (code != expected_code) {
            ESP_LOGW("GMAIL", "SMTP reply %d; expected %d", code, expected_code);
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

/*
 * Send one email command, then wait for its expected reply.
 * If the send failed, return that error without pretending to read a successful reply.
 */
static esp_err_t smtp_command(esp_tls_t *tls, const char *command, int expected_code)
{
    esp_err_t result = smtp_write_all(tls, command, strlen(command));

    if (result != ESP_OK) {
        return result;
    }

    return smtp_expect(tls, expected_code);
}

/*
 * Gmail's AUTH LOGIN step expects the login text in Base64, another text format.
 * Base64 does not hide a password; the encrypted TLS connection protects it.
 */
static esp_err_t smtp_send_base64(esp_tls_t *tls, const char *value, int expected_code)
{
    unsigned char encoded[512];
    size_t encoded_length = 0;

    int encoding_result = mbedtls_base64_encode(encoded, sizeof(encoded) - 3, &encoded_length,
                                                (const unsigned char *)value, strlen(value));

    if (encoding_result != 0) {
        mbedtls_platform_zeroize(encoded, sizeof(encoded));
        return ESP_ERR_INVALID_SIZE;
    }

    encoded[encoded_length++] = '\r';
    encoded[encoded_length++] = '\n';

    esp_err_t result = smtp_write_all(tls, (const char *)encoded, encoded_length);

    /* Clear the temporary Base64 login text from this array after trying to send it. */
    mbedtls_platform_zeroize(encoded, sizeof(encoded));

    if (result != ESP_OK) {
        return result;
    }

    return smtp_expect(tls, expected_code);
}

/*
 * Wait for Gmail's greeting, introduce the ESP32, then supply the email and app password.
 */
static esp_err_t smtp_authenticate(esp_tls_t *tls)
{
    esp_err_t result = smtp_expect(tls, 220);

    if (result == ESP_OK) {
        result = smtp_command(tls, "EHLO plant-monitor.local\r\n", 250);
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

/*
 * Convert the email text to Base64 and send it in lines of at most 76 characters.
 * The multipart sender below writes the final dot only after every part is complete.
 */
static esp_err_t smtp_write_base64_body(esp_tls_t *tls, const char *message)
{
    size_t input_length = strlen(message);

    if (input_length == 0 || input_length >= NOTIFICATION_HTML_CAPACITY) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t capacity = 4 * ((input_length + 2) / 3) + 1;
    unsigned char *encoded = malloc(capacity);

    if (encoded == NULL) {
        return ESP_ERR_NO_MEM;
    }

    size_t encoded_length = 0;
    int encoding_result = mbedtls_base64_encode(encoded, capacity, &encoded_length,
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

        result = smtp_write_all(tls, (const char *)encoded + offset, chunk);

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

    return ESP_OK;
}

/*
 * Send one email with a plain-text alternative, the HTML card, and its inline plant mark.
 * A mail reader chooses the version it supports. End SMTP DATA once, after all parts.
 */
static esp_err_t smtp_send_body(esp_tls_t *tls, const char *plain, const char *html)
{
    const char *start =
        "--plant-monitor-related\r\n"
        "Content-Type: multipart/alternative; boundary=\"plant-monitor-alternative\"\r\n\r\n"
        "--plant-monitor-alternative\r\n"
        "Content-Type: text/plain; charset=UTF-8\r\n"
        "Content-Transfer-Encoding: base64\r\n\r\n";
    const char *html_start = "\r\n--plant-monitor-alternative\r\n"
                             "Content-Type: text/html; charset=UTF-8\r\n"
                             "Content-Transfer-Encoding: base64\r\n\r\n";
    const char *image_start = "\r\n--plant-monitor-alternative--\r\n"
                              "--plant-monitor-related\r\n"
                              "Content-Type: image/png\r\n"
                              "Content-Transfer-Encoding: base64\r\n"
                              "Content-ID: <plant-mark>\r\n"
                              "Content-Disposition: inline; filename=\"plant-mark.png\"\r\n\r\n";
    const char *end = "--plant-monitor-related--\r\n";

    esp_err_t result = smtp_write_all(tls, start, strlen(start));
    if (result == ESP_OK)
        result = smtp_write_base64_body(tls, plain);
    if (result == ESP_OK)
        result = smtp_write_all(tls, html_start, strlen(html_start));
    if (result == ESP_OK)
        result = smtp_write_base64_body(tls, html);
    if (result == ESP_OK)
        result = smtp_write_all(tls, image_start, strlen(image_start));
    if (result == ESP_OK)
        result = smtp_write_all(tls, notification_mark_base64, strlen(notification_mark_base64));
    if (result == ESP_OK)
        result = smtp_write_all(tls, end, strlen(end));
    if (result != ESP_OK)
        return result;
    return smtp_command(tls, ".\r\n", 250);
}

esp_err_t notifications_send_gmail(const notification_message_t *message)
{
    if (message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!connectivity_is_ready() || !connectivity_time_is_ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    /*
     * Read private sender/recipient settings, then reject placeholders or unsafe address text.
     */
    const char *sender = GMAIL_ADDRESS;
    const char *recipient = GMAIL_RECIPIENT;

    if (strlen(sender) > 254 || strlen(recipient) > 254 || strchr(sender, '@') == NULL ||
        strchr(recipient, '@') == NULL || strpbrk(sender, "\r\n<> \t") != NULL ||
        strpbrk(recipient, "\r\n<> \t") != NULL || strstr(sender, "REPLACE_ME") != NULL ||
        strstr(recipient, "REPLACE_ME") != NULL || strlen(GMAIL_APP_PASSWORD) != 16) {
        ESP_LOGE("GMAIL", "Check local email configuration");
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * This Date header says when we submitted the email.
     * The measurement time inside the message still says when the sensors were read.
     */
    time_t now = time(NULL);
    struct tm utc;
    char date[64];

    if (gmtime_r(&now, &utc) == NULL ||
        strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S +0000", &utc) == 0) {
        return ESP_FAIL;
    }

    /* Build the email's From, To, Date, Subject, and text-format information. */
    char headers[1024];
    int header_length = snprintf(headers, sizeof(headers),
                                 "From: <%s>\r\n"
                                 "To: <%s>\r\n"
                                 "Date: %s\r\n"
                                 "Subject: Plant-01 condition update\r\n"
                                 "MIME-Version: 1.0\r\n"
                                 "Content-Type: multipart/related; type=\"multipart/alternative\"; "
                                 "boundary=\"plant-monitor-related\"\r\n"
                                 "\r\n",
                                 sender, recipient, date);

    if (header_length < 0 || header_length >= (int)sizeof(headers)) {
        return ESP_ERR_INVALID_SIZE;
    }

    /* Use heap memory for the larger HTML card so the task stack stays small. */
    char *plain = malloc(NOTIFICATION_TEXT_CAPACITY);
    char *html = malloc(NOTIFICATION_HTML_CAPACITY);
    if (plain == NULL || html == NULL) {
        free(plain);
        free(html);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t result = notification_format_text(message, false, plain, NOTIFICATION_TEXT_CAPACITY);
    if (result == ESP_OK)
        result = notification_format_gmail(message, html, NOTIFICATION_HTML_CAPACITY);
    if (result != ESP_OK) {
        free(plain);
        free(html);
        return result;
    }

    esp_tls_cfg_t config = {
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };

    esp_tls_t *tls = esp_tls_init();
    if (tls == NULL) {
        free(plain);
        free(html);
        return ESP_ERR_NO_MEM;
    }

    const char *host = "smtp.gmail.com";

    /* Open the protected connection to Gmail's SMTP server on port 465. */
    if (esp_tls_conn_new_sync(host, strlen(host), 465, &config, tls) != 1) {
        ESP_LOGE("GMAIL", "TLS connection failed");
        esp_tls_conn_destroy(tls);
        free(plain);
        free(html);
        return ESP_FAIL;
    }

    result = smtp_authenticate(tls);
    char command[320];
    int length;

    if (result == ESP_OK) {
        /* Tell Gmail which account is sending the email. */
        length = snprintf(command, sizeof(command), "MAIL FROM:<%s>\r\n", sender);

        if (length < 0 || length >= (int)sizeof(command)) {
            result = ESP_ERR_INVALID_SIZE;
        } else {
            result = smtp_command(tls, command, 250);
        }
    }

    if (result == ESP_OK) {
        /* Tell Gmail which address should receive the email. */
        length = snprintf(command, sizeof(command), "RCPT TO:<%s>\r\n", recipient);

        if (length < 0 || length >= (int)sizeof(command)) {
            result = ESP_ERR_INVALID_SIZE;
        } else {
            result = smtp_command(tls, command, 250);
        }
    }

    if (result == ESP_OK) {
        /* Ask permission to send the email headers and body next. */
        result = smtp_command(tls, "DATA\r\n", 354);
    }

    if (result == ESP_OK) {
        result = smtp_write_all(tls, headers, (size_t)header_length);
    }

    if (result == ESP_OK) {
        result = smtp_send_body(tls, plain, html);
    }

    if (result == ESP_OK) {
        /*
         * Gmail accepted the submitted email. A matching inbox message is the delivery evidence.
         */
        ESP_LOGI("GMAIL", "SMTP server accepted the email");

        esp_err_t quit_result = smtp_command(tls, "QUIT\r\n", 221);

        if (quit_result != ESP_OK) {
            ESP_LOGW("GMAIL", "Session close failed after email acceptance");
        }
    }

    /* Release the connection even if a step failed, then return the send result. */
    esp_tls_conn_destroy(tls);
    free(plain);
    free(html);
    return result;
}