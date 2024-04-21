#include <stdio.h>
#include "mpu-driver/mpu6050_driver.h"
#include "freertos/FreeRTOS.h"
#include "filter/smoothing_filter.h"
#include "wifi-ws-server/wifi_ap_webserver.h"
#include "driver/gpio.h"

void doubleToAscii( double* data, uint8_t size );

void app_main(void)
{
    gpio_config_t addrPinConfigPin;
    addrPinConfigPin.pin_bit_mask = 1 << 8; // Enable pin 8
    addrPinConfigPin.mode = GPIO_MODE_OUTPUT;
    addrPinConfigPin.pull_up_en = GPIO_PULLUP_DISABLE;
    addrPinConfigPin.pull_down_en = GPIO_PULLDOWN_DISABLE;
    addrPinConfigPin.intr_type = GPIO_INTR_DISABLE;

    esp_err_t error = gpio_config(&addrPinConfigPin);

    if(error == ESP_OK)
    {
        error = gpio_set_level(GPIO_NUM_8, 0);
        if(error == ESP_OK)
        {
            wifi_server_init();
            mpu6050_init();

            while(1)
            {
                // This delay could probably go away
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
        }
        
    }
}

void doubleToAscii(double* data, uint8_t size)
{
    
}