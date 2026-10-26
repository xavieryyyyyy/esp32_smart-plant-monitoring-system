# Build monitoring.c and make monitoring.h available to the application and tests.
# This component checks numbers and states; it does not need sensor or Wi-Fi drivers.
# Register the pure monitoring rules as their own component.
idf_component_register(
    # Compile the conversion, classification, confirmation, and cooldown implementation.
    SRCS "monitoring.c"
    # Expose monitoring.h for the application and Unity tests.
    INCLUDE_DIRS "include"
)