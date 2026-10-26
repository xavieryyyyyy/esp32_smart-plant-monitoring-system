/* Only the ESP-IDF result constants needed by the offline formatting tests. */
#pragma once
/* Match the ESP-IDF result-code type in this host-only stand-in. */
typedef int esp_err_t;
/* Finish successfully; the caller may now use this function's output. */
#define ESP_OK 0
/* Report a failed operation; do not treat this path as successful delivery. */
#define ESP_FAIL (-1)
/* Report that temporary storage could not be allocated; the caller must handle the failure. */
#define ESP_ERR_NO_MEM 0x101
/* Reject an absent, unsafe, or out-of-range input before using it. */
#define ESP_ERR_INVALID_ARG 0x102
/* Stop because required setup/readiness is missing or sending is deliberately paused. */
#define ESP_ERR_INVALID_STATE 0x103
/* Report that the available buffer/format limit could not safely hold this data. */
#define ESP_ERR_INVALID_SIZE 0x104
/* Report that this operation exceeded its allowed wait. */
#define ESP_ERR_TIMEOUT 0x107
/* Reject a sensor value that the driver returned but our plausibility check disallows. */
#define ESP_ERR_INVALID_RESPONSE 0x108
