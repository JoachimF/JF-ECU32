set(COMPONENT_SRCS "websocket.c" "imu.c" "simulator.c" "removed.c" "html.c" "calibration.c" "frsky.c" "error.c" "inputs.c" "wifi.c" "nvs_ecu.c" "main.c" "http_server.c" "jf-ecu32.c" "wifi.c" "nvs_ecu.c"
                     )
set(COMPONENT_ADD_INCLUDEDIRS "."
    EMBED_TXTFILES ota/index.html
    EMBED_FILES "upload_script.html"
    EMBED_FILES "logs_script.html")


register_component()