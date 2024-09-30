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

static const char *TAG = "Calibration";

TaskHandle_t starter_calibration_h ;

void starter_calibration() 
{
/*
    1 - Trouver le minimum PWM pour démarrer le moteur
    2 - Descendre le PWM pour trouver a quel moment le moteur s'arrête
    3 - accélérer pour avoir le régime max
*/
    float pwm_perc = 0 ;
    float pwm_perc_start,pwm_perc_min ;
    uint32_t max_rpm ;
    int error = 0 ;
    
    ESP_LOGI(TAG, "Démarrage de la calibration du démarreur");

    ESP_LOGI(TAG, "Recherche de la valeur de mise en rotation du démarreur");
    while(get_RPM(&turbine) == 0 && error == 0) // Trouver le minimum PWM pour démarrer le moteur
    {
        turbine.starter.set_power(&turbine.starter.config,pwm_perc) ;
        pwm_perc += 0.1 ;
        if(pwm_perc > 8)
            turbine.RPM = pwm_perc * 500 ; // Mode test
        else
            turbine.RPM = 0 ;
        if(pwm_perc > 25)
            error = 1 ;
        ESP_LOGI(TAG, "PWM : %f",pwm_perc);
        ESP_LOGI(TAG, "RPM : %u",get_RPM(&turbine));
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    if(error != 0)
    {
        ESP_LOGI(TAG, "Erreur le démarreur ne tourne pas : %f",pwm_perc);
        ESP_LOGI(TAG, "Fin de la tache calibration du démarreur");
        starter_calibration_h = NULL ;
        vTaskDelete( NULL );
        
    }
    pwm_perc_start = pwm_perc ;
    ESP_LOGI(TAG, "Valeur de mise en rotation du démarreur : %f",pwm_perc);
    
    ESP_LOGI(TAG, "Recherche de la valeur minimale de rotation du démarreur");
    while(get_RPM(&turbine) > 0 && error == 0) //Descendre le PWM pour trouver a quel moment le moteur s'arrête
    {
        turbine.starter.set_power(&turbine.starter.config,pwm_perc) ;
        pwm_perc -= 0.1 ;
        if(pwm_perc > 6)
            turbine.RPM = pwm_perc * 500 ; // Mode test
        else
            turbine.RPM = 0 ;

        if(pwm_perc < 1)
            error = 1 ;
        ESP_LOGI(TAG, "PWM : %f",pwm_perc);
        ESP_LOGI(TAG, "RPM : %u",get_RPM(&turbine));
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    if(error != 0)
    {
        ESP_LOGI(TAG, "Erreur le démarreur ne s'arrete pas : %f",pwm_perc);
        ESP_LOGI(TAG, "Fin de la tache calibration du démarreur");
        starter_calibration_h = NULL ;
        vTaskDelete( NULL );    
    }
    pwm_perc_min = pwm_perc ;
    ESP_LOGI(TAG, "Valeur minimale de rotation du démarreur : %f",pwm_perc);

    ESP_LOGI(TAG, "Recherche de la vitesse maximale de rotation du démarreur");
    for(float i=pwm_perc_start;i<100;i++) //accélérer pour avoir le régime max
    {
        turbine.starter.set_power(&turbine.starter.config,i) ;
        if(i > 6)
            turbine.RPM = i * 200 ; // Mode test
        else
            turbine.RPM = 0 ;
        ESP_LOGI(TAG, "PWM : %f",i) ;
        ESP_LOGI(TAG, "RPM : %u",get_RPM(&turbine));
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);
    max_rpm = get_RPM(&turbine) ;
    ESP_LOGI(TAG, "Valeur maximale de rotation du démarreur : %ld",max_rpm);

    ESP_LOGI(TAG, "Fin de la calibration du démarreur");
    
    turbine.starter.set_power(&turbine.starter.config,0) ;

    turbine_config.starter_pwm_perc_start = pwm_perc_start ; // PWM à laquelle le démarreur commence a tourner
    turbine_config.starter_pwm_perc_min = pwm_perc_min ; // PWM à laquelle le démarreur cale
    turbine_config.starter_max_rpm = max_rpm ; // RPM max du démarreur
    ESP_LOGI(TAG, "Fin de la tache calibration du démarreur");
    starter_calibration_h = NULL ;
    vTaskDelete( NULL );
}

void stop_starter_cal()
{
if( starter_calibration_h != NULL )
    {
        ESP_LOGI(TAG, "Fin de la tache calibration du démarreur");
        vTaskDelete( starter_calibration_h );
    }
    turbine.starter.set_power(&turbine.starter.config,0) ;
}

bool isCalibrated()
{
    if(turbine_config.starter_pwm_perc_start == 0 || turbine_config.starter_pwm_perc_min == 0 ||  turbine_config.starter_max_rpm == 0 )
        return 0 ;
    else
        return 1 ;
}

