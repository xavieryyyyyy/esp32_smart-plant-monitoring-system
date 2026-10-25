idf_component_register(
    SRCS "sensors.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_adc esp_driver_gpio esp-idf-lib__dht
)