# Build sensors.c and make sensors.h available to other components.
# REQUIRES lists the ADC, GPIO, and DHT driver code that sensors.c uses.
# Register hardware acquisition separately from the monitoring rules.
idf_component_register(
    # Compile the ADC and DHT11 read/validation functions.
    SRCS "sensors.c"
    # Expose sensors.h and sensor_reading_t to the application tasks.
    INCLUDE_DIRS "include"
    # Provide the ESP-IDF ADC/GPIO drivers and installed DHT library.
    REQUIRES esp_adc esp_driver_gpio esp-idf-lib__dht
)