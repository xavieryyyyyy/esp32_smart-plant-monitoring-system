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
void esp_tls_conn_destroy(esp_tls_t *tls)
{
    if (tls) {
        open_connections--;
        free(tls);
    }
}
int mbedtls_base64_encode(unsigned char *destination, size_t capacity, size_t *length,
                          const unsigned char *source, size_t size)
{
    /* Use Windows' Base64 implementation as the offline mbedTLS stand-in. */
    DWORD count = (DWORD)capacity;
    if (!CryptBinaryToStringA(source, (DWORD)size, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                              (char *)destination, &count))
        return -1;
    *length = strlen((const char *)destination);
    return 0;
}
void mbedtls_platform_zeroize(void *bytes, size_t size)
{
    volatile unsigned char *p = bytes;
    while (size--)
        *p++ = 0;
}

int main(int argc, char **argv)
{
    if (argc != 3)
        return 2;
    notification_message_t message = {.state = "DRY",
                                      .moisture_percent = 26,
                                      .temperature = 28.0f,
                                      .humidity = 77.0f,
                                      .environment_valid = true,
                                      .measured_at = (time_t)1792973754,
                                      .time_valid = true};
    if (strcmp(argv[2], "missing-dht") == 0)
        message.environment_valid = false;
    if (strcmp(argv[2], "unknown-clock") == 0)
        message.time_valid = false;
    break_write = strcmp(argv[2], "failed-write") == 0;
    esp_err_t result = notifications_send_gmail(&message);
    if (open_connections != 0) {
        fprintf(stderr, "TLS connection leaked\n");
        return 1;
    }
    if (break_write) {
        if (result == ESP_OK)
            return 1;
        puts("PASS Gmail failed-write cleanup");
        return 0;
    }
    if (result != ESP_OK) {
        fprintf(stderr, "SMTP simulation failed: %d\n", result);
        return 1;
    }
    FILE *file = fopen(argv[1], "wb");
    if (!file)
        return 2;
    fwrite(sent, 1, used, file);
    fclose(file);
    puts("PASS Gmail SMTP conversation with partial writes");
    return 0;
}
