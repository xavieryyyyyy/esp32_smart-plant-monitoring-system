# Build main.c and app_tasks.c. REQUIRES makes the components they use available.
# Register the application source as an ESP-IDF component.
idf_component_register(
    # Compile the startup entry point and the file that creates/runs our three application jobs.
    SRCS "main.c" "app_tasks.c"
    # Make the sensor, monitoring, networking, sender, and uptime-timer components available to
    # these sources.
    REQUIRES sensors monitoring connectivity notifications esp_timer
)