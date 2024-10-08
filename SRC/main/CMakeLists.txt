set(COMPONENT_SRCS "html.c" "calibration.c" "frsky.c" "error.c" "inputs.c" "wifi.c" "nvs_ecu.c" "main.c" "http_server.c" "jf-ecu32.c" "wifi.c" "nvs_ecu.c"
                     )
set(COMPONENT_ADD_INCLUDEDIRS "."
    EMBED_TXTFILES ota/index.html)


register_component()