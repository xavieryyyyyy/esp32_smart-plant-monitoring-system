# Build monitoring.c and make monitoring.h available to the application and tests.
# This component checks numbers and states; it does not need sensor or Wi-Fi drivers.
idf_component_register(
    SRCS "monitoring.c"
    INCLUDE_DIRS "include"
)