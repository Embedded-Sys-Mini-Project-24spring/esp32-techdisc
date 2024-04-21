idf_component_register(SRCS "main.c" "mpu-driver/mpu6050_driver.c" "filter/smoothing_filter.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_timer driver)
                    