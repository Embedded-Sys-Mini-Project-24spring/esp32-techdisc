idf_component_register(SRCS "main.c" "mpu-driver/mpu6050_driver.c" "filter/smoothing_filter.c" "wifi-ws-server/wifi_ap_webserver.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_timer driver esp_http_server esp_wifi nvs_flash)
                    