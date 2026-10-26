idf_component_register(
    SRCS "gc9a01.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_spi
    PRIV_REQUIRES esp_lcd
)
