/*
 * This file gets the ESP32 connected to Wi-Fi.
 * On first setup, a phone sends the Wi-Fi name and password over Bluetooth (BLE).
 * Later starts use the saved details. A separate job retries if Wi-Fi disconnects.
 * It also gets the current date/time from the Internet using SNTP.
 * Other files ask this file whether Wi-Fi and time are ready before sending.
 */
/*
 * Event-flag key:
 * Each BIT value selects one on/off flag. bits & FLAG checks only that flag.
 * FLAG_A | FLAG_B combines two flags so either one can prevent a reconnect attempt.
 * An event callback runs when ESP-IDF reports a change. Keep it short: wake the
 * reconnect task here and let that task handle the waiting/connection work.
 * NVS stores saved Wi-Fi settings in flash; SNTP learns wall-clock time from the
 * Internet. Neither is the monotonic uptime timer used for sample ages/cooldowns.
 */
#include "connectivity.h"
/* Use FreeRTOS types and constants for tasks, queues, and waiting. */
#include "freertos/FreeRTOS.h"
/* Store several shared on/off readiness flags safely in one event group. */
#include "freertos/event_groups.h"
/* Use ESP-IDF result codes: ESP_OK means success; other codes describe failures. */
#include "esp_err.h"
/* Receive ESP-IDF events such as a Wi-Fi disconnect or a newly assigned IP address. */
#include "esp_event.h"
/* Print tagged information, warnings, and errors in the serial monitor. */
#include "esp_log.h"
/* Create the network interface that joins a router as a Wi-Fi station. */
#include "esp_netif.h"
/* Configure, start, and reconnect the Wi-Fi radio. */
#include "esp_wifi.h"
/* Initialize nonvolatile storage, where Wi-Fi settings survive a restart. */
#include "nvs_flash.h"
/* Manage the phone setup process and its Wi-Fi credential events. */
#include "network_provisioning/manager.h"
/* Receive phone setup data over Bluetooth Low Energy (BLE). */
#include "network_provisioning/scheme_ble.h"
/* Create background jobs, pause them, or wake them with task notifications. */
#include "freertos/task.h"
/* Format bounded text with snprintf(), or use file/console I/O in host tests. */
#include <stdio.h>
/* Read the device MAC address to help make its phone-setup name. */
#include "esp_mac.h"
/* Generate random numbers for the temporary phone-setup code. */
#include "esp_random.h"
/* Store Unix timestamps and convert them into calendar date/time fields. */
#include <time.h>
/* Configure Internet time synchronization and its completion callback. */
#include "esp_netif_sntp.h"

/*
 * These are three on/off flags stored together in a FreeRTOS event group.
 * Each flag answers a different question; ready Wi-Fi does not mean ready time.
 */
/* On after the router gives this ESP32 an IP address. */
#define WIFI_READY_BIT BIT0
/* On while the phone is setting up Wi-Fi. */
#define PROVISIONING_BIT BIT1
/* On after Internet time has been received at least once. */
#define TIME_READY_BIT BIT2

/* Handle for the shared Wi-Fi/setup/time flags; each bit answers one readiness question. */
static EventGroupHandle_t connection_events;
/* Remember the reconnect job so an event handler can wake that specific job. */
static TaskHandle_t reconnect_task_handle;

/* Other files call this to check Wi-Fi readiness before trying a send. */
bool connectivity_is_ready(void)
{
    /* Check for an absent pointer or failed allocation before dereferencing/using it. */
    if (connection_events == NULL) {
        /* Return no/false: this check did not authorize a change, output, or action. */
        return false;
    }

    /*
     * Read the shared bits, isolate the Wi-Fi flag with &, and answer true only if that bit is
     * on.
     */
    return (xEventGroupGetBits(connection_events) & WIFI_READY_BIT) != 0;
}

/*
 * Other files call this to check whether the clock has been set. Wi-Fi can still be offline.
 */
bool connectivity_time_is_ready(void)
{
    /* Check for an absent pointer or failed allocation before dereferencing/using it. */
    if (connection_events == NULL) {
        /* Return no/false: this check did not authorize a change, output, or action. */
        return false;
    }

    /*
     * Read the shared bits, isolate the time flag with &, and answer whether SNTP has
     * synchronized once.
     */
    return (xEventGroupGetBits(connection_events) & TIME_READY_BIT) != 0;
}

/*
 * This background job waits and tries to reconnect when it is notified.
 * Its waiting happens here so it does not hold up the network event handler.
 * After failures, the retry wait grows from one second up to thirty seconds.
 */
static void reconnect_task(void *parameter)
{
    /*
     * This callback/task must accept the parameter, but this implementation intentionally does
     * not use it.
     */
    (void)parameter;
    /* Start Wi-Fi retries with a 1000 ms wait; later failures may increase that delay. */
    uint32_t retry_delay_ms = 1000;

    /*
     * Keep this task/read loop running; its wait, break, or error path below decides when to
     * proceed.
     */
    while (1) {
        /* Wait until another part of this file asks the reconnect job to do something. */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /*
         * Take a snapshot of the shared readiness flags before deciding whether a retry is
         * needed.
         */
        EventBits_t bits = xEventGroupGetBits(connection_events);

        /*
         * Already connected? Reset the next retry wait to one second and wait for another event.
         */
        if (bits & WIFI_READY_BIT) {
            /*
             * Reset/cap the Wi-Fi retry wait in milliseconds according to the surrounding
             * branch.
             */
            retry_delay_ms = 1000;
            /* Skip the rest of this loop iteration and return to its next receive/retry step. */
            continue;
        }

        /*
         * During phone setup, let Espressif's setup code handle the connection attempts.
         */
        if (bits & PROVISIONING_BIT) {
            /* Skip the rest of this loop iteration and return to its next receive/retry step. */
            continue;
        }

        /*
         * Pause this task and let other tasks run; pdMS_TO_TICKS converts milliseconds to
         * scheduler ticks.
         */
        vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));

        /*
         * Check again after waiting: Wi-Fi or phone setup may have changed while we slept.
         */
        bits = xEventGroupGetBits(connection_events);
        /*
         * Inspect the selected event-group bits before deciding whether Wi-Fi retry work is
         * allowed.
         */
        if (bits & (WIFI_READY_BIT | PROVISIONING_BIT)) {
            /* Skip the rest of this loop iteration and return to its next receive/retry step. */
            continue;
        }

        /* Print an informational progress/result line with a tag for the serial monitor. */
        ESP_LOGI("WIFI", "Attempting connection");
        /* Keep this operation's success/error code so later steps can stop safely on failure. */
        esp_err_t result = esp_wifi_connect();

        /* Double the wait for the next try, but stop increasing it at thirty seconds. */
        if (retry_delay_ms < 30000) {
            /* Double the next retry delay after this attempt, up to the cap checked below. */
            retry_delay_ms *= 2;
            /* Increase the retry wait only until its thirty-second maximum. */
            if (retry_delay_ms > 30000) {
                /*
                 * Reset/cap the Wi-Fi retry wait in milliseconds according to the surrounding
                 * branch.
                 */
                retry_delay_ms = 30000;
            }
        }

        /* Handle/return the earlier failure instead of continuing with invalid output. */
        if (result != ESP_OK) {
            /* Print a warning about a recoverable problem or skipped operation. */
            ESP_LOGW("WIFI", "Connection request failed: %s", esp_err_to_name(result));
            /* Wake the reconnect task; let that task perform any delayed connection work. */
            xTaskNotifyGive(reconnect_task_handle);
        }
    }
}

/*
 * ESP-IDF calls this function when a network or phone-setup event happens.
 * Update our ready flags and wake the reconnect job when needed.
 * The received Wi-Fi password is not printed.
 */
static void connection_event_handler(void *argument, esp_event_base_t event_base, int32_t event_id,
                                     void *event_data)
{
    /*
     * This callback/task must accept the parameter, but this implementation intentionally does
     * not use it.
     */
    (void)argument;
    /*
     * This callback/task must accept the parameter, but this implementation intentionally does
     * not use it.
     */
    (void)event_data;

    /*
     * The router gave us an IP address. Mark Wi-Fi ready and start the Internet clock service.
     */
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        /* Turn on the named shared readiness/setup flag without changing the other flags. */
        xEventGroupSetBits(connection_events, WIFI_READY_BIT);
        /* Wake the reconnect task; let that task perform any delayed connection work. */
        xTaskNotifyGive(reconnect_task_handle);

        /* Keep this operation's success/error code so later steps can stop safely on failure. */
        esp_err_t time_result = esp_netif_sntp_start();

        /* Handle/return the earlier failure instead of continuing with invalid output. */
        if (time_result != ESP_OK) {
            /* Print a warning about a recoverable problem or skipped operation. */
            ESP_LOGW("TIME", "Time service start failed: %s", esp_err_to_name(time_result));
        }

        /* Identify which ESP-IDF Wi-Fi/IP/provisioning event caused this callback. */
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        /* The Wi-Fi system has started. Ask the reconnect job to make the connection. */
        xTaskNotifyGive(reconnect_task_handle);

        /* Identify which ESP-IDF Wi-Fi/IP/provisioning event caused this callback. */
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        /* Wi-Fi was lost. Mark it unavailable and ask the reconnect job to try again. */
        xEventGroupClearBits(connection_events, WIFI_READY_BIT);
        /* Wake the reconnect task; let that task perform any delayed connection work. */
        xTaskNotifyGive(reconnect_task_handle);
        /* Print a warning about a recoverable problem or skipped operation. */
        ESP_LOGW("WIFI", "Disconnected");

        /* Identify which ESP-IDF Wi-Fi/IP/provisioning event caused this callback. */
    } else if (event_base == NETWORK_PROV_EVENT) {
        /* Handle the individual BLE-provisioning event reported by Espressif's manager. */
        switch (event_id) {
            /* The phone setup has begun. Keep the normal reconnect job out of its way. */
            case NETWORK_PROV_START:
                /*
                 * Turn on the named shared readiness/setup flag without changing the other
                 * flags.
                 */
                xEventGroupSetBits(connection_events, PROVISIONING_BIT);
                /* Print an informational progress/result line with a tag for the serial monitor. */
                ESP_LOGI("PROVISION", "Phone setup started");
                /*
                 * Leave the nearest loop or switch; do not execute its remaining steps for this
                 * item.
                 */
                break;

            /*
             * The phone supplied Wi-Fi details. Receiving them does not yet prove they work.
             */
            case NETWORK_PROV_WIFI_CRED_RECV:
                /* Print an informational progress/result line with a tag for the serial monitor. */
                ESP_LOGI("PROVISION", "Wi-Fi credentials received");
                break;

            /* The supplied Wi-Fi details did not work. Allow another phone setup attempt. */
            case NETWORK_PROV_WIFI_CRED_FAIL: {
                ESP_LOGW("PROVISION", "Credentials failed; allowing another setup attempt");

                esp_err_t result = network_prov_mgr_reset_wifi_sm_state_on_failure();

                if (result != ESP_OK) {
                    ESP_LOGE("PROVISION", "Setup reset failed: %s", esp_err_to_name(result));
                }
                break;
            }

            /* The supplied Wi-Fi details worked. */
            case NETWORK_PROV_WIFI_CRED_SUCCESS:
                ESP_LOGI("PROVISION", "Wi-Fi credentials accepted");
                break;

            /*
             * Phone setup is finished. Free its resources and return to normal Wi-Fi handling.
             */
            case NETWORK_PROV_END:
                network_prov_mgr_deinit();
                xEventGroupClearBits(connection_events, PROVISIONING_BIT);
                xTaskNotifyGive(reconnect_task_handle);
                ESP_LOGI("PROVISION", "Phone setup finished");
                break;

            default:
                break;
        }
    }
}

/*
 * SNTP calls this function after it sets the ESP32's date and time.
 * Make that time readable for the log, then mark the clock as ready.
 * Sample-age checks use the separate timer that counts time since startup.
 */
static void time_sync_callback(struct timeval *value)
{
    struct tm utc;
    char timestamp[32];

    gmtime_r(&value->tv_sec, &utc);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S UTC", &utc);

    ESP_LOGI("TIME", "Clock synchronized: %s", timestamp);
    xEventGroupSetBits(connection_events, TIME_READY_BIT);
}

/* main.c calls this once to prepare Wi-Fi, the clock service, and phone setup. */
void connectivity_start(void)
{
    /*
     * Prepare saved settings and the ESP-IDF network system. ESP_ERROR_CHECK stops on failure.
     */
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_sntp_config_t time_config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");

    /* Prepare the clock service now, but wait for an IP address before starting it. */
    time_config.start = false;
    time_config.sync_cb = time_sync_callback;

    ESP_ERROR_CHECK(esp_netif_sntp_init(&time_config));

    /* Create the group that stores our three ready/setup flags. */
    connection_events = xEventGroupCreate();
    if (connection_events == NULL) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }

    if (esp_netif_create_default_wifi_sta() == NULL) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));

    /* Start the background Wi-Fi retry job and keep its handle so we can wake it later. */
    BaseType_t task_result =
        xTaskCreate(reconnect_task, "wifi_reconnect", 4096, NULL, 3, &reconnect_task_handle);

    ESP_ERROR_CHECK(task_result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    /* Tell ESP-IDF to report Wi-Fi, IP-address, and phone-setup events to our handler. */
    ESP_ERROR_CHECK(
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, connection_event_handler, NULL));

    ESP_ERROR_CHECK(
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, connection_event_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID,
                                               connection_event_handler, NULL));

    /* Use Bluetooth for phone setup. Release its Bluetooth memory when setup finishes. */
    network_prov_mgr_config_t provisioning_config = {
        .scheme = network_prov_scheme_ble,
        .scheme_event_handler = NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
    };

    ESP_ERROR_CHECK(network_prov_mgr_init(provisioning_config));

    bool provisioned = false;
    ESP_ERROR_CHECK(network_prov_mgr_is_wifi_provisioned(&provisioned));

    /*
     * Wi-Fi details are saved in NVS, storage that survives a restart.
     * If saved details exist, connect with them and skip phone setup.
     * There is no user command yet for replacing those saved details.
     */
    if (provisioned) {
        network_prov_mgr_deinit();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI("WIFI", "Using saved Wi-Fi credentials");
    } else {
        static char service_name[24];
        static char setup_code[17];
        uint8_t mac[6];

        ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));

        /*
         * Build the device name shown in the phone app, using the last three MAC-address bytes.
         */
        snprintf(service_name, sizeof(service_name), "PROV_PLANT_%02X%02X%02X", mac[3], mac[4],
                 mac[5]);

        /*
         * Make a random setup code that the phone must provide during setup.
         * It is printed on the local serial monitor; keep it out of public screenshots.
         */
        snprintf(setup_code, sizeof(setup_code), "%08lx%08lx", (unsigned long)esp_random(),
                 (unsigned long)esp_random());

        xEventGroupSetBits(connection_events, PROVISIONING_BIT);

        ESP_ERROR_CHECK(network_prov_mgr_start_provisioning(NETWORK_PROV_SECURITY_1, setup_code,
                                                            service_name, NULL));

        ESP_LOGI("PROVISION", "Device name: %s", service_name);
        ESP_LOGI("PROVISION", "Private setup code: %s", setup_code);
    }
}
