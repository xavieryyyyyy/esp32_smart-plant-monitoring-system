# Build sensors.c and make sensors.h available to other components.
# REQUIRES lists the ADC, GPIO, and DHT driver code that sensors.c uses.
idf_component_register(
    SRCS "sensors.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_adc esp_driver_gpio esp-idf-lib__dht
)