# Build connectivity.c and make connectivity.h available to other components.
# REQUIRES brings in Wi-Fi, saved settings, FreeRTOS, and Bluetooth phone setup.
# Register the networking source and its public interface with the ESP-IDF build.
idf_component_register(
    # Compile this file, which manages Wi-Fi, phone setup, and Internet time.
    SRCS "connectivity.c"
    # Expose connectivity.h from this folder to components that require connectivity.
    INCLUDE_DIRS "include"
    # List the ESP-IDF/managed components whose headers or linked functions this code needs.
    REQUIRES
        # Provide Wi-Fi station control, connection events, and reconnect calls.
        esp_wifi
        # Provide the central event-loop registration/dispatch functions.
        esp_event
        # Provide network-interface setup and the SNTP wrapper.
        esp_netif
        # Provide initialization for persistent saved settings.
        nvs_flash
        # Provide tasks, task notifications, and event-group flags.
        freertos
        # Provide MAC-address/random-number hardware-support functions.
        esp_hw_support
        # Provide Espressif's managed BLE network-provisioning manager and scheme.
        espressif__network_provisioning
)