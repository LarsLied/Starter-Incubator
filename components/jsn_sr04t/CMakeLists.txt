idf_component_register(
    SRCS "jsn_sr04t.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio
    PRIV_REQUIRES esp_timer
)
