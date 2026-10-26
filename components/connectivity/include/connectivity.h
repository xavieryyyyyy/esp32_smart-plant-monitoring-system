/*
 * Other files include this header to start Wi-Fi or check whether it is ready.
 * The actual work is in connectivity.c. SNTP is the service that sets Internet time.
 */
#pragma once

#include <stdbool.h>

/* Set up networking once. Use saved Wi-Fi details, or start Bluetooth phone setup. */
void connectivity_start(void);
/*
 * True means the router supplied an IP address. False also covers startup before setup.
 * An IP address alone does not prove that Internet services are reachable.
 */
bool connectivity_is_ready(void);
/*
 * True means Internet time was received at least once. This can stay true if Wi-Fi drops.
 */
bool connectivity_time_is_ready(void);