idf_component_register(
    SRCS "connectivity.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        freertos
        esp_hw_support
        espressif__network_provisioning
)