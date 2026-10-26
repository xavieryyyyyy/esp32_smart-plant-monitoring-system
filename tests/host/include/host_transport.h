/* Offline SMTP stand-ins. These declarations never open a real connection. */
#pragma once
/* Use ESP-IDF result codes: ESP_OK means success; other codes describe failures. */
#include "esp_err.h"
/* Use bool, true, and false for yes/no values. */
#include <stdbool.h>
/* Use explicit-width integers such as uint32_t and int64_t for counters and timers. */
#include <stdint.h>
/* Use size_t for byte counts and NULL for an absent pointer. */
#include <stddef.h>
/* Use ssize_t, a signed byte count that can also represent a read/write error. */
#include <sys/types.h>

/* Group related fields into one value so callers/queues keep the correct pieces together. */
typedef struct {
    /*
     * Provide a tiny placeholder type for a fake TLS object; this is not an ESP32 network
     * handle.
     */
    int open;
} esp_tls_t;
/* Group related fields into one value so callers/queues keep the correct pieces together. */
typedef struct {
    /*
     * Match the certificate-callback setting expected by the real sender; the host test supplies
     * a stub.
     */
    int (*crt_bundle_attach)(void *);
    /* Keep the configured timeout so the fake connection can verify it is ten seconds. */
    int timeout_ms;
} esp_tls_cfg_t;
/* Fake the retryable TLS code that means more incoming data is needed. */
#define ESP_TLS_ERR_SSL_WANT_READ (-0x6900)
/* Fake the retryable TLS code that means the connection needs to become writable. */
#define ESP_TLS_ERR_SSL_WANT_WRITE (-0x6880)
/* The fake host scheduler uses the supplied number directly; it does not wait in real time. */
#define pdMS_TO_TICKS(value) (value)
/* Silence informational ESP-IDF logging in this offline test build. */
#define ESP_LOGI(...) ((void)0)
/* Silence warning ESP-IDF logging in this offline test build. */
#define ESP_LOGW(...) ((void)0)
/* Silence error ESP-IDF logging in this offline test build. */
#define ESP_LOGE(...) ((void)0)

/*
 * Declare the host-only certificate callback; this simulation does not perform certificate
 * validation.
 */
int esp_crt_bundle_attach(void *unused);
/* Declare fake TLS-session allocation; no real socket is opened in this test. */
esp_tls_t *esp_tls_init(void);
/*
 * Declare/check the fake connection opening step; tests verify the host, port, and timeout
 * settings.
 */
int esp_tls_conn_new_sync(const char *host, int length, int port, const esp_tls_cfg_t *config,
                          esp_tls_t *tls);
/*
 * Declare the fake writer that captures only part of each write to exercise the real sender's
 * loop.
 */
ssize_t esp_tls_conn_write(esp_tls_t *tls, const void *bytes, size_t size);
/* Declare the fake reader that returns bytes from predefined SMTP replies. */
ssize_t esp_tls_conn_read(esp_tls_t *tls, void *bytes, size_t size);
/* Release the TLS connection object even if authentication or a later SMTP step failed. */
void esp_tls_conn_destroy(esp_tls_t *tls);
/* Declare a controlled test clock instead of using an ESP32 hardware timer. */
int64_t esp_timer_get_time(void);
/*
 * Pause this task and let other tasks run; pdMS_TO_TICKS converts milliseconds to scheduler
 * ticks.
 */
void vTaskDelay(uint32_t ticks);
/* Answer whether the router supplied an IP address; this does not test Internet reachability. */
bool connectivity_is_ready(void);
/* Answer whether SNTP has set the clock at least once since startup. */
bool connectivity_time_is_ready(void);
/*
 * Declare the host Base64 substitute used to test message construction, not real TLS
 * cryptography.
 */
int mbedtls_base64_encode(unsigned char *destination, size_t capacity, size_t *length,
                          const unsigned char *source, size_t size);
/* Overwrite temporary login bytes so that encoded credentials are not left in this buffer. */
void mbedtls_platform_zeroize(void *bytes, size_t size);
