/*
  imu.c

  Copyright (C) 2022  Joachim Franken

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "driver/i2c.h"
#include "mpu6050.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/message_buffer.h"
#include "imu.h"
#include "cJSON.h"


#define I2C_MASTER_SCL_IO 22      /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 21      /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_0  /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ 400000 /*!< I2C master clock frequency */

TaskHandle_t xIMUHandle ;
QueueHandle_t xQueueIMU = NULL;
MessageBufferHandle_t xMessageBufferToClient;

static const char *TAG = "mpu6050";

static mpu6050_handle_t mpu6050 = NULL;

/**
 * @brief i2c master initialization
 */
static void i2c_bus_init(void)
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;

    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, ret, "I2C config returned error");

    ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, ret, "I2C install returned error");
}

/**
 * @brief i2c mpu6050 initialization
 */
void i2c_sensor_mpu6050_init(void)
{
    esp_err_t ret;

    uint8_t mpu6050_deviceid;
    mpu6050_acce_value_t acce;
    //mpu6050_gyro_value_t gyro;
    //mpu6050_temp_value_t temp;

    i2c_bus_init();
    mpu6050 = mpu6050_create(I2C_MASTER_NUM, MPU6050_I2C_ADDRESS);
    TEST_ASSERT_NOT_NULL_MESSAGE(mpu6050, "MPU6050 create returned NULL");

    ret = mpu6050_config(mpu6050, ACCE_FS_4G, GYRO_FS_500DPS);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = mpu6050_wake_up(mpu6050);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = mpu6050_get_deviceid(mpu6050, &mpu6050_deviceid);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(MPU6050_WHO_AM_I_VAL, mpu6050_deviceid, "Who Am I register does not contain expected data");

    ret = mpu6050_get_acce(mpu6050, &acce);
    ESP_LOGI(TAG,"Accel x : %f - Accel y : %f - Accel z : %f",acce.acce_x,acce.acce_y,acce.acce_z) ;
    xQueueIMU = xQueueCreate(100, sizeof(mpu6050_acce_value_t));
}


// Tache de lecture de l'accéléromètre
void task_imu(void *pvParameter)
{
  mpu6050_acce_value_t acce;
  while(1)
  {
    mpu6050_get_acce(mpu6050, &acce);
    ESP_LOGD(TAG,"Accel x : %f - Accel y : %f - Accel z : %f",acce.acce_x,acce.acce_y,acce.acce_z) ;
    if (xQueueSend(xQueueIMU, &acce, 100) != pdPASS ) {
				//ESP_LOGE(TAG, "xQueueSend fail");
			}

    
    vTaskDelay(5 / portTICK_PERIOD_MS); //200 hz
    
    /* Creation du message pour le websocket*/
    /*cJSON *request;
		request = cJSON_CreateObject();
    cJSON_AddStringToObject(request, "id", "data-request");
    cJSON_AddNumberToObject(request, "roll", acce.acce_x);
    cJSON_AddNumberToObject(request, "pitch", acce.acce_y);
    cJSON_AddNumberToObject(request, "yaw", 0.0);
    char *my_json_string = cJSON_Print(request);
    ESP_LOGD(TAG, "my_json_string\n%s",my_json_string);
    size_t xBytesSent = xMessageBufferSend(xMessageBufferToClient, my_json_string, strlen(my_json_string), 100);
    if (xBytesSent != strlen(my_json_string)) {
      ESP_LOGE(TAG, "xMessageBufferSend fail");
    }
    cJSON_Delete(request);
    cJSON_free(my_json_string);

    vTaskDelay(1);*/
  }
}