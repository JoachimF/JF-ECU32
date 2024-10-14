/*
  calibration.c

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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "calibration.h"
#include "jf-ecu32.h"
#include "inputs.h"
#include "esp_log.h"
#include "freertos/queue.h"

#define DEBUG false

static const char *TAG = "Calibration";

TaskHandle_t starter_calibration_h ;
QueueHandle_t *Q_Calibration_Values ;

//int calib_end ;

void starter_calibration() 
{
/*
    1 - Trouver le minimum PWM pour démarrer le moteur
    2 - Descendre le PWM pour trouver a quel moment le moteur s'arrête
    3 - accélérer pour avoir le régime max
*/
    float pwm_perc = 0 ;
    float pwm_perc_start,pwm_perc_min ;
    TickType_t Ticks ;
    _chart_data_t c_datas = {
        .rpmmax = 0 ,
        .powermin = 0 ,
        .power_start = 0 ,
        .rpmstart = 0 
    };

    uint32_t max_rpm ;
    int error = 0 ;
    c_datas.end = 0 ;

    Ticks =  xTaskGetTickCount();
    

    Q_Calibration_Values = xQueueCreate(1,sizeof(_chart_data_t)) ;
    #if DEBUG
    ESP_LOGI(TAG, "Démarrage de la calibration du démarreur");
    ESP_LOGI(TAG, "Recherche de la valeur de mise en rotation du démarreur");
    #endif

    while(get_RPM(&turbine) == 0 && error == 0) // Trouver le minimum PWM pour démarrer le moteur
    {
        set_power(&turbine.starter,pwm_perc) ;
        vTaskDelay(100 / portTICK_PERIOD_MS);
        c_datas.data = get_RPM(&turbine) ;
        c_datas.label = pwm_perc ;
        c_datas.rpmstart = pwm_perc ;
        c_datas.rpmstart = c_datas.data ;
        c_datas.time = xTaskGetTickCount() - Ticks ;
        c_datas.rpm = get_RPM(&turbine) ;
        xQueueSendToFront(Q_Calibration_Values,&c_datas,0) ; 
        pwm_perc += 0.1 ; 
        if(pwm_perc > 25)
            error = 1 ;

        #if DEBUG
        /* MODE TEST*/
        if(pwm_perc > 8)
            turbine.RPM = pwm_perc * 500 ;
        else
            turbine.RPM = 0 ;
        ESP_LOGI(TAG, "PWM : %f",get_starter_power(&turbine.starter));
        ESP_LOGI(TAG, "RPM : %lu",get_RPM(&turbine));

        #endif

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    if(error != 0)
    {
        set_power(&turbine.starter,0) ;
        ESP_LOGI(TAG, "Erreur le démarreur ne tourne pas : %f",pwm_perc);
        ESP_LOGI(TAG, "Fin de la tache calibration du démarreur");
        starter_calibration_h = NULL ;
        c_datas.end = 1 ;
        xQueueSendToFront(Q_Calibration_Values,&c_datas,0) ;
        vTaskDelete( NULL );        
    }
    pwm_perc_start = pwm_perc ;
    c_datas.power_start = pwm_perc_start ;
    c_datas.rpmstart = c_datas.data ;

    #if DEBUG
    ESP_LOGI(TAG, "Valeur de mise en rotation du démarreur : %f",pwm_perc);
    ESP_LOGI(TAG, "Recherche de la valeur minimale de rotation du démarreur");
    #endif

    while(get_RPM(&turbine) > 0 && error == 0) //Descendre le PWM pour trouver a quel moment le moteur s'arrête
    {
         set_power(&turbine.starter,pwm_perc) ;
        pwm_perc -= 0.1 ;
        vTaskDelay(100 / portTICK_PERIOD_MS);
        c_datas.data = get_RPM(&turbine) ;
        c_datas.label = pwm_perc ;
        c_datas.powermin = pwm_perc ;
        c_datas.time = xTaskGetTickCount() - Ticks ;
        c_datas.rpm = get_RPM(&turbine) ;   
        xQueueSendToFront(Q_Calibration_Values,&c_datas,0) ; 
        if(pwm_perc < 1)
            error = 1 ;
        #if DEBUG
        /* MODE TEST*/
        if(pwm_perc > 6)
            turbine.RPM = pwm_perc * 500 ;
        else
            turbine.RPM = 0 ;
        /* MODE TEST*/
        ESP_LOGI(TAG, "PWM : %f",get_starter_power(&turbine.starter));
        ESP_LOGI(TAG, "RPM : %lu",get_RPM(&turbine));    
        #endif
    }
    if(error != 0)
    {
        set_power(&turbine.starter,0) ;
        ESP_LOGI(TAG, "Erreur le démarreur ne s'arrete pas : %f",pwm_perc);
        ESP_LOGI(TAG, "Fin de la tache calibration du démarreur");
        starter_calibration_h = NULL ;
        c_datas.end = 1 ;
        xQueueSendToFront(Q_Calibration_Values,&c_datas,0) ;
        vTaskDelete( NULL );    
    }
    pwm_perc_min = pwm_perc ;
    //c_datas.powermin = pwm_perc_min ;
    ESP_LOGI(TAG, "Valeur minimale de rotation du démarreur : %f",pwm_perc);

    ESP_LOGI(TAG, "Recherche de la vitesse maximale de rotation du démarreur");
    for(float i=pwm_perc_start;i<100;i++) //accélérer pour avoir le régime max
    {
        set_power(&turbine.starter,i) ;
        vTaskDelay(100 / portTICK_PERIOD_MS);
        #if DEBUG
        /* MODE TEST*/
        if(i > 6)
            turbine.RPM = i * 200 ; // Mode test
        else
            turbine.RPM = 0 ;

        ESP_LOGI(TAG, "PWM : %f",get_starter_power(&turbine.starter)) ;
        ESP_LOGI(TAG, "RPM : %lu",get_RPM(&turbine));      
        #endif
        c_datas.data = get_RPM(&turbine) ;
        c_datas.label = i ;
        c_datas.rpmmax = c_datas.data ;
        c_datas.time = xTaskGetTickCount() - Ticks ;
        c_datas.rpm = get_RPM(&turbine) ;
        xQueueSendToFront(Q_Calibration_Values,&c_datas,0) ; 
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);
    max_rpm = get_RPM(&turbine) ;
    c_datas.end = 1 ;
    //c_datas.rpmmax = max_rpm ;
    xQueueSendToFront(Q_Calibration_Values,&c_datas,0) ;
    ESP_LOGI(TAG, "Valeur maximale de rotation du démarreur : %lu",max_rpm);
    ESP_LOGI(TAG, "Fin de la calibration du démarreur");
    
    set_power(&turbine.starter,0) ;
    turbine_config.starter_pwm_perc_start = pwm_perc_start ; // PWM à laquelle le démarreur commence a tourner
    turbine_config.starter_pwm_perc_min = pwm_perc_min ; // PWM à laquelle le démarreur cale
    turbine_config.starter_max_rpm = max_rpm ; // RPM max du démarreur
    ESP_LOGI(TAG, "Fin de la tache calibration du démarreur");
    starter_calibration_h = NULL ;
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    //vQueueDelete(Q_Calibration_Values) ;
    //calib_end = 1 ;
    vTaskDelete( NULL );
}

void stop_starter_cal()
{
if( starter_calibration_h != NULL )
    {
        ESP_LOGI(TAG, "Fin de la tache calibration du démarreur");
        vTaskDelete( starter_calibration_h );
    }
    set_power(&turbine.starter,0) ;
}

bool isCalibrated()
{
    if(turbine_config.starter_pwm_perc_start == 0 || turbine_config.starter_pwm_perc_min == 0 ||  turbine_config.starter_max_rpm == 0 )
        return 0 ;
    else
        return 1 ;
}

