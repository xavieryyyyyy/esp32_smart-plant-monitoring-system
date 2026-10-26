# Build main.c and app_tasks.c. REQUIRES makes the components they use available.
idf_component_register(
    SRCS "main.c" "app_tasks.c"
    REQUIRES sensors monitoring connectivity notifications esp_timer
)