/* Offline SMTP stand-ins. These declarations never open a real connection. */
#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

typedef struct {
    int open;
} esp_tls_t;
typedef struct {
    int (*crt_bundle_attach)(void *);
    int timeout_ms;
} esp_tls_cfg_t;
#define ESP_TLS_ERR_SSL_WANT_READ (-0x6900)
#define ESP_TLS_ERR_SSL_WANT_WRITE (-0x6880)
#define pdMS_TO_TICKS(value) (value)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)

int esp_crt_bundle_attach(void *unused);
esp_tls_t *esp_tls_init(void);
int esp_tls_conn_new_sync(const char *host, int length, int port, const esp_tls_cfg_t *config,
                          esp_tls_t *tls);
ssize_t esp_tls_conn_write(esp_tls_t *tls, const void *bytes, size_t size);
ssize_t esp_tls_conn_read(esp_tls_t *tls, void *bytes, size_t size);
void esp_tls_conn_destroy(esp_tls_t *tls);
int64_t esp_timer_get_time(void);
void vTaskDelay(uint32_t ticks);
bool connectivity_is_ready(void);
bool connectivity_time_is_ready(void);
int mbedtls_base64_encode(unsigned char *destination, size_t capacity, size_t *length,
                          const unsigned char *source, size_t size);
void mbedtls_platform_zeroize(void *bytes, size_t size);
