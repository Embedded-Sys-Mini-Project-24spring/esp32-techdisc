#include <stdio.h>
#include "mpu-driver/mpu6050_driver.h"
#include "freertos/FreeRTOS.h"
#include "filter/smoothing_filter.h"

void app_main(void)
{
    mpu6050_init();

    while(1)
    {
        // This delay could probably go away
        vTaskDelay(100 / portTICK_PERIOD_MS); 
    }
}
