idf_component_register(
    SRCS "sensor_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos
    PRIV_REQUIRES hx711 dht22 jsn_sr04t esp_timer
)
