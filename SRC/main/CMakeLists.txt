set(COMPONENT_SRCS "logs.c" "outputs.c" "sdcard.c" "websocket.c" "imu.c" "simulator.c" "html.c" "calibration.c" "frsky.c" "error.c" "inputs.c" "wifi.c" "nvs_ecu.c" "main.c" "http_server.c" "jf-ecu32.c" "wifi.c" "nvs_ecu.c"
                     )
set(COMPONENT_ADD_INCLUDEDIRS "."
    EMBED_TXTFILES ota/index.html
    EMBED_FILES "upload_script.html"
    EMBED_FILES "logs_script.html"
    EMBED_FILES "favicon.ico")


register_component()