/* ESP-IDF/POSIX use gmtime_r; Windows UCRT provides the equivalent gmtime_s. */
#pragma once
/* Compile this compatibility helper only for Windows host tests, not ESP32 firmware. */
#ifdef _WIN32
/* Store Unix timestamps and convert them into calendar date/time fields. */
#include <time.h>
/* Adapt Windows gmtime_s to the gmtime_r calling convention used by ESP-IDF. */
static inline struct tm *plant_host_gmtime_r(const time_t *seconds, struct tm *result)
{
    /*
     * Translate Windows success (0) into the result pointer, or return NULL on conversion
     * failure.
     */
    return gmtime_s(result, seconds) == 0 ? result : NULL;
}
/* Use the Windows adapter for host builds; production still calls ESP-IDF gmtime_r. */
#define gmtime_r plant_host_gmtime_r
#endif
