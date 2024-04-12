#include <stdio.h>
#include "mpu-driver/mpu6050_driver.h"
#include "freertos/FreeRTOS.h"
#include "filter/smoothing_filter.h"
#include "driver/gpio.h"

void doubleToAscii( double* data, uint8_t size );

void app_main(void)
{
    gpio_config_t pushButtonPinConfigPin;
    pushButtonPinConfigPin.pin_bit_mask = 1 << 8; // Enable pin 8
    pushButtonPinConfigPin.mode = GPIO_MODE_OUTPUT;
    pushButtonPinConfigPin.pull_up_en = GPIO_PULLUP_DISABLE;
    pushButtonPinConfigPin.pull_down_en = GPIO_PULLDOWN_DISABLE;
    pushButtonPinConfigPin.intr_type = GPIO_INTR_DISABLE;

    mpu6050_init();

    while(1)
    {
        // This delay could probably go away
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void doubleToAscii(double* data, uint8_t size)
{
    
}