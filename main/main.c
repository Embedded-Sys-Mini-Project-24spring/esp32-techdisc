#include <stdio.h>
#include "mpu-driver/mpu6050_driver.h"
#include "freertos/FreeRTOS.h"
#include "filter/smoothing_filter.h"
#include <esp_http_server.h>
#include "wifi-ws-server/wifi_ap_webserver.h"

void app_main(void)
{
    wifi_server_init();
    mpu6050_init();

    while(1)
    {
        // This delay could probably go away
        vTaskDelay(100 / portTICK_PERIOD_MS); 
    }
}
