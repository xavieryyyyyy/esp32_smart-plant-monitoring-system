# Build all four senders. A new .c sender must be added to SRCS to be compiled.
# INCLUDE_DIRS exposes notifications.h; REQUIRES supplies networking and secure connections.
idf_component_register(
    SRCS "notifications.c" "discord.c" "telegram.c" "gmail.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client esp-tls mbedtls connectivity esp_timer
)