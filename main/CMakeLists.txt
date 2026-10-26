idf_component_register(
    SRCS "main.c" "ui.c"
    INCLUDE_DIRS "."
    REQUIRES sensor_manager display nvs_flash
)
