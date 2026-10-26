# Build connectivity.c and make connectivity.h available to other components.
# REQUIRES brings in Wi-Fi, saved settings, FreeRTOS, and Bluetooth phone setup.
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