#include "mpu6050_driver.h"
#include "esp_timer.h"
#include "filter/smoothing_filter.h"
#include "semaphore.h"
#include "math.h"
#include "sys/types.h"
#include <stdio.h>
#include "wifi-ws-server/wifi_ap_webserver.h"

#undef DEBUG // use undef to remove print statements

#define IMU_DATA_SIZE_BYTES 14
#define BUFFER_SAMPLES 5
#define OFFSET_LOOP 200
#define ACCEL_THRESHOLD .01
#define GYRO_THRESHOLD 4
#define GYRO_THRESHOLD_XY 4
#define GYRO_LSB_SENSITIVITY  16.4 // Pulled from MPU6050 datasheet
#define ACCEL_LSB_SENSITIVITY 16384 // Pulled from MPU6050 datasheet
#define S_TO_MS 1000 // Conversion to MS
#define NUM_DATA_TO_RETURN 7
#define GAIN .85

// Handle for the timer that is used to sample the IMU data
esp_timer_handle_t i2cTimerHandle;

// Buffer to store I2C read data from the IMU
uint8_t imu_data[IMU_DATA_SIZE_BYTES];

// Buffers to hold 5 samples of data from IMU in order to do
// the filtering
double accel_x_buffer[BUFFER_SAMPLES] = {};
double accel_y_buffer[BUFFER_SAMPLES] ={};
double accel_z_buffer[BUFFER_SAMPLES] = {};
double gyro_x_buffer[BUFFER_SAMPLES] = {};
double gyro_y_buffer[BUFFER_SAMPLES] = {};
double gyro_z_buffer[BUFFER_SAMPLES] = {};
double temp_data_buffer[BUFFER_SAMPLES] = {};

// Offset values to account for initial bias
int16_t gyro_x_offset = 0;
int16_t gyro_y_offset = 0;
int16_t gyro_z_offset = 0;

// The final acceleration values that can be used for
// calculations
double accel_x_output;
double accel_y_output;
double accel_z_output;

double accel_x_scale_factor = ACCEL_LSB_SENSITIVITY;
double accel_y_scale_factor = ACCEL_LSB_SENSITIVITY;
double accel_z_scale_factor;

// Final output acceleration values that can be used in
// calculations
double gyro_x_output;
double gyro_y_output;
double gyro_z_output;
double temp_data_output;
uint count = 0;

double rpm = 0;

double currentAngleYGyro = 0;
double currentAngleXGyro = 0;
double currentAngleYAccel = 0;
double currentAngleXAccel = 0;
double finalAngleX = 0;
double finalAngleY = 0;

volatile bool dataLock = false;

void timerCallbackRawDataGathering(void* arg);

static const uint8_t mpu6050_init_cmd[11][2] = {
    //{MPU6050_RA_PWR_MGMT_1, 0x80}, // PWR_MGMT_1, DEVICE_RESET  
    // need wait 
    {MPU6050_RA_PWR_MGMT_1, 0x00}, // cleat SLEEP
    {MPU6050_RA_GYRO_CONFIG, 0x18}, // Gyroscope Full Scale Range = ± 2000 °/s
    {MPU6050_RA_ACCEL_CONFIG, 0x00}, // Accelerometer Full Scale Range = ± 2g 
    {MPU6050_RA_INT_ENABLE, 0x00}, // Interrupt Enable.disenable 
    {MPU6050_RA_USER_CTRL, 0x00}, // User Control.auxiliary I2C are logically driven by the primary I2C bus
    {MPU6050_RA_FIFO_EN, 0x00}, // FIFO Enable.disenable  
    {MPU6050_RA_SMPLRT_DIV, 0x63}, // Sample Rate Divider.Sample Rate = 1KHz / (1 + 99)  
    {MPU6050_RA_CONFIG, 0x0B}, // 0x13 EXT_SYNC_SET = GYRO_XOUT_L[0]; Bandwidth = 3
    {MPU6050_RA_PWR_MGMT_1, 0x01}, // Power Management 1.PLL with X axis gyroscope reference
    {MPU6050_RA_PWR_MGMT_2, 0x00}  // Power Management 2
};

static esp_err_t  i2c_master_read_slave(i2c_port_t i2c_num, uint8_t reg_addr, uint8_t *data_rd, size_t size)
{
    if (size == 0) {
        return ESP_OK;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | READ_BIT, ACK_CHECK_EN);
    if (size > 1) {
        i2c_master_read(cmd, data_rd, size - 1, ACK_VAL);
    }
    i2c_master_read_byte(cmd, data_rd + size - 1, NACK_VAL);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t i2c_master_write_slave(i2c_port_t i2c_num, uint8_t *data_wr, size_t size)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write(cmd, data_wr, size, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}
/**
 * @brief Reset function to reset the angle of the x & y access
*/
void reset()
{
    while(dataLock == true)
    {}
    dataLock = true;
    currentAngleYGyro = 0;
    currentAngleXGyro = 0;
    dataLock = false;
}

esp_err_t i2c_init() {
    esp_err_t esp_err;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = MPU6050_I2C_SDA,         // select GPIO specific to your project
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = MPU6050_I2C_SCL,         // select GPIO specific to your project
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = MPU6050_I2C_FREQ,  // select frequency specific to your project
        // .clk_flags = 0,          /*!< Optional, you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here. */
    };
    esp_err = i2c_param_config(MPU6050_I2C_PORT_NUM, &conf);
    printf("i2c_param_config: %d \n", esp_err);

    esp_err = i2c_driver_install(MPU6050_I2C_PORT_NUM, I2C_MODE_MASTER, 0, 0, 0);
    printf("i2c_driver_install: %d \n", esp_err);

    // Set up the reset commad for the IMU
    uint8_t reset_command[1][2] = {{MPU6050_RA_PWR_MGMT_1, 0x80}};
    esp_err = i2c_master_write_slave(MPU6050_I2C_PORT_NUM, reset_command, 2);
    return esp_err;
}

/**
* @brief Initialize the mpu6050 and obtain starting offset and configuration values
*/
esp_err_t mpu6050_init()
{

    esp_err_t esp_err = ESP_OK;
    // The datasheet says to wait until the reset register is cleared. But just to make it easy
    // I just used a delay to give some time before moving on
    vTaskDelay(200 / portTICK_PERIOD_MS);

    for (size_t i = 0; i < 11 && esp_err == ESP_OK; i++)
    {
        esp_err = i2c_master_write_slave(MPU6050_I2C_PORT_NUM, mpu6050_init_cmd[i], 2);
        if (i == 0)
            vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    printf("mpu6050_init_cmd: %d \n", esp_err);

    // Delay following the initial commands to give time to settle
    vTaskDelay(200 / portTICK_PERIOD_MS);

    // Following settup we should find the offset
    int64_t gyro_x_offset_temp = 0;
    int64_t gyro_y_offset_temp = 0;
    int64_t gyro_z_offset_temp = 0;

    queue_send("Please hold IMU still while calibrating gyros...\n");

    // Delay to give user time to orient imu and be steady
    vTaskDelay(3000 / portTICK_PERIOD_MS);


    // Get a bunch of gyro samples and take the average whle holding everything stil in order find the
    // initial bias of the gyros
    for( unsigned int i = 0; i < OFFSET_LOOP && esp_err == ESP_OK; ++i )
    {
        esp_err = i2c_master_read_slave( MPU6050_I2C_PORT_NUM, MPU6050_RA_GYRO_XOUT_H, imu_data, 6 );
        
        if( esp_err == ESP_OK )
        {
            gyro_x_offset_temp += (int16_t)(imu_data[0]<<8 | imu_data[1]);
            gyro_y_offset_temp += (int16_t)(imu_data[2]<<8 | imu_data[3]);
            gyro_z_offset_temp += (int16_t)(imu_data[4]<<8 | imu_data[5]);
        }
    }

    gyro_x_offset = (int16_t)(gyro_x_offset_temp/OFFSET_LOOP);
    gyro_y_offset = (int16_t)(gyro_y_offset_temp/OFFSET_LOOP);
    gyro_z_offset = (int16_t)(gyro_z_offset_temp/OFFSET_LOOP);

    // Calibrate the z axis for the accelerometer
    queue_send("Please place z-axis of IMU upwards and hold steady...\n");

    // Delay to give user time to orient imu and be steady
    vTaskDelay(3000 / portTICK_PERIOD_MS);

    // Grab a bunch of samples that can be averaged and used for the returned data in this orientation
    int64_t temp_accel_z_data_upwards = 0;
    for( unsigned int i = 0; i < OFFSET_LOOP && esp_err == ESP_OK; ++i )
    {
        esp_err = i2c_master_read_slave( MPU6050_I2C_PORT_NUM, MPU6050_RA_ACCEL_ZOUT_H, imu_data, 2 );
        
        if( esp_err == ESP_OK )
        {
            temp_accel_z_data_upwards += (int16_t)(imu_data[0]<<8 | imu_data[1]);
        }
    }
    temp_accel_z_data_upwards = temp_accel_z_data_upwards/OFFSET_LOOP;

    queue_send("Please place z-axis of IMU downwards and hold steady...\n");

    // Delay to give user time to orient imu and be steady
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    // Grab a bunch of samples that can be averaged and used for the returned data in this orientation
    int64_t temp_accel_z_data_downwards = 0;
    for( unsigned int i = 0; i < OFFSET_LOOP && esp_err == ESP_OK; ++i )
    {
        esp_err = i2c_master_read_slave( MPU6050_I2C_PORT_NUM, MPU6050_RA_ACCEL_ZOUT_H, imu_data, 2 );
        
        if( esp_err == ESP_OK )
        {
            temp_accel_z_data_downwards += (int16_t)(imu_data[0]<<8 | imu_data[1]);
        }
    }
    temp_accel_z_data_downwards = temp_accel_z_data_downwards/OFFSET_LOOP;

    // With a perfect device the two positions that the IMU was held in would return a value of +/-1g for
    // the acceleration in the z-axis. Of course nothing is perfect and there is inherent error in the
    // device. To account for this we can calculate the actual range of values (or scaling factor) for our
    // device. Since the difference betwen the two positions is 2g we can divide that by the distance between
    // the two read values to obtain that scalling factor. NOTE: I used difference/2 in this case to avoid a
    // decimal number. This value can be divided by our read values in the future to obtain the same result.
    accel_z_scale_factor = (double)((int16_t)((temp_accel_z_data_downwards-temp_accel_z_data_upwards)/2));

    // Get rid of any negative sign
    accel_z_scale_factor = (accel_z_scale_factor < 0) ? (-1*accel_z_scale_factor) : accel_z_scale_factor;

    queue_send("Return to upright position.\n");

    // Delay to give user time to orient imu and be steady
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    printf("Configuring timer for periodic measurements.\n");
    esp_timer_create_args_t i2cTimerArgs;
    i2cTimerArgs.name = "I2C Data Collection Timer";
    i2cTimerArgs.dispatch_method = ESP_TIMER_TASK;
    i2cTimerArgs.callback = timerCallbackRawDataGathering;
    esp_timer_create( &i2cTimerArgs, &i2cTimerHandle );

    // Should configure the timer to expire every 100ms
    esp_timer_start_periodic( i2cTimerHandle, 100000 );

    return esp_err;
}

/**
* @brief Returns the rpm, acceleration, and board x & y tilt in degrees
*/
bool mpu6050_get_value_double( double* data, uint8_t size )
{
    bool retStatus = false;
    if( size >= NUM_DATA_TO_RETURN )
    {
        uint8_t index = 0;
        while(dataLock == true)
        {}
        dataLock = true;
        data[index++] = accel_x_output;
        data[index++] = accel_y_output;
        data[index++] = accel_z_output;
        data[index++] = finalAngleX;
        data[index++] = finalAngleY;
        data[index++] = temp_data_output;
        data[index++] = rpm;
        dataLock = false;
        retStatus = true;
    }

    return retStatus;
}

/**
* @brief Returns the rpm, acceleration, and board x & y tilt in degrees
*/
bool mpu6050_get_value_int( int32_t* data, uint8_t size )
{
    bool retStatus = false;
    if( size >= NUM_DATA_TO_RETURN )
    {
        uint8_t index = 0;
        while(dataLock == true)
        {}
        dataLock = true;
        data[index++] = (int32_t)accel_x_output;
        data[index++] = (int32_t)accel_y_output;
        data[index++] = (int32_t)accel_z_output;
        data[index++] = (int32_t)finalAngleX;
        data[index++] = (int32_t)finalAngleY;
        data[index++] = (int32_t)temp_data_output;
        data[index++] = (int32_t)rpm;
        dataLock = false;
        retStatus = true;
    }

    return retStatus;
}

/**
* @brief Returns the rpm, acceleration, and board x & y tilt in degrees
*/
bool mpu6050_get_value_string(char* data, uint16_t size)
{
    bool retStatus = false;
    while(dataLock == true)
    {};

    dataLock = true;

    snprintf( data, 
              size,
              "accel_xout:%lf\t;accel_yout:%lf\t;accel_zout:%lf\t;gyro_xout:%lf\t;gyro_yout:%lf\t;gyro_zout:%lf\t;temp:%lf\t;cnt:%d\t;angle_x:%lf\t;angle_y:%lf\t;rpm:%lf\n",
              accel_x_output,
              accel_y_output,
              accel_z_output,
              gyro_x_output,
              gyro_y_output,
              gyro_z_output,
              temp_data_output,
              count++,
              finalAngleX,
              finalAngleY,
              rpm);

    dataLock = false;
    retStatus = true;

    return retStatus;
}

/**
 * @brief Timer callback that will handle reading data from the mpu6050
 * and then calculating usable data such as rpm and current x & y axis
 * angle.
*/
void timerCallbackRawDataGathering(void* arg)
{
    // Read the accelerations and gyro data from the IMU
    esp_err_t status = i2c_master_read_slave( MPU6050_I2C_PORT_NUM, MPU6050_RA_ACCEL_XOUT_H, imu_data, 14 );

    if( status == ESP_OK )
    {
        // Shift the data down in the buffer
        accel_x_buffer[0] = accel_x_buffer[1];
        accel_x_buffer[1] = accel_x_buffer[2];
        accel_x_buffer[2] = accel_x_buffer[3];
        accel_x_buffer[3] = accel_x_buffer[4];
        accel_x_buffer[4] = ((double)((int16_t)(imu_data[0]<<8 | imu_data[1])))/accel_x_scale_factor;

        // Shift the data down in the buffer
        accel_y_buffer[0] = accel_y_buffer[1];
        accel_y_buffer[1] = accel_y_buffer[2];
        accel_y_buffer[2] = accel_y_buffer[3];
        accel_y_buffer[3] = accel_y_buffer[4];
        accel_y_buffer[4] = ((double)((int16_t)(imu_data[2]<<8 | imu_data[3])))/accel_y_scale_factor;

        // Shift the data down in the buffer
        accel_z_buffer[0] = accel_z_buffer[1];
        accel_z_buffer[1] = accel_z_buffer[2];
        accel_z_buffer[2] = accel_z_buffer[3];
        accel_z_buffer[3] = accel_z_buffer[4];
        accel_z_buffer[4] = ((double)((int16_t)(imu_data[4]<<8 | imu_data[5])))/accel_z_scale_factor;

        // Shift the data down in the buffer
        temp_data_buffer[0] = temp_data_buffer[1];
        temp_data_buffer[1] = temp_data_buffer[2];
        temp_data_buffer[2] = temp_data_buffer[3];
        temp_data_buffer[3] = temp_data_buffer[4];
        temp_data_buffer[4] = ((double)((int16_t)(imu_data[6]<<8 | imu_data[7]))/340) + 36.53; //the equation found in the MPU6050 datasheet. This value is in C not F.

        // Shift the data down in the buffer
        gyro_x_buffer[0] = gyro_x_buffer[1];
        gyro_x_buffer[1] = gyro_x_buffer[2];
        gyro_x_buffer[2] = gyro_x_buffer[3];
        gyro_x_buffer[3] = gyro_x_buffer[4];
        gyro_x_buffer[4] = ((double)((int16_t)(imu_data[8]<<8 | imu_data[9]) - gyro_x_offset)) / GYRO_LSB_SENSITIVITY;

        // Shift the data down in the buffer
        gyro_y_buffer[0] = gyro_y_buffer[1];
        gyro_y_buffer[1] = gyro_y_buffer[2];
        gyro_y_buffer[2] = gyro_y_buffer[3];
        gyro_y_buffer[3] = gyro_y_buffer[4];
        gyro_y_buffer[4] = ((double)((int16_t)(imu_data[10]<<8 | imu_data[11]) - gyro_y_offset)) / GYRO_LSB_SENSITIVITY;

        // Shift the data down in the buffer
        gyro_z_buffer[0] = gyro_z_buffer[1];
        gyro_z_buffer[1] = gyro_z_buffer[2];
        gyro_z_buffer[2] = gyro_z_buffer[3];
        gyro_z_buffer[3] = gyro_z_buffer[4];
        gyro_z_buffer[4] = ((double)((int16_t)(imu_data[12]<<8 | imu_data[13]) - gyro_z_offset)) / GYRO_LSB_SENSITIVITY;

        // See if this takes too long
        FilterDataFloating(accel_x_buffer, BUFFER_SAMPLES, &accel_x_output);
        FilterDataFloating(accel_y_buffer, BUFFER_SAMPLES, &accel_y_output);
        FilterDataFloating(accel_z_buffer, BUFFER_SAMPLES, &accel_z_output);
        FilterDataFloating(temp_data_buffer, BUFFER_SAMPLES, &temp_data_output);
        FilterDataFloating(gyro_x_buffer, BUFFER_SAMPLES, &gyro_x_output);
        FilterDataFloating(gyro_y_buffer, BUFFER_SAMPLES, &gyro_y_output);
        FilterDataFloating(gyro_z_buffer, BUFFER_SAMPLES, &gyro_z_output);

        // Make sure the data is above a minimum threshold. This prevents tiny
        // movement from effecting our data
        gyro_x_output = ((-1*GYRO_THRESHOLD_XY) < gyro_x_output && gyro_x_output < GYRO_THRESHOLD_XY) ? 0 : gyro_x_output;
        gyro_y_output = ((-1*GYRO_THRESHOLD_XY) < gyro_y_output && gyro_y_output < GYRO_THRESHOLD_XY) ? 0 : gyro_y_output;
        gyro_z_output = ((-1*GYRO_THRESHOLD) < gyro_z_output && gyro_z_output < GYRO_THRESHOLD) ? 0 : gyro_z_output;

        // This data is output to to user so make sure we have the lock before doing anything
        while(dataLock == true)
        {}

        dataLock = true; // Get the lock. In the future this should be a semaphore  
        accel_x_output = ((-1*ACCEL_THRESHOLD) < accel_x_output && accel_x_output < ACCEL_THRESHOLD) ? 0 : accel_x_output;
        accel_y_output = ((-1*ACCEL_THRESHOLD) < accel_y_output && accel_y_output < ACCEL_THRESHOLD) ? 0 : accel_y_output;
        accel_z_output = ((-1*ACCEL_THRESHOLD) < accel_z_output && accel_z_output < ACCEL_THRESHOLD) ? 0 : accel_z_output;
        
        // Calculate the instantaneous RPM
        rpm = (gyro_z_output/6);
        rpm = (gyro_z_output < 0) ? -1*rpm : rpm; // We want rpm to be positive always
        
        // Calculate the current angle in the x and y direction
        currentAngleYGyro += (gyro_y_output/S_TO_MS)*100; // Convert seconds to ms and then multiple by the spacing between timer interrupts
        currentAngleXGyro += (gyro_x_output/S_TO_MS)*100; //

        if( currentAngleYGyro < -360 )
        {
            currentAngleYGyro += 360; // readjust back to 0 if made a full rotation
        }
        else if( currentAngleYGyro > 360 )
        {
            currentAngleYGyro -= 360; // readjust back to 0 if made a full rotation
        }

        if( currentAngleXGyro < -360 )
        {
            currentAngleXGyro += 360; // readjust back to 0 if made a full rotation
        }
        else if( currentAngleXGyro > 360 )
        {
            currentAngleXGyro -= 360; // readjust back to 0 if made a full rotation
        }
        
        // Calculate the angle based on the accel data
        currentAngleYAccel = atan(accel_x_output/sqrt(pow(accel_y_output,2)+pow(accel_z_output,2)))*(180/M_PI);
        currentAngleXAccel = atan(accel_y_output/sqrt(pow(accel_x_output,2)+pow(accel_z_output,2)))*(180/M_PI);

        // Reset the integration so that we don't keep accumulating error
        if(currentAngleXAccel > -.01 && currentAngleXAccel < .01)
        {
            currentAngleXGyro = 0;
        }

        if(currentAngleYAccel > -.01 && currentAngleYAccel < .01)
        {
            currentAngleYGyro = 0;
        }

        // Apply the complementary filter
        finalAngleX = GAIN*currentAngleXGyro + (1-GAIN)*currentAngleXAccel;
        finalAngleY = GAIN*currentAngleYGyro + (1-GAIN)*currentAngleYAccel;
        dataLock = false;


        #ifdef DEBUG
        printf("accel_x_degree: %lf\n", currentAngleXAccel);
        printf("accel_y_degree: %lf\n", currentAngleYAccel);
        printf("Gyro angle x: %lf\n", currentAngleXGyro);
        printf("Gyro angle y: %lf\n", currentAngleYGyro);
        printf("Final angle X degrees: %lf\n", finalAngleX);
        printf("Final angle Y degrees: %lf\n", finalAngleY);
        printf("RPM: %lf\n", rpm);
        #endif

    }
    else
    {
        printf("I2C read error\n");
    }
}
