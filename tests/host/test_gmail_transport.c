/*
 * Exercise the real Gmail sender with a fake SMTP conversation and partial writes.
 * The copied sender includes only dummy test configuration, never the private header.
 */
#include "host_transport.h"
/* Use the captured-message structure and the four service-sending functions. */
#include "notifications.h"
/* Format bounded text with snprintf(), or use file/console I/O in host tests. */
#include <stdio.h>
/* Allocate/free temporary memory, and provide standard utility functions. */
#include <stdlib.h>
/* Measure, compare, copy, or search text and byte arrays. */
#include <string.h>
/* Use Windows types required by the offline Base64 stand-in. */
#include <windows.h>
/* Use Windows Base64 conversion to stand in for mbedTLS in the host test. */
#include <wincrypt.h>

/* Track bytes already written, or hold the fake test connection's captured outgoing bytes. */
static char sent[65536];
/* Track how many bytes have been stored; leave space for the final string-ending byte. */
static size_t used;
/* Track the next byte to return from the prewritten fake SMTP replies. */
static size_t reply_offset;
/* Count fake TLS sessions so tests can detect a connection cleanup leak. */
static int open_connections;
/* Enable a deliberate fake network-write failure for the cleanup test. */
static bool break_write;
/*
 * Prewrite the expected SMTP conversation, including a multiline EHLO reply, for repeatable
 * offline checks.
 */
static const char replies[] =
    "220 ready\r\n250-hello\r\n250 auth\r\n334 user\r\n334 password\r\n"
    "235 authenticated\r\n250 sender\r\n250 recipient\r\n354 send data\r\n"
    "250 queued\r\n221 closing\r\n";

/*
 * Host-only stub: satisfy the sender's callback signature without claiming to verify a real
 * certificate.
 */
int esp_crt_bundle_attach(void *unused)
{
    /*
     * This callback/task must accept the parameter, but this implementation intentionally does
     * not use it.
     */
    (void)unused;
    /* Finish successfully; the caller may now use this function's output. */
    return ESP_OK;
}
/*
 * Host-only stand-in: pretend Wi-Fi is ready so this test reaches the SMTP message-building
 * code.
 */
bool connectivity_is_ready(void)
{
    /* Return yes/true: the condition this helper is checking has been satisfied. */
    return true;
}
/* Host-only stand-in: pretend Internet time is ready; no SNTP service runs in this executable. */
bool connectivity_time_is_ready(void)
{
    /* Return yes/true: the condition this helper is checking has been satisfied. */
    return true;
}
/*
 * Host-only stub: keep time fixed so this test exercises message I/O, not real elapsed-time
 * behavior.
 */
int64_t esp_timer_get_time(void)
{
    /* Return a fixed fake uptime value; this does not read a live device clock. */
    return 100;
}
/* Host-only stand-in: accept a delay request without sleeping, keeping the simulation fast. */
void vTaskDelay(uint32_t ticks)
{
    /*
     * This callback/task must accept the parameter, but this implementation intentionally does
     * not use it.
     */
    (void)ticks;
}
/* Allocate a fake session and count it so the test can detect a missing cleanup call. */
esp_tls_t *esp_tls_init(void)
{
    /* Create/hold the TLS session object; release it on success and every later error path. */
    esp_tls_t *tls = calloc(1, sizeof(*tls));
    /* Only count/use this fake session after its allocation succeeded. */
    if (tls)
        /* Count one successfully allocated fake connection so later cleanup can be checked. */
        open_connections++;
    /* Give the allocated fake session back to the real Gmail code under test. */
    return tls;
}
/*
 * Declare/check the fake connection opening step; tests verify the host, port, and timeout
 * settings.
 */
int esp_tls_conn_new_sync(const char *host, int length, int port, const esp_tls_cfg_t *config,
                          esp_tls_t *tls)
{
    /*
     * This callback/task must accept the parameter, but this implementation intentionally does
     * not use it.
     */
    (void)length;
    /*
     * This callback/task must accept the parameter, but this implementation intentionally does
     * not use it.
     */
    (void)tls;
    /*
     * Pretend to connect only when the real sender selected Gmail, TLS port 465, and a ten-
     * second timeout.
     */
    return strcmp(host, "smtp.gmail.com") == 0 && port == 465 && config->timeout_ms == 10000;
}
/*
 * Fake network writer: capture SMTP bytes locally, force partial writes, and optionally inject a
 * failure.
 */
ssize_t esp_tls_conn_write(esp_tls_t *tls, const void *bytes, size_t size)
{
    /*
     * This callback/task must accept the parameter, but this implementation intentionally does
     * not use it.
     */
    (void)tls;
    /* Inject or handle the chosen fake write failure; no real network is involved. */
    if (break_write && used > 250)
        /*
         * Report a fake read/write/conversion failure so the sender's error-handling path is
         * exercised.
         */
        return -1;
    /* Force the actual sender to handle writes that accept only part of a buffer. */
    if (size > 17)
        /* Accept at most 17 bytes to force the real sender's partial-write loop to run. */
        size = 17;
    /* Reject capture-buffer overflow and keep room for its final string-ending byte. */
    if (used + size >= sizeof(sent))
        /*
         * Report a fake read/write/conversion failure so the sender's error-handling path is
         * exercised.
         */
        return -1;
    /* Copy exactly this many bytes; capacity is checked before reaching this copy. */
    memcpy(sent + used, bytes, size);
    /* Count the bytes the fake write accepted into its capture array. */
    used += size;
    /* Terminate captured outgoing SMTP bytes for safe local text inspection. */
    sent[used] = '\0';
    /* Report the actual accepted byte count so the real sender knows how far to advance. */
    return (ssize_t)size;
}
/* Fake network reader: supply one byte at a time from the prewritten SMTP replies. */
ssize_t esp_tls_conn_read(esp_tls_t *tls, void *bytes, size_t size)
{
    /*
     * This callback/task must accept the parameter, but this implementation intentionally does
     * not use it.
     */
    (void)tls;
    /*
     * This callback/task must accept the parameter, but this implementation intentionally does
     * not use it.
     */
    (void)size;
    /* Reject reads after all fake reply characters have been consumed. */
    if (reply_offset >= sizeof(replies) - 1)
        /*
         * Report a fake read/write/conversion failure so the sender's error-handling path is
         * exercised.
         */
        return -1;
    /* Return the next prewritten fake SMTP reply byte and advance the reply cursor. */
    *(char *)bytes = replies[reply_offset++];
    /*
     * Return a nonzero process status so the computer-side runner treats this test invocation as
     * failed.
     */
    return 1;
}
/* Release the TLS connection object even if authentication or a later SMTP step failed. */
void esp_tls_conn_destroy(esp_tls_t *tls)
{
    /* Only free/count an allocated fake session; a NULL pointer owns no connection. */
    if (tls) {
        /* Count the fake connection as closed before freeing its object. */
        open_connections--;
        /*
         * Release this temporary heap allocation; free(NULL) is safe for partial-allocation
         * cleanup.
         */
        free(tls);
    }
}
/*
 * Declare the host Base64 substitute used to test message construction, not real TLS
 * cryptography.
 */
int mbedtls_base64_encode(unsigned char *destination, size_t capacity, size_t *length,
                          const unsigned char *source, size_t size)
{
    /* Use Windows' Base64 implementation as the offline mbedTLS stand-in. */
    DWORD count = (DWORD)capacity;
    /*
     * Use Windows Base64 encoding without its own line breaks; the real sender performs MIME
     * wrapping.
     */
    if (!CryptBinaryToStringA(source, (DWORD)size, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                              (char *)destination, &count))
        /*
         * Report a fake read/write/conversion failure so the sender's error-handling path is
         * exercised.
         */
        return -1;
    /* Return the actual Base64 output length to the real Gmail sender under test. */
    *length = strlen((const char *)destination);
    /* Return a successful process/test status to the computer-side runner. */
    return 0;
}
/* Overwrite temporary login bytes so that encoded credentials are not left in this buffer. */
void mbedtls_platform_zeroize(void *bytes, size_t size)
{
    /* Use volatile writes so the compiler cannot simply discard the test buffer-erasing loop. */
    volatile unsigned char *p = bytes;
    /* Overwrite each byte of this test buffer once. */
    while (size--)
        /* Write zero to this byte, then advance the pointer to the next byte to erase. */
        *p++ = 0;
}

/*
 * Computer-side test entry point: validate arguments, run this executable's checks, and return
 * pass/fail.
 */
int main(int argc, char **argv)
{
    /* Require the expected test command-line arguments before reading argv entries. */
    if (argc != 3)
        /*
         * Return a nonzero process status so the computer-side runner treats this test
         * invocation as failed.
         */
        return 2;
    /* Keep one event's values together while the senders build their own presentation. */
    notification_message_t message = {.state = "DRY",
                                      /*
                                       * Copy this captured event's simulated percentage, not a
                                       * newer live reading.
                                       */
                                      .moisture_percent = 26,
                                      /*
                                       * Use temperature captured with this event;
                                       * environment_valid determines whether it is displayed.
                                       */
                                      .temperature = 28.0f,
                                      /*
                                       * Use humidity from the same captured reading, keeping the
                                       * values paired.
                                       */
                                      .humidity = 77.0f,
                                      /*
                                       * Mark both environment values usable only if this event's
                                       * DHT read succeeded.
                                       */
                                      .environment_valid = true,
                                      /*
                                       * Keep the original captured Unix time; the formatter
                                       * converts only its display to PhST.
                                       */
                                      .measured_at = (time_t)1792973754,
                                      /*
                                       * Carry the acquisition-time validity flag instead of
                                       * substituting a later synchronized time.
                                       */
                                      .time_valid = true};
    /* Compare the actual characters; strcmp returns zero when both state words match. */
    if (strcmp(argv[2], "missing-dht") == 0)
        /*
         * Set this controlled test input/flag for the scenario named by the surrounding test
         * function.
         */
        message.environment_valid = false;
    /* Compare the actual characters; strcmp returns zero when both state words match. */
    if (strcmp(argv[2], "unknown-clock") == 0)
        /*
         * Set this controlled test input/flag for the scenario named by the surrounding test
         * function.
         */
        message.time_valid = false;
    /* Enable write-failure injection only for the failed-write test mode. */
    break_write = strcmp(argv[2], "failed-write") == 0;
    /* Pass this captured message to the named service; return status is checked below. */
    esp_err_t result = notifications_send_gmail(&message);
    /* Fail if the real sender left a fake TLS session unclosed. */
    if (open_connections != 0) {
        /* Print a test failure explanation to stderr so the runner can show which check failed. */
        fprintf(stderr, "TLS connection leaked\n");
        /*
         * Return a nonzero process status so the computer-side runner treats this test
         * invocation as failed.
         */
        return 1;
    }
    /* Inject or handle the chosen fake write failure; no real network is involved. */
    if (break_write) {
        /* Continue to the next step only if the preceding operation succeeded. */
        if (result == ESP_OK)
            /*
             * Return a nonzero process status so the computer-side runner treats this test
             * invocation as failed.
             */
            return 1;
        /* Print the result of this completed offline check. */
        puts("PASS Gmail failed-write cleanup");
        /* Return a successful process/test status to the computer-side runner. */
        return 0;
    }
    /* Handle/return the earlier failure instead of continuing with invalid output. */
    if (result != ESP_OK) {
        /* Print a test failure explanation to stderr so the runner can show which check failed. */
        fprintf(stderr, "SMTP simulation failed: %d\n", result);
        /*
         * Return a nonzero process status so the computer-side runner treats this test
         * invocation as failed.
         */
        return 1;
    }
    /*
     * Open the requested local capture file in binary mode; this does not contact a mail
     * service.
     */
    FILE *file = fopen(argv[1], "wb");
    /* Stop if the local artifact file could not be opened. */
    if (!file)
        /*
         * Return a nonzero process status so the computer-side runner treats this test
         * invocation as failed.
         */
        return 2;
    /* Write these captured/generated bytes to the opened local test artifact file. */
    fwrite(sent, 1, used, file);
    /* Close the local file so its saved output is complete and the handle is released. */
    fclose(file);
    /* Print the result of this completed offline check. */
    puts("PASS Gmail SMTP conversation with partial writes");
    /* Return a successful process/test status to the computer-side runner. */
    return 0;
}
