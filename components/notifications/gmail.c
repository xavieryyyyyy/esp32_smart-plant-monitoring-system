/*
 * This file sends the notification task's message as an email through Gmail.
 * SMTP is the conversation used to submit email; TLS protects the connection.
 * The steps are: connect, say hello, sign in, name sender/recipient, send text, close.
 * Only reply codes/errors are printed; the password is not printed.
 * Each call tries one email session. Failed emails are not automatically resent.
 */
/*
 * SMTP/MIME reading key:
 * A command is a request sent over the already encrypted connection. A three-digit
 * reply says whether Gmail accepted that step; each helper checks the required code.
 * CRLF (\r\n) is the network line ending. MIME boundary strings separate the text,
 * HTML, and image PARTS inside one email. Only the final dot command ends SMTP DATA.
 * Base64 changes bytes into transport-safe text. It is not encryption; TLS protects
 * the email and app password while they travel to Gmail.
 */
#include "notifications.h"
/* Use the shared text, PhST, and JSON builders instead of duplicating layouts. */
#include "notification_format.h"
/* Read private settings from the ignored local file; never print its secret values. */
#include "config.local.h"
/* Use the networking component to start Wi-Fi or ask whether Wi-Fi/time is ready. */
#include "connectivity.h"
/* Erase temporary encoded login data after use. */
#include "mbedtls/platform_util.h"
/* Format bounded text with snprintf(), or use file/console I/O in host tests. */
#include <stdio.h>
/* Allocate/free temporary memory, and provide standard utility functions. */
#include <stdlib.h>
/* Measure, compare, copy, or search text and byte arrays. */
#include <string.h>
/* Use FreeRTOS types and constants for tasks, queues, and waiting. */
#include "freertos/FreeRTOS.h"
/* Create background jobs, pause them, or wake them with task notifications. */
#include "freertos/task.h"
/* Use the trusted root certificates when checking a server certificate. */
#include "esp_crt_bundle.h"
/* Print tagged information, warnings, and errors in the serial monitor. */
#include "esp_log.h"
/* Read the microsecond timer since startup; use it for ages and waiting deadlines. */
#include "esp_timer.h"
/* Open, read, write, and close the protected connection used by Gmail SMTP. */
#include "esp_tls.h"
/* Encode email content/login text as Base64; TLS supplies the encryption. */
#include "mbedtls/base64.h"
/* Store Unix timestamps and convert them into calendar date/time fields. */
#include <time.h>

/*
 * A network write may send only part of the text. Keep sending the part left over.
 * Check a ten-second deadline while doing this; return an error if the connection fails.
 */
static esp_err_t smtp_write_all(esp_tls_t *tls, const char *data, size_t length)
{
    /* Track bytes already written, or hold the fake test connection's captured outgoing bytes. */
    size_t sent = 0;
    /* Set a limit ten seconds from now for this write/reply operation, using the uptime clock. */
    int64_t deadline = esp_timer_get_time() + 10000000LL;

    /* Repeat until the actual written-byte count reaches the requested total. */
    while (sent < length) {
        /* Compare uptime against this operation's deadline so network work cannot wait forever. */
        if (esp_timer_get_time() >= deadline) {
            /* Report that this operation exceeded its allowed wait. */
            return ESP_ERR_TIMEOUT;
        }

        /*
         * Ask TLS to write the unsent suffix; the result is accepted bytes or a negative
         * TLS/error code.
         */
        ssize_t result = esp_tls_conn_write(tls, data + sent, length - sent);

        /* A positive byte count means some data was sent; advance by that actual amount. */
        if (result > 0) {
            /*
             * Advance by the actual number of bytes the connection accepted, not the requested
             * length.
             */
            sent += (size_t)result;
            /*
         * The protected connection is not ready yet; yield before trying the same operation
         * again.
         */
        } else if (result == ESP_TLS_ERR_SSL_WANT_READ || result == ESP_TLS_ERR_SSL_WANT_WRITE) {
            /*
             * Pause this task and let other tasks run; pdMS_TO_TICKS converts milliseconds to
             * scheduler ticks.
             */
            vTaskDelay(pdMS_TO_TICKS(10) + 1);
        } else {
            /* Report a failed operation; do not treat this path as successful delivery. */
            return ESP_FAIL;
        }
    }

    /* Finish successfully; the caller may now use this function's output. */
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
    /* Set a limit ten seconds from now for this write/reply operation, using the uptime clock. */
    int64_t deadline = esp_timer_get_time() + 10000000LL;

    /* Limit this SMTP reply to 64 lines so a malformed server cannot extend it indefinitely. */
    for (int lines = 0; lines < 64; lines++) {
        /* Reserve up to 512 SMTP reply bytes plus one final zero byte. */
        char line[513];
        /* Track how many bytes have been stored; leave space for the final string-ending byte. */
        size_t used = 0;

        /*
         * Keep this task/read loop running; its wait, break, or error path below decides when to
         * proceed.
         */
        while (1) {
            /*
             * Compare uptime against this operation's deadline so network work cannot wait
             * forever.
             */
            if (esp_timer_get_time() >= deadline) {
                /* Report that this operation exceeded its allowed wait. */
                return ESP_ERR_TIMEOUT;
            }

            /* Receive one byte at a time so SMTP lines can be checked before moving on. */
            char character;
            /* Ask TLS for one reply byte; the result is a byte count or a retry/error code. */
            ssize_t result = esp_tls_conn_read(tls, &character, 1);

            /*
             * The protected connection is not ready yet; yield before trying the same operation
             * again.
             */
            if (result == ESP_TLS_ERR_SSL_WANT_READ || result == ESP_TLS_ERR_SSL_WANT_WRITE) {
                /*
                 * Pause this task and let other tasks run; pdMS_TO_TICKS converts milliseconds
                 * to scheduler ticks.
                 */
                vTaskDelay(pdMS_TO_TICKS(10) + 1);
                /*
                 * Skip the rest of this loop iteration and return to its next receive/retry
                 * step.
                 */
                continue;
            }

            /*
             * Zero/negative data length here means the connection/read cannot provide a valid
             * reply.
             */
            if (result <= 0) {
                /* Report a failed operation; do not treat this path as successful delivery. */
                return ESP_FAIL;
            }

            /*
             * Leave room for the final zero byte; reject an SMTP reply line that exceeds the
             * buffer.
             */
            if (used >= sizeof(line) - 1) {
                /* Report that the available buffer/format limit could not safely hold this data. */
                return ESP_ERR_INVALID_SIZE;
            }

            /* Store this received character and move to the next free position in the SMTP line. */
            line[used++] = character;

            /* A newline completes this reply line; finish collecting and validate it below. */
            if (character == '\n') {
                /*
                 * Leave the nearest loop or switch; do not execute its remaining steps for this
                 * item.
                 */
                break;
            }
        }

        /* Terminate the completed reply line so later text/code checks stay within it. */
        line[used] = '\0';

        /* Require a three-digit reply code and a complete CRLF-terminated SMTP line. */
        if (used < 6 || line[used - 2] != '\r' || line[0] < '0' || line[0] > '9' || line[1] < '0' ||
            line[1] > '9' || line[2] < '0' || line[2] > '9') {
            /* Report a failed operation; do not treat this path as successful delivery. */
            return ESP_FAIL;
        }

        /* Turn the first three digit characters into the numeric SMTP response code. */
        int code = (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');

        /* A different SMTP reply code means this step was not accepted as expected. */
        if (code != expected_code) {
            /* Print a warning about a recoverable problem or skipped operation. */
            ESP_LOGW("GMAIL", "SMTP reply %d; expected %d", code, expected_code);
            /* Report a failed operation; do not treat this path as successful delivery. */
            return ESP_FAIL;
        }

        /* A space after the SMTP code marks the final line of a successful multiline reply. */
        if (line[3] == ' ') {
            /* Print an informational progress/result line with a tag for the serial monitor. */
            ESP_LOGI("GMAIL", "SMTP reply: %d", code);
            /* Finish successfully; the caller may now use this function's output. */
            return ESP_OK;
        }

        /* Only a hyphen may mark another SMTP reply line; reject any other separator. */
        if (line[3] != '-') {
            /* Report a failed operation; do not treat this path as successful delivery. */
            return ESP_FAIL;
        }
    }

    /* Report that the available buffer/format limit could not safely hold this data. */
    return ESP_ERR_INVALID_SIZE;
}

/*
 * Send one email command, then wait for its expected reply.
 * If the send failed, return that error without pretending to read a successful reply.
 */
static esp_err_t smtp_command(esp_tls_t *tls, const char *command, int expected_code)
{
    /*
     * Write every byte of this SMTP fragment, handling partial writes and its operation
     * deadline.
     */
    esp_err_t result = smtp_write_all(tls, command, strlen(command));

    /* Handle/return the earlier failure instead of continuing with invalid output. */
    if (result != ESP_OK) {
        /* Pass the actual success/error code back to the caller; do not replace it with success. */
        return result;
    }

    /* Wait for and validate the SMTP reply code required by this step. */
    return smtp_expect(tls, expected_code);
}

/*
 * Gmail's AUTH LOGIN step expects the login text in Base64, another text format.
 * Base64 does not hide a password; the encrypted TLS connection protects it.
 */
static esp_err_t smtp_send_base64(esp_tls_t *tls, const char *value, int expected_code)
{
    /* Reserve memory for Base64 text; this changes representation, not encryption. */
    unsigned char encoded[512];
    /* Receive how many Base64 bytes were produced, excluding any later SMTP line endings. */
    size_t encoded_length = 0;

    /* Keep this operation's success/error code so later steps can stop safely on failure. */
    int encoding_result = mbedtls_base64_encode(encoded, sizeof(encoded) - 3, &encoded_length,
                                                (const unsigned char *)value, strlen(value));

    /* Handle Base64 conversion failure and release/erase temporary storage. */
    if (encoding_result != 0) {
        /*
         * Overwrite temporary login bytes so that encoded credentials are not left in this
         * buffer.
         */
        mbedtls_platform_zeroize(encoded, sizeof(encoded));
        /* Report that the available buffer/format limit could not safely hold this data. */
        return ESP_ERR_INVALID_SIZE;
    }

    /* Append the SMTP CR/LF line ending after the encoded login value. */
    encoded[encoded_length++] = '\r';
    /* Append the SMTP CR/LF line ending after the encoded login value. */
    encoded[encoded_length++] = '\n';

    /*
     * Write every byte of this SMTP fragment, handling partial writes and its operation
     * deadline.
     */
    esp_err_t result = smtp_write_all(tls, (const char *)encoded, encoded_length);

    /* Clear the temporary Base64 login text from this array after trying to send it. */
    mbedtls_platform_zeroize(encoded, sizeof(encoded));

    /* Handle/return the earlier failure instead of continuing with invalid output. */
    if (result != ESP_OK) {
        /* Pass the actual success/error code back to the caller; do not replace it with success. */
        return result;
    }

    /* Wait for and validate the SMTP reply code required by this step. */
    return smtp_expect(tls, expected_code);
}

/*
 * Wait for Gmail's greeting, introduce the ESP32, then supply the email and app password.
 */
static esp_err_t smtp_authenticate(esp_tls_t *tls)
{
    /* Wait for and validate the SMTP reply code required by this step. */
    esp_err_t result = smtp_expect(tls, 220);

    /* Continue to the next step only if the preceding operation succeeded. */
    if (result == ESP_OK) {
        /* Introduce this device and check the server's capabilities reply (250). */
        result = smtp_command(tls, "EHLO plant-monitor.local\r\n", 250);
    }

    /* Continue to the next step only if the preceding operation succeeded. */
    if (result == ESP_OK) {
        /* Start SMTP login; reply 334 asks for the first encoded login value. */
        result = smtp_command(tls, "AUTH LOGIN\r\n", 334);
    }

    /* Continue to the next step only if the preceding operation succeeded. */
    if (result == ESP_OK) {
        /*
         * Send this one login value in SMTP's required Base64 representation over the existing
         * TLS session.
         */
        result = smtp_send_base64(tls, GMAIL_ADDRESS, 334);
    }

    /* Continue to the next step only if the preceding operation succeeded. */
    if (result == ESP_OK) {
        /*
         * Send this one login value in SMTP's required Base64 representation over the existing
         * TLS session.
         */
        result = smtp_send_base64(tls, GMAIL_APP_PASSWORD, 235);
    }

    /* Pass the actual success/error code back to the caller; do not replace it with success. */
    return result;
}

/*
 * Convert the email text to Base64 and send it in lines of at most 76 characters.
 * The multipart sender below writes the final dot only after every part is complete.
 */
static esp_err_t smtp_write_base64_body(esp_tls_t *tls, const char *message)
{
    /* Count the source text bytes before calculating Base64 storage. */
    size_t input_length = strlen(message);

    /* Reject an empty or over-limit email part before allocating its encoded representation. */
    if (input_length == 0 || input_length >= NOTIFICATION_HTML_CAPACITY) {
        /* Reject an absent, unsafe, or out-of-range input before using it. */
        return ESP_ERR_INVALID_ARG;
    }

    /* Allow four Base64 characters per three input bytes, plus a string-ending byte. */
    size_t capacity = 4 * ((input_length + 2) / 3) + 1;
    /* Reserve memory for Base64 text; this changes representation, not encryption. */
    unsigned char *encoded = malloc(capacity);

    /* Check for an absent pointer or failed allocation before dereferencing/using it. */
    if (encoded == NULL) {
        /*
         * Report that temporary storage could not be allocated; the caller must handle the
         * failure.
         */
        return ESP_ERR_NO_MEM;
    }

    /* Receive how many Base64 bytes were produced, excluding any later SMTP line endings. */
    size_t encoded_length = 0;
    /* Keep this operation's success/error code so later steps can stop safely on failure. */
    int encoding_result = mbedtls_base64_encode(encoded, capacity, &encoded_length,
                                                (const unsigned char *)message, input_length);

    /* Handle Base64 conversion failure and release/erase temporary storage. */
    if (encoding_result != 0) {
        /*
         * Release this temporary heap allocation; free(NULL) is safe for partial-allocation
         * cleanup.
         */
        free(encoded);
        /* Report a failed operation; do not treat this path as successful delivery. */
        return ESP_FAIL;
    }

    /* Keep this operation's success/error code so later steps can stop safely on failure. */
    esp_err_t result = ESP_OK;

    /* Send the encoded email part in chunks until every Base64 byte is written. */
    for (size_t offset = 0; offset < encoded_length;) {
        /* Start with the remaining encoded byte count; cap each outgoing line at 76 below. */
        size_t chunk = encoded_length - offset;

        /* Keep Base64 lines within the MIME limit of 76 encoded characters. */
        if (chunk > 76) {
            /* Limit this outgoing Base64 line to 76 characters, as required for MIME encoding. */
            chunk = 76;
        }

        /*
         * Write every byte of this SMTP fragment, handling partial writes and its operation
         * deadline.
         */
        result = smtp_write_all(tls, (const char *)encoded + offset, chunk);

        /* Continue to the next step only if the preceding operation succeeded. */
        if (result == ESP_OK) {
            /*
             * Write every byte of this SMTP fragment, handling partial writes and its operation
             * deadline.
             */
            result = smtp_write_all(tls, "\r\n", 2);
        }

        /* Handle/return the earlier failure instead of continuing with invalid output. */
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