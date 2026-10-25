#include "connectivity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "network_provisioning/manager.h"
#include "network_provisioning/scheme_ble.h"
#include "freertos/task.h"
#include <stdio.h>
#include "esp_mac.h"
#include "esp_random.h"

#define WIFI_READY_BIT BIT0
#define PROVISIONING_BIT BIT1

static EventGroupHandle_t connection_events;
static TaskHandle_t reconnect_task_handle;

bool connectivity_is_ready(void)
{
    if (connection_events == NULL) {
        return false;
    }

    return (xEventGroupGetBits(connection_events)
            & WIFI_READY_BIT) != 0;
}

static void reconnect_task(void *parameter)
{
    (void)parameter;
    uint32_t retry_delay_ms = 1000;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        EventBits_t bits = xEventGroupGetBits(connection_events);

        if (bits & WIFI_READY_BIT) {
            retry_delay_ms = 1000;
            continue;
        }

        if (bits & PROVISIONING_BIT) {
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));

        bits = xEventGroupGetBits(connection_events);
        if (bits & (WIFI_READY_BIT | PROVISIONING_BIT)) {
            continue;
        }

        ESP_LOGI("WIFI", "Attempting connection");
        esp_err_t result = esp_wifi_connect();

        if (retry_delay_ms < 30000) {
            retry_delay_ms *= 2;
            if (retry_delay_ms > 30000) {
                retry_delay_ms = 30000;
            }
        }

        if (result != ESP_OK) {
            ESP_LOGW("WIFI", "Connection request failed: %s",
                     esp_err_to_name(result));
            xTaskNotifyGive(reconnect_task_handle);
        }
    }
}


static void connection_event_handler(
    void *argument,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    (void)argument;
    (void)event_data;

    if (event_base == IP_EVENT &&
        event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(connection_events, WIFI_READY_BIT);
        xTaskNotifyGive(reconnect_task_handle);
        ESP_LOGI("WIFI", "Connected and obtained an IP address");

    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_START) {
        xTaskNotifyGive(reconnect_task_handle);

    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(connection_events, WIFI_READY_BIT);
        xTaskNotifyGive(reconnect_task_handle);
        ESP_LOGW("WIFI", "Disconnected");

    } else if (event_base == NETWORK_PROV_EVENT) {
        switch (event_id) {
        case NETWORK_PROV_START:
            xEventGroupSetBits(connection_events, PROVISIONING_BIT);
            ESP_LOGI("PROVISION", "Phone setup started");
            break;

        case NETWORK_PROV_WIFI_CRED_RECV:
            ESP_LOGI("PROVISION", "Wi-Fi credentials received");
            break;

        case NETWORK_PROV_WIFI_CRED_FAIL: {
            ESP_LOGW("PROVISION",
                     "Credentials failed; allowing another setup attempt");

            esp_err_t result =
                network_prov_mgr_reset_wifi_sm_state_on_failure();

            if (result != ESP_OK) {
                ESP_LOGE("PROVISION", "Setup reset failed: %s",
                         esp_err_to_name(result));
            }
            break;
        }

        case NETWORK_PROV_WIFI_CRED_SUCCESS:
            ESP_LOGI("PROVISION", "Wi-Fi credentials accepted");
            break;

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

void connectivity_start(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    connection_events = xEventGroupCreate();
    if (connection_events == NULL) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }

    if (esp_netif_create_default_wifi_sta() == NULL) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));

    BaseType_t task_result = xTaskCreate(
        reconnect_task, "wifi_reconnect", 4096, NULL, 3,
        &reconnect_task_handle);

    ESP_ERROR_CHECK(
        task_result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        connection_event_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP,
        connection_event_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(
        NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID,
        connection_event_handler, NULL));

    network_prov_mgr_config_t provisioning_config = {
        .scheme = network_prov_scheme_ble,
        .scheme_event_handler =
            NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
    };

    ESP_ERROR_CHECK(network_prov_mgr_init(provisioning_config));
   

    bool provisioned = false;
    ESP_ERROR_CHECK(
        network_prov_mgr_is_wifi_provisioned(&provisioned));

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

        snprintf(service_name, sizeof(service_name),
                 "PROV_PLANT_%02X%02X%02X",
                 mac[3], mac[4], mac[5]);

        snprintf(setup_code, sizeof(setup_code),
                 "%08lx%08lx",
                 (unsigned long)esp_random(),
                 (unsigned long)esp_random());

        xEventGroupSetBits(connection_events, PROVISIONING_BIT);

        ESP_ERROR_CHECK(network_prov_mgr_start_provisioning(
            NETWORK_PROV_SECURITY_1, setup_code,
            service_name, NULL));

        ESP_LOGI("PROVISION", "Device name: %s", service_name);
        ESP_LOGI("PROVISION", "Private setup code: %s", setup_code);
    }
}