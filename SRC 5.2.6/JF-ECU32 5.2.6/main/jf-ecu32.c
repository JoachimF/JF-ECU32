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
#include "sdcard.h"
#include "driver/mcpwm.h" //IDF 4.3.4
//#include "driver/mcpwm_prelude.h" // IDF 5.0
#include <stdio.h>
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "driver/pulse_cnt.h"
#include "freertos/semphr.h"
#include "mdns.h"
#include "driver/rmt_rx.h"
#include "driver/pulse_cnt.h"
//#include "esp_heap_trace.h"
#include <dirent.h>

#include "jf-ecu32.h"
#include "logs.h"
#include "outputs.h"
#include "nvs_ecu.h"
#include "inputs.h"
#include "error.h"
#include "Langues/fr_FR.h"

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

//#define BLINK_GPIO 2
#define BUFFSIZE 2000

//Taches
TaskHandle_t xlogHandle ;
TaskHandle_t xWebHandle ;
TaskHandle_t xecuHandle ;
TaskHandle_t xinputsHandle ;

//Timers
TimerHandle_t xTimer10ms ;
TimerHandle_t xTimer100ms ;
TimerHandle_t xTimer1s ;
TimerHandle_t xTimer60s ;

// Semaphores
//SemaphoreHandle_t xTimeMutex;
//SemaphoreHandle_t xRPMmutex;
//SemaphoreHandle_t xGAZmutex;
SemaphoreHandle_t log_task_start;
SemaphoreHandle_t ecu_task_start;

//Events
EventGroupHandle_t xLogEventGroup;

static const char *TAG = "ECU";

bool isEngineRun(void)
{
    uint8_t phase = turbine.phase_fonctionnement ;
    if(phase ==  KEROSTART || phase == IDLE || phase == RUN || phase == START )
        return 1;
    else
        return 0 ;
}

void linear_interpolation(uint32_t rpm1,uint32_t pump1,uint32_t rpm2,uint32_t pump2,uint32_t rpm,uint32_t *res) //RPM,PUMP,RPM,PUMP
{
    if(rpm2-rpm1 != 0)
        *res =  pump1 + ((pump2-pump1)* (rpm - rpm1))/(rpm2-rpm1) ;
    else
        *res = 0 ;
    //ESP_LOGI(TAG,"RPM1 : %d ; pump1 : %d , RPM1 : %d ; pump1 : %d , rpm : %d , res : %d",rpm1,pump1,rpm2,pump2,rpm,*res);
}

void get_time(uint32_t _time_up, uint8_t *sec, uint8_t *min, uint8_t *heure)
{
    //ESP_LOGI("get_time", "_time_up : %d",_time_up);
    *sec = _time_up % 60 ;
    //ESP_LOGI("get_time", "*sec : %d",*sec);
    if(min != NULL)
    {
        *min = (_time_up % 3600)/60 ;
        //ESP_LOGI("*min", "*min : %d",*min);
    }
    if(heure != NULL)
    {
        *heure = _time_up / 3600 ;
        //ESP_LOGI("get_time", "*heure : %d",*heure);
    }
}

/*Renvoie le nombre total d'heure/minutes/secondes de fonctionnement du moteur*/
void get_time_up(_engine_t *engine, uint8_t *sec, uint8_t *min, uint8_t *heure)
{
    //ESP_LOGI("get_time_up", "engine->time_up : %ld",engine->time_up);
    get_time(engine->time_up,sec,min,heure);
}

/*Renvoie le nombre total d'heure/minutes/secondes de fonctionnement du moteur*/
void get_time_total(_engine_t *engine, uint8_t *sec, uint8_t *min, uint8_t *heure)
{
    get_time(engine->time,sec,min,heure);
}

/*Renvoie le nombre de secondes depuis l'allumage de l'ECU*/
uint8_t get_secondes_up(_engine_t *engine)
{
    return engine->time_up % 60 ;

}

/*Renvoie le nombre de minutes depuis l'allumage de l'ECU*/
uint8_t get_minutes_up(_engine_t *engine)
{
    return (engine->time_up % 3600)/60 ;
}

/*Renvoie le nombre de heure depuis l'allumage de l'ECU*/
uint8_t get_heures_up(_engine_t *engine)
{
    return engine->time_up / 3600 ;
}

/*Renvoie le nombre total de secondes de fonctionnement du moteur*/
uint8_t get_secondes_total(_engine_t *engine)
{
    return engine->time % 60 ;

}

/*Renvoie le nombre total de minutes de fonctionnement du moteur*/
uint8_t get_minutes_total(_engine_t *engine)
{
    return (engine->time % 3600)/60 ;
}

/*Renvoie le nombre total de heures de fonctionnement du moteur*/
uint16_t get_heures_total(_engine_t *engine)
{
    return engine->time / 3600 ;
}

/*void set_power_func_us(_PUMP_t *config ,float value)
{
    mcpwm_set_duty_in_us(config->config.MCPWM_UNIT, config->config.MCPWM_TIMER, config->config.MCPWM_GEN, value);
    config->value = value ;
    //ESP_LOGI(TAG,"MCPWM_UNIT : %d ; MCPWM_TIMER : %d ; MCPWM_GEN : %d ; value : %d ; pin : %d",config->MCPWM_UNIT,config->MCPWM_TIMER,config->MCPWM_GEN,value,config->gpio_num);
}

void set_power_func(_PUMP_t *config ,float value)
{
    ESP_LOGI("set_power_func", "Value = %f", value);
    mcpwm_set_duty(config->config.MCPWM_UNIT, config->config.MCPWM_TIMER, config->config.MCPWM_GEN, value);
    config->value = value  ;
    ESP_LOGI("set_power_func", "config->value = %f", config->value);
}*/



uint8_t get_conf_lipo_elements(void)
{
    return turbine_config.lipo_elements ;
}

void set_conf_lipo_elements(uint8_t elem)
{
    turbine_config.lipo_elements = elem ;
}

void set_batOk(bool set)
{
    turbine.batOk = set ; 
}

bool isBatOk(void)
{
    return turbine.batOk ;
}

float get_Vmin_decollage(void)
{
    return turbine_config.Vmin_decollage ;
}

_engine_t turbine = { 
    .time_up = 0 ,
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
    //.pump1.set_power, = set_power_func,
    .pump1.target = 0 ,
    .pump1.new_target = 0 ,
    .pump1.value = 0 ,

    .pump2.config.nbits = _10BITS,
    .pump2.config.gpio_num = PUMP2_PIN,
    //.pump2.config.MCPWM_UNIT = MCPWM_UNIT_0,
    //.pump2.set_power = set_power_func,
    .pump2.target = 0 ,
    .pump2.new_target = 0 ,
    .pump2.value = 0 ,

    .starter.config.nbits = _10BITS,
    .starter.config.gpio_num = STARTER_PIN,
//    .starter.config.MCPWM_UNIT = MCPWM_UNIT_0,
    //.starter.set_power = set_power_func,  
    .starter.value = 0 ,

// Configuration en LEDC
    .vanne1.config.gpio_num = VANNE1_PIN,
    .vanne1.config.ledc_channel = LEDC_CHANNEL_0,
    //.vanne1.set_power = set_power_ledc,
    .vanne1.value = 0 ,

    .vanne2.config.gpio_num = VANNE2_PIN,
    .vanne2.config.ledc_channel = LEDC_CHANNEL_1,
    //.vanne2.set_power = set_power_ledc,
    .vanne2.value = 0 ,
    
    .glow.config.gpio_num = GLOW_PIN,    
    .glow.config.ledc_channel = LEDC_CHANNEL_2,
    //.glow.set_power = set_power_ledc,
    .glow.value = 0 ,
 };
 
 _configEngine_t turbine_config ;
 _BITsconfigECU_u config_ECU ;




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

    init_errors() ;
    
    //Set les sortie pour test
//    turbine.pump1.set_power(&turbine.pump1.config,256) ;
//    turbine.pump2.set_power(&turbine.pump2.config,512) ;
//    turbine.glow.set_power(&turbine.glow.config,128) ;
//    turbine.vanne1.set_power(&turbine.vanne1.config,50) ;
//    turbine.vanne2.set_power(&turbine.vanne2.config,100) ;    
//    turbine.EGT = 0 ;
//    turbine.GAZ = 1000 ;
}



void phase_to_str(char *status)
{
    switch(turbine.phase_fonctionnement)
    {
        case WAIT : strcpy(status,"WAIT") ;
                    break ;
        case START : strcpy(status,"START") ;
                    break ;
        case KEROSTART : strcpy(status,"KEROSTART") ;
                    break ;
        case IDLE : strcpy(status,"IDLE") ;
                    break ;
        case PURGE : strcpy(status,"PURGE") ;
                    break ;
        case COOL : strcpy(status,"COOL") ;
                    break ;
        case STOP : strcpy(status,"STOP") ;
                    break ;
    }
}

void start_phase_to_str(char *status)
{
    switch(turbine.phase_fonctionnement)
    {
        case TESTGLOW : strcpy(status,"TEST GLOW") ;
                    break ;
        case PREHEAT : strcpy(status,"PREHEAT") ;
                    break ;
        case RAMP : strcpy(status,"RAMP") ;
                    break ;
    }
}

void update_curve_file(void)
{
    FILE *fd = NULL;
	char FileName[] = "/sdcard/logs/curves.txt" ;
    fd = fopen(FileName, "w");
	if (!fd) {
       ESP_LOGI("File", "Failed to open file : curves.txt");       
    } else {
        fprintf(fd,"RPM;Pompe\n");
        for (int i=0;i<50;i++) {
            fprintf(fd,"%ld;%ld\n", turbine_config.power_table.RPM[i],turbine_config.power_table.pump[i]);
        }
    fclose(fd);
    }
}


void start_timers(void)
{
    xTimerStart( xTimer100ms, 0 ) ;
    xTimerStart( xTimer1s, 0 ) ;
    xTimerStart( xTimer60s, 0 ) ;
    xTimerStart( xTimer10ms, 0 ) ;
}

void vTimer10msCallback( TimerHandle_t pxTimer ) //toutes les 100 millisecondes
{
}

void vTimer100msCallback( TimerHandle_t pxTimer ) //toutes les 100 millisecondes
{
    int32_t delta_raw ;
    int32_t delta_filter ;
    for(int i=9; i>0; i--)
        turbine.RPMs[i] = turbine.RPMs[i-1] ;
    turbine.RPMs[0] = get_RPM(&turbine) ;
    delta_raw = turbine.RPMs[0] - turbine.RPMs[9] ;
    delta_filter = (delta_raw + get_delta_RPM(&turbine)) / 2  ;
    set_delta_RPM(&turbine,delta_filter) ;
    
    //printf("\ndelta_raw : %ld delta_filter : %ld get_delta %ld",delta_raw,delta_filter,get_delta_RPM(&turbine)) ;
    /* zero rpm detection*/
    if(get_WDT_RPM(&turbine) > 0)
    {
        gpio_intr_disable(RPM_PIN) ;
        turbine.WDT_RPM--;
        gpio_intr_enable(RPM_PIN) ;
    }
    else
        Reset_RPM() ;
    
    //for(int i=0; i<8; i++)
    //    turbine.EGTs[i] = turbine.EGTs[i+1] ;
    //turbine.EGTs[0] = get_EGT(&turbine) ;
}

void vTimer1sCallback( TimerHandle_t pxTimer ) //toutes les secondes
{
    // Mutex sur le temps d'allumage de l'ECU
    //static uint32_t rpm_prec = 0 ;
    //static uint32_t egt_prec = 0 ;
    
    turbine.time++ ;
    if(isEngineRun())
        turbine.time_up++ ;
    else
        turbine.time_up = 0 ;
    //ESP_LOGI(TAG,"secondes : %ld",turbine.time) ;
    //set_delta_RPM(&turbine,get_RPM(&turbine)-rpm_prec) ;
    //set_delta_EGT(&turbine,get_EGT(&turbine)-egt_prec) ;
    
    //rpm_prec = get_RPM(&turbine) ;
    //egt_prec = get_EGT(&turbine) ;
   
    /*
    printf("\nEGTs : ") ;
    for(int i = 0;i<10;i++)
    {
        printf("%ld - ",turbine.EGTs[i]) ;
    }
    printf(" ----- EGT Delta : %ld\n",get_delta_EGT(&turbine)) ;

    printf("\nRPMs : ") ;
    for(int i = 0;i<10;i++)
    {
        printf("%ld - ",turbine.RPMs[i]) ;
    }
    printf(" ----- RPM Delta : %ld\n",get_delta_RPM(&turbine)) ;
    */

    //ESP_LOGI(TAG,"RPM_sec : %ld - %ld",turbine.RPM_sec,turbine.RPM_sec*60) ;
    //turbine.RPM_sec = 0 ;
    //ESP_LOGI(TAG,"WDT_RPM : %ld",turbine.WDT_RPM) ;
    
   
    battery_check() ;
    check_errors() ;

    //ESP_LOGI(TAG,"RPM_Pulse : %ld",turbine.RPM_Pulse) ;
    //ESP_LOGI(TAG,"RPM : %ld",turbine.RPM_Pulse*60) ;
    
    turbine.RPM_Pulse = 0 ;
    //heap_trace_dump();
    //long long int Timer1 = esp_timer_get_time();
    //printf("Timer: %lld μs\n", Timer1/1000);
}

void vTimer60sCallback( TimerHandle_t pxTimer )
{
    ESP_ERROR_CHECK(esp_wifi_stop() );
    ESP_LOGI(TAG,"Wifi STOP") ;
    //vTaskDelete( xWebHandle );
    ESP_LOGI(TAG,"Server STOP") ;
    mdns_free();
    if(isEngineRun())
        horametre_save() ;
}

void create_timers(void)
{
    //xTimeMutex = xSemaphoreCreateMutex() ;
    //xRPMmutex = xSemaphoreCreateBinary() ;
    //xSemaphoreGive(xRPMmutex) ;
    //xGAZmutex = xSemaphoreCreateMutex() ;
    xTimer100ms = xTimerCreate("Timer100ms",       // Just a text name, not used by the kernel.
                            ( 100 /portTICK_PERIOD_MS ),   // The timer period in ticks.
                            pdTRUE,        // The timers will auto-reload themselves when they expire.
                            ( void * ) 3,  // Assign each timer a unique id equal to its array index.
                            vTimer100msCallback // Each timer calls the same callback when it expires.
                            );

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
    xTimer10ms = xTimerCreate("Timer10ms",       // Just a text name, not used by the kernel.
                            ( pdMS_TO_TICKS(10) ),   // The timer period in ticks.
                            pdFALSE,        // The timers will auto-reload themselves when they expire.
                            ( void * ) 2,  // Assign each timer a unique id equal to its array index.
                            vTimer10msCallback // Each timer calls the same callback when it expires.
                            );
}

void ecu_task(void * pvParameters ) 
{
    float avg_current = 0 ;
    float avg_cur = 0;
    float starter_power_perc ;
    uint8_t count_curr_sample = 0 ;
    xSemaphoreTake(ecu_task_start, portMAX_DELAY);
    ESP_LOGI(TAG, "Start ECU Task");
    starter_power_perc = turbine_config.starter_pwm_perc_start ;
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
            switch(turbine.phase_fonctionnement)
            {
                case STOP :
                        ESP_LOGI(TAG, "STOP");
                        set_power(&turbine.starter,0) ;
                    	set_power_vanne(&turbine.vanne1,0);
                        set_power_vanne(&turbine.vanne2,0);
                        set_power(&turbine.pump1,0) ;
                        set_power(&turbine.pump2,0) ;
                        set_power_glow(&turbine.glow,0) ;
                        turbine.phase_fonctionnement = WAIT ;
                        break ;
                case WAIT :
                    //ESP_LOGI(TAG, "WAIT");
                    xEventGroupClearBits(xLogEventGroup,BIT_0);
                    if(turbine.EGT > 100)
                        turbine.phase_fonctionnement = COOL ;
                    else if(turbine.position_gaz == COUPE) 
                    {
                        bool fail = 0 ;
                        for(int i=0;i<100;i++) //Attendre 1 seconde
                        {
                            if(turbine.position_gaz != COUPE)
                            {
                                fail = 1 ;
                                break;
                            }
                            vTaskDelay(10 / portTICK_PERIOD_MS);  
                        }
                    }                        
                    else if(turbine.position_gaz == PLEINGAZ)
                    {
                        // Attendre 1sec pour verifier si la position reste
                        bool fail = 0;
                        for(int i=0;i<100;i++)
                        {
                            if(turbine.position_gaz != PLEINGAZ)
                            {
                                fail = 1 ;
                                break;
                            }
                            vTaskDelay(10 / portTICK_PERIOD_MS);  
                        }
                        if(!fail){
                            turbine.phase_start = INIT ;
                            if(config_ECU.glow_type == GAS)
                                turbine.phase_fonctionnement = START ;
                            else
                                turbine.phase_fonctionnement = KEROSTART ;
                        }
                            
                    }                      
                    else if(turbine.position_gaz == MIGAZ)
                    {
                        // Attendre 1sec pour verifier si la position reste
                        bool fail = 0;
                        for(int i=0;i<100;i++)
                        {
                            if(turbine.position_gaz != MIGAZ)
                            {
                                fail = 1 ;
                                break;
                            }
                            vTaskDelay(10 / portTICK_PERIOD_MS);  
                        }
                        if(!fail)
                            turbine.phase_fonctionnement = PURGE ;
                    }
                    break;
                case START : //Demarrage GAZ
                    ESP_LOGI(TAG, "START");
                    //xSemaphoreGive(log_task_start) ;
                    xEventGroupSetBits(xLogEventGroup,BIT_0 );// Demarrage du log
                    if(battery_check()) {
                        if(0){//turbine.position_gaz == COUPE) {
                            turbine.phase_fonctionnement = STOP ;
                            ESP_LOGI(TAG, "START GAZ COUPE");
                        }
                        else if(1){//turbine.position_gaz == PLEINGAZ) {
                            ESP_LOGI(TAG, "START OK PLEIN GAZ");
                            switch(turbine.phase_start)
                            {
                                case INIT : 
                                    ESP_LOGI(TAG, "START INIT");
                                    avg_current = 0 ;
                                    count_curr_sample = 0 ;
                                    if(get_EGT(&turbine) < 50)
                                        turbine.phase_start = TESTGLOW ;
                                    else
                                        turbine.phase_fonctionnement = STOP ;
                                    starter_power_perc = turbine_config.starter_pwm_perc_start ;
                                    avg_cur = 0 ;
                                    vTaskDelay(pdMS_TO_TICKS(1000));
                                    turbine.phase_start_begin = xTaskGetTickCount() ;
                                    break ;

                                case TESTGLOW :
                                    ESP_LOGI(TAG, "START TESTGLOW");
                                    //ESP_LOGI(TAG, "Tasktick %u Taskbegin %u", xTaskGetTickCount(),turbine.phase_start_begin);
                                    if((xTaskGetTickCount() - turbine.phase_start_begin) < pdMS_TO_TICKS(TESTGLOW_TIMEOUT)) //Ajouter abort)
                                    {   
                                        if(count_curr_sample < 90 && avg_cur < 0.05 )
                                        {
                                            set_power_glow(&turbine.glow,5) ;
                                            ESP_LOGI(TAG, "SP : %u Curr : %fmA ", count_curr_sample,avg_cur);
                                            avg_current += get_glow_current(&turbine.glow) ;
                                            count_curr_sample++ ;
                                            avg_cur = avg_current/ count_curr_sample ;
                                        } else {
                                            avg_current /= count_curr_sample ;
                                            if(avg_current > 0.05){
                                                ESP_LOGI(TAG, "TEST GLOW PASS : %fmA ", avg_current);
                                                turbine.phase_start = IGNITE ;
                                                set_power_glow(&turbine.glow,0) ;
                                                turbine.phase_start_begin = xTaskGetTickCount() ;
                                            }
                                            else {
                                                ESP_LOGE(TAG, "TEST GLOW FAIL : %fmA ", avg_current) ;
                                                set_power_glow(&turbine.glow,0) ;
                                                turbine.phase_start = INIT ;
                                                turbine.phase_fonctionnement = WAIT ;
                                                add_error_msg(E_GLOW,"NO GLOW DETECTED");
                                                
                                            }
                                        }
                                    } else {
                                        ESP_LOGE(TAG, "TEST GLOW TIMEOUT") ;
                                        set_power_glow(&turbine.glow,0) ;
                                        turbine.phase_start = INIT ;
                                        turbine.phase_fonctionnement = WAIT ;
                                    }
                                    break ;
                                case IGNITE :                                    
                                    ESP_LOGI(TAG, "START IGNITE");
                                    if((xTaskGetTickCount() - turbine.phase_start_begin) < pdMS_TO_TICKS(IGNITE_TIMEOUT)
                                        && get_EGT(&turbine) < &turbine_config.max_temp) //Ajouter abort)
                                    {
                                        if(xTaskGetTickCount() - turbine.phase_start_begin < pdMS_TO_TICKS(500)) { /// Attendre 500ms
                                            // Ventilation
                                            ESP_LOGI(TAG, "IGNITE Set starter %d",turbine_config.starter_pwm_perc_start);
                                            set_power(&turbine.starter,turbine_config.starter_pwm_perc_start) ;
                                            set_power_glow(&turbine.glow,turbine_config.glow_power) ; 
                                        }  
                                        else if(xTaskGetTickCount() - turbine.phase_start_begin > pdMS_TO_TICKS(2000)){
                                            set_power(&turbine.starter,turbine_config.starter_pwm_perc_min) ;
                                            ESP_LOGI(TAG, "START Vanne Gas ON") ; 
                                            set_power_vanne(&turbine.vanne2,turbine_config.max_vanne2);
                                            ESP_LOGI(TAG, "IGNITE WAIT 150° Temp : %d",get_EGT(&turbine));
                                            if(get_EGT(&turbine) > 150) {
                                                turbine.phase_start = PREHEAT ;
                                                ESP_LOGE(TAG, "IGNITE 150° atteint");
                                                turbine.phase_start_begin = xTaskGetTickCount() ;
                                            }
                                        } else if(xTaskGetTickCount() - turbine.phase_start_begin > pdMS_TO_TICKS(IGNITE_TIMEOUT)) //10 seconde max
                                        { 
                                            ESP_LOGE(TAG, "IGNITE FAIL");
                                            turbine.phase_fonctionnement = STOP ; 
                                            turbine.phase_start = INIT ;
                                            break ;
                                        }
                                    } else {
                                        ESP_LOGE(TAG, "IGNITE TIMEOUT") ;
                                        turbine.phase_start = INIT ;
                                        turbine.phase_fonctionnement = STOP ;                                        
                                    }
                                    break ;
                                case PREHEAT :
                                    ESP_LOGI(TAG, "START PREHEAT");
                                    if((xTaskGetTickCount() - turbine.phase_start_begin) < pdMS_TO_TICKS(PREHEAT_TIMEOUT)
                                    && get_EGT(&turbine) < &turbine_config.max_temp) //Ajouter abort)
                                    {
                                        if(xTaskGetTickCount() - turbine.phase_start_begin < pdMS_TO_TICKS(3000)) { /// Attendre 2000ms que les tubes chauffent
                                            // Augementation de la température
                                            if(get_EGT(&turbine) > 200) {
                                                ESP_LOGI(TAG, "PREHEAT Set starter %d",turbine_config.starter_pwm_perc_start);
                                                starter_power_perc = starter_power_perc + 0.05 ;
                                                set_power(&turbine.starter,starter_power_perc) ;
                                            }
                                        } else if(xTaskGetTickCount() - turbine.phase_start_begin < pdMS_TO_TICKS(3100)) {
                                            if(get_EGT(&turbine) > 250) {
                                                ESP_LOGI(TAG, "PREHEAT Envoie du kerozene");
                                                set_power_vanne(&turbine.vanne1,turbine_config.max_vanne1);
                                                set_power(&turbine.pump1,turbine_config.min_pump1) ;
                                            }
                                        } else if(xTaskGetTickCount() - turbine.phase_start_begin < pdMS_TO_TICKS(6000) && get_EGT(&turbine) < 400) {
                                            if(get_EGT(&turbine) > 400) {
                                                set_power_glow(&turbine.glow,0) ;
                                                turbine.phase_start = RAMP ;
                                                ESP_LOGE(TAG, "PREHEAT KERO 400° atteint");
                                                turbine.phase_start_begin = xTaskGetTickCount() ;
                                            }
                                        }
                                     
                                    } else {
                                        ESP_LOGE(TAG, "PREHEAT TIMEOUT") ;
                                        set_power_glow(&turbine.glow,0) ;
                                        set_power_vanne(&turbine.vanne1,0);
                                        set_power_vanne(&turbine.vanne2,0);
                                        set_power(&turbine.pump1,0) ;
                                        set_power(&turbine.starter,0) ;
                                        set_power(&turbine.pump1,0) ;
                                        turbine.phase_start = INIT ;
                                        turbine.phase_fonctionnement = WAIT ;                                        
                                    }
                                    break ;
                                case RAMP :
                                    ESP_LOGI(TAG, "START RAMP");
                                    if((xTaskGetTickCount() - turbine.phase_start_begin) < pdMS_TO_TICKS(RAMP_TIMEOUT)
                                    && get_EGT(&turbine) < &turbine_config.max_temp) //Ajouter abort)
                                    {
                                        if(get_RPM(&turbine) < &turbine_config.jet_idle_rpm) {
                                            if(starter_power_perc < 75) {
                                            starter_power_perc = starter_power_perc + 0.05 ;
                                                set_power(&turbine.starter,starter_power_perc) ;
                                            }
                                            if(get_RPM(&turbine) < &turbine_config.jet_idle_rpm && get_EGT(&turbine) < (&turbine_config.max_temp -50))
                                            {
                                                //On incremente la pompe de 0.1%
                                                set_power(&turbine.pump1,get_pump_power_float(&turbine.pump1) + 0.1) ;
                                            }
                                        } else {
                                            ESP_LOGE(TAG, "Régime IDLE atteint");
                                            turbine.phase_start_begin = xTaskGetTickCount() ;
                                            turbine.phase_fonctionnement = IDLE ;
                                        }
                                    } else {
                                        ESP_LOGE(TAG, "RAMP TIMEOUT") ;
                                        turbine.phase_start = INIT ;
                                        turbine.phase_fonctionnement = STOP ;                                        
                                    }
                                    break ;
                            }                  
                        } else {
                            turbine.phase_start = INIT ;
                            turbine.phase_fonctionnement = STOP ;
                            ESP_LOGI(TAG, "START GAZ NON FULL");// Battery check
                        }
                    } else {
                        turbine.phase_start = INIT ;
                        turbine.phase_fonctionnement = STOP ;
                        ESP_LOGE(TAG, "START Batterie trop faible");// Battery check
                    }
                    break;
                    default :
                        ESP_LOGE(TAG, "START phase inconnue");// Battery check
                        turbine.phase_start = INIT ;
                        turbine.phase_fonctionnement = STOP ;
                        break;
                case KEROSTART : //DEMARRAGE KEO
                    xEventGroupSetBits(xLogEventGroup,BIT_0 );// Demarrage du log
                    if(battery_check()) {
                        if(turbine.position_gaz == COUPE) {
                            turbine.phase_fonctionnement = WAIT ;
                            ESP_LOGI(TAG, "START GAZ COUPE");
                        }
                        else if(turbine.position_gaz == PLEINGAZ) {
                            ESP_LOGI(TAG, "START OK PLEIN GAZ");
                        } else {
                            turbine.phase_start = INIT ;
                            turbine.phase_fonctionnement = STOP ;
                            ESP_LOGI(TAG, "START GAZ NON FULL");// Battery check
                        }
                    } else {
                        turbine.phase_start = INIT ;
                        turbine.phase_fonctionnement = STOP ;
                        ESP_LOGE(TAG, "START Batterie trop faible");// Battery check
                    }
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
                    if(turbine.EGT > 70){
                        set_power(&turbine.starter,50) ; //5000RPM
                        set_power(&turbine.pump1,0) ;
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                        //Attendre 1 secondes
                        set_power(&turbine.pump1,0) ;
                        //Attendre 5 secondes
                        set_power(&turbine.starter,0) ;
                        vTaskDelay(5000 / portTICK_PERIOD_MS);
                        //Attendre 5 secondes ;
                    }
                    else
                        turbine.phase_fonctionnement = WAIT ;
                    break;
            }
    }

}

void inputs_task(void * pvParameters)
{
    //unsigned long long timer,timer_prec = 0 ;

    rmt_rx_done_event_data_t rx_data;
    rmt_rx_done_event_data_t aux_rx_data;
    while(1)
    {
        
        //ESP_ERROR_CHECK( heap_trace_start(HEAP_TRACE_LEAKS) );
        //gptimer_get_raw_count(gptimer, &timer) ;
        //RPM

        //PPM Voie Gaz
        // wait for RX done signal
        if(xQueueReceive(receive_queue, &rx_data, ( TickType_t ) 2)== pdTRUE)
        {
            // Redemarre la lecture
            ESP_ERROR_CHECK(rmt_receive(rx_ppm_chan, raw_symbols, sizeof(raw_symbols), &receive_config));
            if(rx_data.received_symbols[0].level0 == 1)
            {
                //if( xSemaphoreTake(xGAZmutex,( TickType_t ) 2 ) == pdTRUE )
                    turbine.GAZ = rx_data.received_symbols[0].duration0 ;
                //xSemaphoreGive(xGAZmutex) ;
            }else{
                //if( xSemaphoreTake(xGAZmutex,( TickType_t ) 2 ) == pdTRUE )
                    turbine.GAZ = rx_data.received_symbols[0].duration1 ;
                //xSemaphoreGive(xGAZmutex) ;
            }
            /*ESP_LOGI("PPM", "Symbols : %ld",rx_data.num_symbols) ;
            for(int ii=0; ii<rx_data.num_symbols; ii++)
            {
            ESP_LOGI("PPM", "Time Up : %ld",rx_data.received_symbols[ii].duration0) ;
            ESP_LOGI("PPM", "Time Up : %ld",rx_data.received_symbols[ii].level0) ;
            ESP_LOGI("PPM", "Time Up : %ld",rx_data.received_symbols[ii].duration1) ;
            ESP_LOGI("PPM", "Time Up : %ld",rx_data.received_symbols[ii].level1) ;
            }*/
        }
        else 
        {
            turbine.GAZ = 0 ;
            add_error_msg(E_RC_SIGNAL,"Thr signal lost");

        }
         //   ESP_LOGI("Time","XQ not rx");
        //PPM Voie 2
        if(xQueueReceive(receive_queue_aux, &aux_rx_data, ( TickType_t ) 2)== pdTRUE)
        {
            // Redemarre la lecture
            ESP_ERROR_CHECK(rmt_receive(rx_ppm_aux_chan, aux_raw_symbols, sizeof(aux_raw_symbols), &receive_config));
            if(aux_rx_data.received_symbols[0].level0 == 1)
            {
                //if( xSemaphoreTake(xGAZmutex,( TickType_t ) 2 ) == pdTRUE )
                    turbine.Aux = aux_rx_data.received_symbols[0].duration0 ;
                //xSemaphoreGive(xGAZmutex) ;
            }else{
                //if( xSemaphoreTake(xGAZmutex,( TickType_t ) 2 ) == pdTRUE )
                    turbine.Aux = aux_rx_data.received_symbols[0].duration1 ;
                //xSemaphoreGive(xGAZmutex) ;
            }
            /*ESP_LOGI("PPM", "Symbols : %ld",rx_data.num_symbols) ;
            for(int ii=0; ii<rx_data.num_symbols; ii++)
            {
            ESP_LOGI("PPM", "Time Up : %ld",rx_data.received_symbols[ii].duration0) ;
            ESP_LOGI("PPM", "Time Up : %ld",rx_data.received_symbols[ii].level0) ;
            ESP_LOGI("PPM", "Time Up : %ld",rx_data.received_symbols[ii].duration1) ;
            ESP_LOGI("PPM", "Time Up : %ld",rx_data.received_symbols[ii].level1) ;
            }*/
        }
        else 
        {
            turbine.Aux = 0 ;
            add_error_msg(E_AUX_SIGNAL,"Aux signal lost");
        }    
        //ESP_LOGI("RPM", "%ldtrs/min",turbine.RPM);
        //ESP_LOGI("PPM Gaz Time", "%ldµS",turbine.GAZ);
        //ESP_LOGI("PPM Aux Time", "%ldµS",turbine.Aux);
        //ESP_LOGI("PPM Timer", "%lldµS",timer-timer_prec);
        //timer_prec = timer ;
        //vTaskDelay( 10 / portTICK_PERIOD_MS );
        //ESP_ERROR_CHECK( heap_trace_stop() );
		
    }
}

