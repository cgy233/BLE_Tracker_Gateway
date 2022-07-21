set(component_srcs
                   )

idf_component_register(SRCS "sntp_tools.c" "${component_srcs}"
                       INCLUDE_DIRS "." 
                       PRIV_INCLUDE_DIRS ""
                       REQUIRES "main")

