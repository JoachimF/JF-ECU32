/*
  jf-ecu32.h

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

#include "driver/ledc.h"
#include "jf-ecu32.h"
#include <stdio.h>
#include "esp_system.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"

#define BLINK_GPIO 2
#define BUFFSIZE 2000

static const char *TAG = "HTTP";

void linear_interpolation(uint32_t rpm1,uint32_t pump1,uint32_t rpm2,uint32_t pump2,uint32_t rpm,uint32_t *res) //RPM,PUMP,RPM,PUMP
{
    *res =  pump1 + ((pump2-pump1)* (rpm - rpm1))/(rpm2-rpm1) ;
    printf("RPM1 : %d ; pump1 : %d , RPM1 : %d ; pump1 : %d , rpm : %d , res : %d\n",rpm1,pump1,rpm2,pump2,rpm,*res);
}

void set_power_func(_pwm_config *config ,uint16_t power)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, config->ledc_channel, power));
    printf("Pin : %d\n",config->gpio_num) ;
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, config->ledc_channel));
}

ledc_timer_config_t ledc_timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .timer_num  = LEDC_TIMER_0,
    .duty_resolution = LEDC_TIMER_10_BIT,
    .freq_hz = 10000,
    .clk_cfg = LEDC_AUTO_CLK
};
ledc_channel_config_t ledc_channel[5];

_engine_t turbine = { 
    .pump1.config.gpio_num = 25,
    .pump1.config.ledc_channel = LEDC_CHANNEL_0,
    .pump1.set_power = set_power_func,
    .pump1.target = 0 ,
    .pump1.new_target = 0 ,
    .pump2.config.gpio_num = 26,
    .pump2.config.ledc_channel = LEDC_CHANNEL_1,
    .pump2.set_power = set_power_func,
    .pump2.target = 0 ,
    .pump2.new_target = 0 ,
    .glow.config.gpio_num = 32,
    .glow.config.ledc_channel = LEDC_CHANNEL_2,
    .glow.set_power = set_power_func,
    .vanne1.config.gpio_num = 27,
    .vanne1.config.ledc_channel = LEDC_CHANNEL_3,
    .vanne1.set_power = set_power_func,
    .vanne2.config.gpio_num = 12,
    .vanne2.config.ledc_channel = LEDC_CHANNEL_4,
    .vanne2.set_power = set_power_func
 };
 
 _configEngine_t turbine_config ;
 _BITsconfigECU_u config_ECU ;
 nvs_handle my_handle;


void init_nvs(void)
{
    //nvs_flash_erase() ;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    printf("\n");
    printf("Opening Non-Volatile Storage (NVS) handle... ");
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } 
}
void read_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    size_t required_size;
    
    err = nvs_get_blob(my_handle, "config", NULL, &required_size );
    err = nvs_get_blob(my_handle, "config", (void *)&turbine_config, &required_size);
    switch (err) {
            case ESP_OK:
                printf("Done\n\n");
                printf("Log count = %d\n\n", turbine_config.log_count);
                printf("glow power = %d\n\n", turbine_config.glow_power);
                printf("Max rpm = %d\n\n", turbine_config.jet_full_power_rpm);
                printf("idle rpm = %d\n\n", turbine_config.jet_idle_rpm);
                printf("start_temp = %d\n\n", turbine_config.start_temp);
                printf("max_temp = %d\n\n", turbine_config.max_temp);
                printf("acceleration_delay = %d\n\n", turbine_config.acceleration_delay);
                printf("deceleration_delay = %d\n\n", turbine_config.deceleration_delay);
                printf("stability_delay = %d\n\n", turbine_config.stability_delay);
                printf("max_pump1 = %d\n\n", turbine_config.max_pump1);
                printf("min_pump1= %d\n\n", turbine_config.min_pump1);
                printf("max_pump2 = %d\n\n", turbine_config.max_pump2);
                printf("min_pump2= %d\n\n", turbine_config.jet_min_rpm);
                for(int i=0;i<50;i++)
                {
                    printf("pump = %d - ", turbine_config.power_table.pump[i]);
                    printf("rpm = %d\n", turbine_config.power_table.RPM[i]);
                }
                printf("\nChecksum = %d\n", turbine_config.power_table.checksum_RPM);
                printf("\nChecksum2 = %d\n", checksum_power_table());
                if(checksum_power_table() != turbine_config.power_table.checksum_RPM)
                    {
                    init_power_table() ;
                    init_random_pump() ;
                    write_nvs() ;
                    }
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                printf("The value is not initialized yet!\n");
                required_size = sizeof(turbine_config);
                turbine_config.log_count = 1 ;
                turbine_config.glow_power = 25 ;
                turbine_config.jet_full_power_rpm = 145000 ;
                turbine_config.jet_idle_rpm = 35000 ;
                turbine_config.start_temp = 100 ;
                turbine_config.max_temp = 750 ;
                turbine_config.acceleration_delay = 10 ;
                turbine_config.deceleration_delay = 12 ;
                turbine_config.stability_delay = 5 ;
                turbine_config.max_pump1 = 1024 ;
                turbine_config.min_pump1 = 0 ;
                turbine_config.max_pump2 = 512 ;
                turbine_config.jet_min_rpm = 0 ;
                init_power_table() ;
                write_nvs() ;
                break;
            default :
                printf("Error (%s) reading!\n", esp_err_to_name(err));
                init_power_table() ;
                init_random_pump() ;
                write_nvs() ;
        }
    err = nvs_get_blob(my_handle, "configECU", NULL, &required_size );
    err = nvs_get_blob(my_handle, "configECU", (void *)&config_ECU, &required_size);
    switch (err) {
            case ESP_OK:
                printf("config_ECU Done\n\n");
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                printf("config_ECU The value is not initialized yet!\n");
                write_nvs() ;
                break;
            default :
                printf("Error (%s) reading!\n", esp_err_to_name(err));
        }        
}

void write_nvs(void)
{
        // Write
        esp_err_t err = nvs_flash_init();
        size_t required_size = sizeof(_configEngine_t);
        printf("Save config to turbine_config Struct... Size : %d ",required_size);
        err = nvs_set_blob(my_handle, "config", (const void*)&turbine_config, required_size);
        //err = nvs_set_str(my_handle, "nvs_struct", (const char*)nvs_struct.buffer);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        // Commit written value.
        // After setting any values, nvs_commit() must be called to ensure changes are written
        // to flash storage. Implementations may write to storage at other times,
        // but this is not guaranteed.
        printf("Committing updates in NVS ... ");
        err = nvs_commit(my_handle);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        required_size = sizeof(_BITsconfigECU_u);
        printf("Save config to config_ECU Struct... Size : %d ",required_size);
        err = nvs_set_blob(my_handle, "configECU", (const void*)&config_ECU, required_size);
        //err = nvs_set_str(my_handle, "nvs_struct", (const char*)nvs_struct.buffer);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        // Commit written value.
        // After setting any values, nvs_commit() must be called to ensure changes are written
        // to flash storage. Implementations may write to storage at other times,
        // but this is not guaranteed.
        printf("Committing updates in NVS ... ");
        err = nvs_commit(my_handle);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        // Close
        nvs_close(my_handle);
}

void init(void)
{
    turbine.secondes = 0 ;
    turbine.minutes = 0 ;

    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    ledc_channel[0].channel = turbine.pump1.config.ledc_channel;
    ledc_channel[0].gpio_num = turbine.pump1.config.gpio_num ;
    ledc_channel[1].channel = turbine.pump2.config.ledc_channel;
    ledc_channel[1].gpio_num = turbine.pump2.config.gpio_num ;
    ledc_channel[2].channel = turbine.glow.config.ledc_channel;
    ledc_channel[2].gpio_num = turbine.glow.config.gpio_num ;
    ledc_channel[3].channel = turbine.vanne1.config.ledc_channel;
    ledc_channel[3].gpio_num = turbine.vanne1.config.gpio_num ;
    ledc_channel[4].channel = turbine.vanne2.config.ledc_channel;
    ledc_channel[4].gpio_num = turbine.vanne2.config.gpio_num ;
    init_nvs() ;
    read_nvs() ;
    turbine_config.log_count++ ;
    write_nvs() ;
    //turbine_config.jet_full_power_rpm = 150000 ;
    //write_nvs() ;
    for (int i = 0; i < 5; i++)
    {   
        ledc_channel[i].speed_mode = LEDC_LOW_SPEED_MODE;
        ledc_channel[i].timer_sel = LEDC_TIMER_0;
        ledc_channel[i].intr_type = LEDC_INTR_DISABLE;
        ledc_channel[i].duty = 0;
        ledc_channel[i].hpoint = 0;
        
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel[i]));
    }
    turbine.pump1.set_power(&turbine.pump1.config,256) ;
    turbine.pump2.set_power(&turbine.pump2.config,512) ;
    turbine.glow.set_power(&turbine.glow.config,128) ;
    turbine.vanne1.set_power(&turbine.vanne1.config,50) ;
    turbine.vanne2.set_power(&turbine.vanne2.config,100) ;
    
    create_timers() ;
}

void set_kero_pump_target(uint32_t RPM)
{
    uint32_t rpm1,pump1,pump2,rpm2,res ;
    for(int i = 0;i<50;i++)
    {
        if(RPM >= turbine_config.power_table.RPM[i])
        {
            pump1 = turbine_config.power_table.pump[i] ;
            rpm1 = turbine_config.power_table.RPM[i] ;
        }
        else if(RPM <= turbine_config.power_table.RPM[i])
        {
            pump2 = turbine_config.power_table.pump[i] ;
            rpm2 = turbine_config.power_table.RPM[i] ;
            i = 50 ;
        }
    }
    linear_interpolation(rpm1,pump1,rpm2,pump2,RPM,&res) ;
    turbine.pump1.target = res ;
    turbine.pump1.new_target = 1 ;
    printf("Target : %d - pump : %d\n",RPM,res) ;
}

void init_random_pump(void)
{
  uint32_t interval = 20 ;
  turbine_config.power_table.checksum_pump = 0 ;
  turbine_config.power_table.pump[0] = 50 ;
  turbine_config.power_table.checksum_pump += turbine_config.power_table.pump[0] ;
  for(int i=1;i<50;i++)
  {
        turbine_config.power_table.pump[i] = turbine_config.power_table.pump[0] + interval*i;
        turbine_config.power_table.checksum_pump += turbine_config.power_table.pump[i] ;
  }
}
uint32_t checksum_power_table(void)
{
  uint32_t interval,checksum_RPM ;
  interval = (turbine_config.jet_full_power_rpm - turbine_config.jet_idle_rpm) / 48 ;
  checksum_RPM = 0 ;
  checksum_RPM += turbine_config.power_table.RPM[0] ;
  for(int i=1;i<49;i++)
  {
        checksum_RPM += turbine_config.jet_idle_rpm + interval*i;
  }
  checksum_RPM += turbine_config.jet_full_power_rpm ;
  return checksum_RPM ;
}

void init_power_table(void)
{
  uint32_t interval ;
  turbine_config.power_table.checksum_RPM = 0 ;
  interval = (turbine_config.jet_full_power_rpm - turbine_config.jet_idle_rpm) / 48 ;
  turbine_config.power_table.RPM[0] = turbine_config.jet_idle_rpm ;
  turbine_config.power_table.checksum_RPM += turbine_config.power_table.RPM[0] ;
  for(int i=1;i<49;i++)
  {
        turbine_config.power_table.RPM[i] = turbine_config.jet_idle_rpm + interval*i;
        turbine_config.power_table.checksum_RPM += turbine_config.power_table.RPM[i] ;
  }
  turbine_config.power_table.RPM[49] = turbine_config.jet_full_power_rpm ;
  turbine_config.power_table.checksum_RPM += turbine_config.power_table.RPM[49] ;
}

void update_curve_file(void)
{
    FILE *fd = NULL;
	char FileName[] = "/html/curves.txt" ;
    fd = fopen(FileName, "w");
	if (!fd) {
       ESP_LOGI("File", "Failed to read existing file : logs.txt");
    }
    fprintf(fd,"RPM;Pompe\n");
    for (int i=0;i<50;i++) {
		fprintf(fd,"%d;%d\n", turbine_config.power_table.RPM[i],turbine_config.power_table.pump[i]);
	}
    fclose(fd);
}


void head_logs_file(void)
{
    FILE *fd = NULL;
	char FileName[] = "/html/logs.txt" ;
    fd = fopen(FileName, "a");
	if (!fd) {
       ESP_LOGI("File", "Failed to read existing file : logs.txt");
    }
    fprintf(fd,"Num;Time;RPM;EGT;Pompe1;Cible Pompe1;Pompe2;Glow;Vanne1;Vanne2;Voie Gaz;Voie aux\n");
    fclose(fd);
}

void update_logs_file(void)
{
    FILE *fd = NULL;
	char FileName[] = "/html/logs.txt" ;
    uint8_t minutes,secondes ;
    if( xSemaphoreTake(xTimeMutex,( TickType_t ) 10) == pdTRUE ) 
    {
        minutes = turbine.minutes ;
        secondes = turbine.secondes ;
    }
    else
    {
        minutes = 99 ;
        secondes = 99 ;
    
    }
    xSemaphoreGive(xTimeMutex) ;
    fd = fopen(FileName, "a");
    if (!fd) {
        ESP_LOGI("File", "Failed to read existing file : logs.txt");
    }
    fprintf(fd,"%d;%02d:%02d;%06d;%03d;%04d;%04d;%04d;%03d;%03d;%03d;%04d;%04d\n", turbine_config.log_count,minutes,secondes,turbine.RPM,turbine.EGT,
                                                                                            turbine.pump1.value,turbine.pump1.target,turbine.pump2.value,turbine.glow.value,
                                                                                            turbine.vanne1.value,turbine.vanne2.value,turbine.GAZ,turbine.Aux);   

    fclose(fd);                                                                                            
}

void log_task( void * pvParameters )
{



 	ESP_LOGI(TAG, "Start Logtask");
    while(1) {
        //ESP_LOGI("LOG", "New Log");

        update_logs_file() ;
        
        vTaskDelay( 500 / portTICK_PERIOD_MS);
    }
    //ESP_LOGI(TAG, "finish");
	vTaskDelete(xlogHandle);
}

void create_timers(void)
{
    xTimeMutex = xSemaphoreCreateMutex() ;
    xTimer1s = xTimerCreate("Timer1s",       // Just a text name, not used by the kernel.
                            ( 1000 /portTICK_PERIOD_MS ),   // The timer period in ticks.
                            pdTRUE,        // The timers will auto-reload themselves when they expire.
                            ( void * ) 1,  // Assign each timer a unique id equal to its array index.
                            vTimer1sCallback // Each timer calls the same callback when it expires.
                            );

    xTimerStart( xTimer1s, 0 ) ;
}

void vTimer1sCallback( TimerHandle_t pxTimer )
{
    if( xSemaphoreTake(xTimeMutex,( TickType_t ) 10) == pdTRUE ) {
        turbine.secondes++ ;
        if(turbine.secondes > 59) {
            turbine.secondes = 0 ;
            turbine.minutes++ ;
        }
    }
    xSemaphoreGive(xTimeMutex) ;
    //ESP_LOGI("Time", "%02d:%02d",turbine.minutes,turbine.secondes);
    //long long int Timer1 = esp_timer_get_time();
    //printf("Timer: %lld μs\n", Timer1/1000);  
}