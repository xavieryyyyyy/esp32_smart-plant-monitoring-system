/*
 * ESP-IDF starts our program by calling app_main().
 * This file starts the background jobs and the Wi-Fi setup.
 * The jobs keep running after app_main() finishes.
 */
#include "app_tasks.h"
/* Use the networking component to start Wi-Fi or ask whether Wi-Fi/time is ready. */
#include "connectivity.h"

/* ESP-IDF calls this application entry point; its background tasks continue after it returns. */
void app_main(void)
{
    /*
     * Start the jobs that read the sensors, check the readings, and send messages.
     * They can start before Wi-Fi connects.
     */
    app_tasks_start();
    /* Connect using saved Wi-Fi details, or let a phone provide them over Bluetooth. */
    connectivity_start();
}