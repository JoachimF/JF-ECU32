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
#include <stdio.h>
#include "esp_system.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"

#include "jf-ecu32.h"
#include "nvs_ecu.h"

#define BLINK_GPIO 2
#define BUFFSIZE 2000

static const char *TAG = "ECU";

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
    .minutes = 0 ,
    .secondes = 0 ,
    .GAZ = 0 ,
    .Aux = 0 ,
    .RPM = 0 ,
    .EGT = 0 ,
    .phase_fonctionnement = WAIT ,
    .position_gaz = COUPE ,
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


void init(void)
{
    turbine.secondes = 0 ;
    turbine.minutes = 0 ;

    //Init les sortie PWM
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

    read_nvs() ;
    //turbine_config.log_count++ ;
    //write_nvs_turbine() ;

    for (int i = 0; i < 5; i++)
    {   
        ledc_channel[i].speed_mode = LEDC_LOW_SPEED_MODE;
        ledc_channel[i].timer_sel = LEDC_TIMER_0;
        ledc_channel[i].intr_type = LEDC_INTR_DISABLE;
        ledc_channel[i].duty = 0;
        ledc_channel[i].hpoint = 0;
        
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel[i]));
    }
    //Set les sortie pour test
    turbine.pump1.set_power(&turbine.pump1.config,256) ;
    turbine.pump2.set_power(&turbine.pump2.config,512) ;
    turbine.glow.set_power(&turbine.glow.config,128) ;
    turbine.vanne1.set_power(&turbine.vanne1.config,50) ;
    turbine.vanne2.set_power(&turbine.vanne2.config,100) ;
    
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
    ESP_LOGI(TAG,"Target : %d - pump : %d\n",RPM,res) ;
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

    xTimer60s = xTimerCreate("Timer60s",       // Just a text name, not used by the kernel.
                            ( 60000 /portTICK_PERIOD_MS ),   // The timer period in ticks.
                            pdFALSE,        // The timers will auto-reload themselves when they expire.
                            ( void * ) 2,  // Assign each timer a unique id equal to its array index.
                            vTimer60sCallback // Each timer calls the same callback when it expires.
                            );

    xTimerStart( xTimer1s, 0 ) ;
    xTimerStart( xTimer60s, 0 ) ;
}

void vTimer1sCallback( TimerHandle_t pxTimer )
{
    if( xSemaphoreTake(xTimeMutex,( TickType_t ) 10) == pdTRUE ) {
        turbine.secondes++ ;
        if(turbine.secondes > 59) {
            turbine.secondes = 0 ;
            turbine.minutes++ ;
        }
        ESP_LOGI(TAG,"%02d:%02d",turbine.minutes,turbine.secondes) ;
    }
    xSemaphoreGive(xTimeMutex) ;
    //ESP_LOGI("Time", "%02d:%02d",turbine.minutes,turbine.secondes);
    //long long int Timer1 = esp_timer_get_time();
    //printf("Timer: %lld μs\n", Timer1/1000);  
}

void vTimer60sCallback( TimerHandle_t pxTimer )
{
        ESP_ERROR_CHECK(esp_wifi_stop() );
        ESP_LOGI(TAG,"Wifi STOP") ;
        vTaskDelete( xWebHandle );
        ESP_LOGI(TAG,"Server STOP") ;

}

void ecu_task(void * pvParameters ) 
{
    while(1)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
            switch(turbine.phase_fonctionnement)
            {
                case WAIT :
                    if(turbine.EGT > 100)
                        turbine.phase_fonctionnement = COOL ;
                    else if(turbine.position_gaz == COUPE) {}
                        //Attendret
                    else if(turbine.position_gaz == PLEINGAZ)
                        // Attendre 1sec pour verifier si la position reste
                        turbine.phase_fonctionnement = START ;
                    else if(turbine.position_gaz == MIGAZ)
                        // Attendre 1sec pour verifier si la position reste
                        turbine.phase_fonctionnement = PURGE ;
                    break;
                case START :
                    if(turbine.position_gaz == COUPE)
                        turbine.phase_fonctionnement = WAIT ;
                    else
                        if(config_ECU.glow_type == GAS)
                            turbine.phase_fonctionnement = GLOW ;
                         else
                            turbine.phase_fonctionnement = KEROSTART ;
                    break;
                case GLOW :
                    if(turbine.position_gaz == COUPE)
                        turbine.phase_fonctionnement = WAIT ;
                    else if(turbine.position_gaz == PLEINGAZ){
                        turbine.glow.set_power(&turbine.glow.config,turbine_config.glow_power) ;
                        //Attendre 1 seconde
                        turbine.starter.set_power(&turbine.starter.config,300) ;
                        if(turbine.EGT > 100)
                            turbine.phase_fonctionnement = PREHEAT ;
                        else{}
                            //Attendre 5 secondes Max sinon GOTO WAIT
                        }
                    break;
                case KEROSTART :
                    if(turbine.position_gaz == COUPE)
                        turbine.phase_fonctionnement = WAIT ;
                    else if(turbine.position_gaz == PLEINGAZ){
                        turbine.glow.set_power(&turbine.glow.config,turbine_config.glow_power) ;
                        //Attendre 10 secondes
                        //Mettre des petits coups de pompe
                        turbine.starter.set_power(&turbine.starter.config,300) ; //2000RPM
                        
                        if(turbine.EGT > 100)
                            turbine.phase_fonctionnement = PREHEAT ;
                        else{}
                            //Attendre 5 secondes Max sinon GOTO WAIT
                        }
                    break;
                case PREHEAT :

                    break;
                case RAMP :

                    break;
                case IDLE :

                    break;
                case PURGE :
                    if(turbine.position_gaz == COUPE)
                        // Attendre 1sec pour verifier si la position reste
                        turbine.phase_fonctionnement = WAIT ;
                    else if(turbine.position_gaz == PLEINGAZ){}
                    else if(turbine.position_gaz == MIGAZ){}
                        //Ne rien faire
                        // Si GAZ audessus du milieu activer la pompe proptionnelement
                    break;
                case COOL :
                    if(turbine.EGT > 100){
                        turbine.starter.set_power(&turbine.starter.config,500) ; //5000RPM
                        turbine.pump1.set_power(&turbine.pump1.config,turbine_config.min_pump1) ;
                        //Attendre 1 secondes
                        turbine.pump1.off(&turbine.pump1.config) ;
                        //Attendre 5 secondes
                        turbine.starter.off(&turbine.starter.config) ;
                        //Attendre 5 secondes ;
                    }
                    else
                        turbine.phase_fonctionnement = WAIT ;
                    break;
            }
    }

}