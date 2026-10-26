# Build all four senders. A new .c sender must be added to SRCS to be compiled.
# INCLUDE_DIRS exposes notifications.h; REQUIRES supplies networking and secure connections.
# Register the sender implementations and their shared formatter as one component.
idf_component_register(
    # Compile every listed source; omitting a sender/formatter here causes missing definitions at
    # link time.
    SRCS "notifications.c" "discord.c" "telegram.c" "gmail.c" "notification_format.c"
    # Expose the public notifications.h interface; keep notification_format.h private to this
    # component.
    INCLUDE_DIRS "include"
    # Provide HTTP/TLS/certificate, connectivity-readiness, and monotonic-timer APIs used by the
    # senders.
    REQUIRES esp_http_client esp-tls mbedtls connectivity esp_timer
)