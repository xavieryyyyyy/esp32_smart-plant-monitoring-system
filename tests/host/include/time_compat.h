/* ESP-IDF/POSIX use gmtime_r; Windows UCRT provides the equivalent gmtime_s. */
#pragma once
#ifdef _WIN32
#include <time.h>
static inline struct tm *plant_host_gmtime_r(const time_t *seconds, struct tm *result)
{
    return gmtime_s(result, seconds) == 0 ? result : NULL;
}
#define gmtime_r plant_host_gmtime_r
#endif
