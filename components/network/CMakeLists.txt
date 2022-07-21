idf_component_register(SRCS "ethernet.c" "wifi_smartconfig.c"
                       INCLUDE_DIRS "." 
                       PRIV_INCLUDE_DIRS ""
                       PRIV_REQUIRES "driver"
                       REQUIRES "main")

