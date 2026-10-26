idf_component_register(
    SRCS "thermal_control.c"
    INCLUDE_DIRS "include"
    REQUIRES sensor_manager
    PRIV_REQUIRES peltier
)
