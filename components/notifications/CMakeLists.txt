idf_component_register(
    SRCS "notifications.c" "discord.c" "telegram.c" "gmail.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client esp-tls mbedtls connectivity esp_timer
)