/*
 * Exercise the real Gmail sender with a fake SMTP conversation and partial writes.
 * The copied sender includes only dummy test configuration, never the private header.
 */
#include "host_transport.h"
#include "notifications.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <wincrypt.h>

static char sent[65536];
static size_t used;
static size_t reply_offset;
static int open_connections;
static bool break_write;
static const char replies[] =
    "220 ready\r\n250-hello\r\n250 auth\r\n334 user\r\n334 password\r\n"
    "235 authenticated\r\n250 sender\r\n250 recipient\r\n354 send data\r\n"
    "250 queued\r\n221 closing\r\n";

int esp_crt_bundle_attach(void *unused)
{
    (void)unused;
    return ESP_OK;
}
bool connectivity_is_ready(void)
{
    return true;
}
bool connectivity_time_is_ready(void)
{
    return true;
}
int64_t esp_timer_get_time(void)
{
    return 100;
}
void vTaskDelay(uint32_t ticks)
{
    (void)ticks;
}
esp_tls_t *esp_tls_init(void)
{
    esp_tls_t *tls = calloc(1, sizeof(*tls));
    if (tls)
        open_connections++;
    return tls;
}
int esp_tls_conn_new_sync(const char *host, int length, int port, const esp_tls_cfg_t *config,
                          esp_tls_t *tls)
{
    (void)length;
    (void)tls;
    return strcmp(host, "smtp.gmail.com") == 0 && port == 465 && config->timeout_ms == 10000;
}
ssize_t esp_tls_conn_write(esp_tls_t *tls, const void *bytes, size_t size)
{
    (void)tls;
    if (break_write && used > 250)
        return -1;
    /* Force the actual sender to handle writes that accept only part of a buffer. */
    if (size > 17)
        size = 17;
    if (used + size >= sizeof(sent))
        return -1;
    memcpy(sent + used, bytes, size);
    used += size;
    sent[used] = '\0';
    return (ssize_t)size;
}
ssize_t esp_tls_conn_read(esp_tls_t *tls, void *bytes, size_t size)
{
    (void)tls;
    (void)size;
    if (reply_offset >= sizeof(replies) - 1)
        return -1;
    *(char *)bytes = replies[reply_offset++];
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
