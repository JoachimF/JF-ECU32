/*
  jf-ecu32.c

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
#include "driver/mcpwm.h" //IDF 4.3.4
//#include "driver/mcpwm_prelude.h" // IDF 5.0
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

//Taches
TaskHandle_t xlogHandle ;
TaskHandle_t xWebHandle ;
TaskHandle_t xecuHandle ;

//Timers
TimerHandle_t xTimer1s ;
TimerHandle_t xTimer60s ;

// Semaphores
SemaphoreHandle_t xTimeMutex;
SemaphoreHandle_t log_task_start;
SemaphoreHandle_t ecu_task_start;

static const char *TAG = "ECU";

void linear_interpolation(uint32_t rpm1,uint32_t pump1,uint32_t rpm2,uint32_t pump2,uint32_t rpm,uint32_t *res) //RPM,PUMP,RPM,PUMP
{
    *res =  pump1 + ((pump2-pump1)* (rpm - rpm1))/(rpm2-rpm1) ;
    //ESP_LOGI(TAG,"RPM1 : %d ; pump1 : %d , RPM1 : %d ; pump1 : %d , rpm : %d , res : %d",rpm1,pump1,rpm2,pump2,rpm,*res);
}

void set_power_func_us(_pwm_config *config ,int32_t value)
{
    mcpwm_set_duty_in_us(config->MCPWM_UNIT, config->MCPWM_TIMER, config->MCPWM_GEN, value);
    //ESP_LOGI(TAG,"MCPWM_UNIT : %d ; MCPWM_TIMER : %d ; MCPWM_GEN : %d ; value : %d ; pin : %d",config->MCPWM_UNIT,config->MCPWM_TIMER,config->MCPWM_GEN,value,config->gpio_num);
}

void set_power_func(_pwm_config *config ,float value)
{
    mcpwm_set_duty(config->MCPWM_UNIT, config->MCPWM_TIMER, config->MCPWM_GEN, value);
}

void set_power_ledc(_ledc_config *config ,uint32_t value)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, config->ledc_channel, value));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, config->ledc_channel));
    //ESP_LOGI(TAG,"ledc_channel %d ; value : %d ; pin : %d",config->ledc_channel,value,config->gpio_num);
}

ledc_timer_config_t ledc_timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .timer_num  = LEDC_TIMER_0,
    .duty_resolution = LEDC_TIMER_8_BIT,
    .freq_hz = 400,
    .clk_cfg = LEDC_AUTO_CLK
};

static ledc_channel_config_t ledc_channel[3];
static mcpwm_config_t pwm_config[2]; //IDF 4.3.4
//mcpwm_timer_handle_t timer[2] = NULL; //IDF 5.0
//mcpwm_timer_config_t timer_config[2] = NULL ;


_engine_t turbine = { 
    .minutes = 0 ,
    .secondes = 0 ,
    .GAZ = 0 ,
    .Aux = 0 ,
    .RPM = 0 ,
    .EGT = 0 ,
    .phase_fonctionnement = WAIT ,
    .position_gaz = COUPE ,

// Configuration en MCPWM
    .pump1.config.nbits = _10BITS,
    .pump1.config.gpio_num = PUMP1_PIN,
//    .pump1.config.MCPWM_UNIT = MCPWM_UNIT_0,
    .pump1.set_power = set_power_func,
    .pump1.target = 0 ,
    .pump1.new_target = 0 ,

    .pump2.config.nbits = _10BITS,
    .pump2.config.gpio_num = PUMP2_PIN,
    //.pump2.config.MCPWM_UNIT = MCPWM_UNIT_0,
    .pump2.set_power = set_power_func,
    .pump2.target = 0 ,
    .pump2.new_target = 0 ,

    .starter.config.nbits = _10BITS,
    .starter.config.gpio_num = STARTER_PIN,
//    .starter.config.MCPWM_UNIT = MCPWM_UNIT_0,
    .starter.set_power = set_power_func,    

// Configuration en LEDC
    .vanne1.config.gpio_num = VANNE1_PIN,
    .vanne1.config.ledc_channel = LEDC_CHANNEL_0,
    .vanne1.set_power = set_power_ledc,

    .vanne2.config.gpio_num = VANNE2_PIN,
    .vanne2.config.ledc_channel = LEDC_CHANNEL_1,
    .vanne2.set_power = set_power_ledc,
    
    .glow.config.gpio_num = GLOW_PIN,    
    .glow.config.ledc_channel = LEDC_CHANNEL_2,
    .glow.set_power = set_power_ledc,
 };
 
 _configEngine_t turbine_config ;
 _BITsconfigECU_u config_ECU ;

void init_pwm_outputs(_pwm_config *config)
{
    mcpwm_gpio_init(config->MCPWM_UNIT, config->MCPWM, config->gpio_num);   
    ESP_LOGI(TAG,"MCPWM_UNIT : %d ; MCPWM_TIMER : %d ; MCPWM_GEN : %d ; MCPWM_TIMER : %d ; pin : %d",config->MCPWM_UNIT,config->MCPWM_TIMER,config->MCPWM_GEN,config->MCPWM_TIMER,config->gpio_num); 
}

// IDF 4.3.4
void init_mcpwm(void) // IDF 4.3.4
{
    printf("initializing mcpwm servo control gpio......\n");

    turbine.pump1.config.MCPWM_GEN = MCPWM_GEN_A ;
    if(turbine.pump1.config.ppm_pwm == PWM){
        turbine.pump1.config.MCPWM = MCPWM0A ;
        turbine.pump1.config.MCPWM_TIMER = PWM_TIMER ;
        turbine.pump1.config.MCPWM_UNIT = MCPWM_UNIT_0 ;
    }
    else{
        turbine.pump1.config.MCPWM = MCPWM1A ;
        turbine.pump1.config.MCPWM_TIMER = PPM_TIMER ;
        turbine.pump1.config.MCPWM_UNIT = MCPWM_UNIT_1 ; 
    }
    
    ESP_LOGI(TAG,"MCPWM_UNIT init outputs") ;
    if(turbine.pump2.config.ppm_pwm != NONE)
    {
        turbine.pump2.config.MCPWM_GEN = MCPWM_GEN_B ;
        if(turbine.pump2.config.ppm_pwm == PWM )
        {
            turbine.pump2.config.MCPWM = MCPWM0B ;
            turbine.pump2.config.MCPWM_TIMER = PWM_TIMER ;
            turbine.pump2.config.MCPWM_UNIT = MCPWM_UNIT_0 ;
        }
        else if(turbine.pump2.config.ppm_pwm == PPM )
        {
            turbine.pump2.config.MCPWM = MCPWM1B ;
            turbine.pump2.config.MCPWM_TIMER = PPM_TIMER ;
            turbine.pump2.config.MCPWM_UNIT = MCPWM_UNIT_1 ;
        }
        init_pwm_outputs(&turbine.pump2.config) ;
    }
    
    turbine.starter.config.MCPWM_TIMER = MCPWM_TIMER_2 ;
    turbine.starter.config.MCPWM_GEN = MCPWM_GEN_A ;
    turbine.starter.config.MCPWM = MCPWM2A ;
    if(config_ECU.output_starter == PWM)
        turbine.starter.config.MCPWM_UNIT = MCPWM_UNIT_0 ;
    else
        turbine.starter.config.MCPWM_UNIT = MCPWM_UNIT_1 ;
    
    init_pwm_outputs(&turbine.pump1.config) ;
    init_pwm_outputs(&turbine.starter.config) ;


    
    //2. initial mcpwm configuration
    printf("Configuring Initial Parameters of mcpwm......\n");

    //Config pour PWM
    pwm_config[0].frequency = 10000;    //frequency = 10000Hz, pour les moteur DC et vannes
    pwm_config[0].cmpr_a = 0;    //duty cycle of PWMxA = 0  Pompe1
    pwm_config[0].cmpr_b = 0;    //duty cycle of PWMxb = 0  Pompe2
    pwm_config[0].counter_mode = MCPWM_UP_COUNTER;
    pwm_config[0].duty_mode = MCPWM_DUTY_MODE_0;
    //Config pour PPM
    pwm_config[1].frequency = 50;    //frequency = 50Hz, pour les variateur et la bougie
    pwm_config[1].cmpr_a = 0;    //duty cycle of PWMxA = 0 Pompe1
    pwm_config[1].cmpr_b = 0;    //duty cycle of PWMxb = 0 Pompe2
    pwm_config[1].counter_mode = MCPWM_UP_COUNTER;
    pwm_config[1].duty_mode = MCPWM_DUTY_MODE_0;
    
    mcpwm_init(MCPWM_UNIT_0, PWM_TIMER, &pwm_config[0]);    //Configure PWM0A (pompe1) & PWM1B (pompe2) 10KHz
    mcpwm_init(MCPWM_UNIT_1, PPM_TIMER, &pwm_config[1]);    //Configure PWM1A (pompe1) & PWM0B (pompe2) 50Hz
    if(config_ECU.output_starter == PWM)
        mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_2, &pwm_config[0]);    //Configure PWM2A 10KHz (Starter)
    else
        mcpwm_init(MCPWM_UNIT_1, MCPWM_TIMER_2, &pwm_config[1]);    //Configure PWM2A 50KHz (Starter)
    
    ESP_LOGI(TAG,"LEDC init") ;
    ledc_timer_config(&ledc_timer);
    ESP_LOGI(TAG,"LEDC init outputs") ;
    ledc_channel[0].channel = turbine.vanne1.config.ledc_channel;
    ledc_channel[0].gpio_num = turbine.vanne1.config.gpio_num ;
    ledc_channel[1].channel = turbine.vanne2.config.ledc_channel;
    ledc_channel[1].gpio_num = turbine.vanne2.config.gpio_num ;
    ledc_channel[2].channel = turbine.glow.config.ledc_channel;
    ledc_channel[2].gpio_num = turbine.glow.config.gpio_num ;
    for (int i = 0; i < 3; i++)
    {   
        ledc_channel[i].speed_mode = LEDC_LOW_SPEED_MODE;
        ledc_channel[i].timer_sel = LEDC_TIMER_0;
        ledc_channel[i].intr_type = LEDC_INTR_DISABLE;
        ledc_channel[i].duty = 0;
        ledc_channel[i].hpoint = 0;
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel[i]));
    }
}

void init(void)
{
//    turbine.secondes = 0 ;
//    turbine.minutes = 0 ;
    read_nvs() ;

    //Init les sortie PWM
    turbine.starter.config.ppm_pwm = config_ECU.output_starter ;
    turbine.pump1.config.ppm_pwm = config_ECU.output_pump1 ;
    turbine.pump2.config.ppm_pwm = config_ECU.output_pump2 ;
    init_mcpwm() ;


    //Set les sortie pour test
//    turbine.pump1.set_power(&turbine.pump1.config,256) ;
//    turbine.pump2.set_power(&turbine.pump2.config,512) ;
//    turbine.glow.set_power(&turbine.glow.config,128) ;
//    turbine.vanne1.set_power(&turbine.vanne1.config,50) ;
//    turbine.vanne2.set_power(&turbine.vanne2.config,100) ;    
    turbine.EGT = 0 ;
    turbine.GAZ = 1000 ;
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
    //ESP_LOGI(TAG,"Target : %d - pump : %d\n",RPM,res) ;
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
    xSemaphoreTake(log_task_start, portMAX_DELAY);
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
        //ESP_LOGI(TAG,"%02d:%02d",turbine.minutes,turbine.secondes) ;
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
        mdns_free(void);

}

void ecu_task(void * pvParameters ) 
{
    xSemaphoreTake(ecu_task_start, portMAX_DELAY);
    ESP_LOGI(TAG, "Start ECU Task");
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
