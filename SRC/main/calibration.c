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
#include "calibration.h"
#include "jf-ecu32.h"
#include "inputs.h"

void starter_calibration() 
{
/*
    1 - Trouver le minimum PWM pour démarrer le moteur
    2 - Descendre le PWM pour trouver a quel moment le moteur s'arrête
    3 - accélérer pour avoir le régime max
*/
    float pwm_perc = 0 ;
    float pwm_perc_start,pwm_perc_min ;
    u_int32_t max_rpm ;

    while(get_RPM(&turbine) == 0) // Trouver le minimum PWM pour démarrer le moteur
    {
        turbine.starter.set_power(&turbine.starter.config,pwm_perc) ;
        pwm_perc += 0.1 ;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    pwm_perc_start = pwm_perc ;

    while(get_RPM(&turbine) > 0) //Descendre le PWM pour trouver a quel moment le moteur s'arrête
    {
        turbine.starter.set_power(&turbine.starter.config,pwm_perc) ;
        pwm_perc -= 0.1 ;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    pwm_perc_min = pwm_perc ;
    
    for(float i=pwm_perc_start;i<100;i++) //accélérer pour avoir le régime max
    {
        turbine.starter.set_power(&turbine.starter.config,i) ;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);
    max_rpm = get_RPM(&turbine) ;
    
    turbine.starter.set_power(&turbine.starter.config,0) ;

    turbine_config.starter_pwm_perc_start = pwm_perc_start ; // PWM à laquelle le démarreur commence a tourner
    turbine_config.starter_pwm_perc_min = pwm_perc_min ; // PWM à laquelle le démarreur cale
    turbine_config.starter_max_rpm = max_rpm ; // RPM max du démarreur
}

bool isCalibrated()
{
    if(turbine_config.starter_pwm_perc_start == 0 || turbine_config.starter_pwm_perc_min == 0 ||  turbine_config.starter_max_rpm == 0 )
        return 0 ;
    else
        return 1 ;
}

