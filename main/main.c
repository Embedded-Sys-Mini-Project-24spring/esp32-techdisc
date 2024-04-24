#include <stdio.h>
#include "mpu-driver/mpu6050_driver.h"
#include "freertos/FreeRTOS.h"
#include "filter/smoothing_filter.h"
#include "wifi-ws-server/wifi_ap_webserver.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define dataStringSize 512
char dataString[dataStringSize];

// Handle for the timer that is used to send data to the client app
esp_timer_handle_t clientAppSenderTimerHandle;

void timerCallbackAppDataGathering(void* arg);

void app_main(void)
{
    gpio_config_t addrPinConfigPin;
    addrPinConfigPin.pin_bit_mask = 1 << 8; // Enable pin 8
    addrPinConfigPin.mode = GPIO_MODE_OUTPUT;
    addrPinConfigPin.pull_up_en = GPIO_PULLUP_DISABLE;
    addrPinConfigPin.pull_down_en = GPIO_PULLDOWN_DISABLE;
    addrPinConfigPin.intr_type = GPIO_INTR_DISABLE;

    esp_err_t error = gpio_config(&addrPinConfigPin);

    printf("Configuring client app timer for periodic measurements.\n");
    esp_timer_create_args_t clientAppSenderTimerArgs;
    clientAppSenderTimerArgs.name = "Client app data sender";
    clientAppSenderTimerArgs.dispatch_method = ESP_TIMER_TASK;
    clientAppSenderTimerArgs.callback = timerCallbackAppDataGathering;
    esp_timer_create( &clientAppSenderTimerArgs, &clientAppSenderTimerHandle );

    if(error == ESP_OK)
    {
        error = gpio_set_level(GPIO_NUM_8, 0);
        if(error == ESP_OK)
        {
            wifi_server_init();
            i2c_init();
            mpu6050_init();
            
            // Should configure the timer to expire every 150ms
            esp_timer_start_periodic( clientAppSenderTimerHandle, 150000 );

            while(1)
            {
                // This delay could probably go away
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
        }
        
    }
}

void timerCallbackAppDataGathering(void* arg)
{
    (void)mpu6050_get_value_string( dataString , dataStringSize);
    printf("%s", dataString);
    (void)queue_send(dataString);
}
