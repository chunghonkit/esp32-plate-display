idf_component_register(SRCS "main.c" "display.c" "mqtt_handler.c" "ui.c" "xl9555.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_lcd esp_wifi esp_event nvs_flash mqtt esp_netif esp_timer lvgl esp_http_server)
