set(component_srcs "ping_baidu.c"
                   )

idf_component_register(SRCS "${component_srcs}"
                       INCLUDE_DIRS "." 
                       PRIV_INCLUDE_DIRS ""
                       PRIV_REQUIRES "driver"
                       REQUIRES "main")

