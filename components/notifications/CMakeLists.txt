idf_component_register(
    SRCS "notifications.c" "discord.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client mbedtls connectivity esp_timer
)