/*
  simulator.c

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
#include "esp_log.h"
#include "simulator.h"
#include <stdio.h>
#include "esp_system.h"
#include "inputs.h"
#include "outputs.h"

#define TAG "Simulator"

TimerHandle_t SIMxTimer100ms ;
uint32_t sim_rpm ;
uint32_t sim_egt ;
float sim_flow,sim_energy,sim_frottement,sim_heat;
uint32_t flow, heat, energy;
bool ignited ;



void vTimer100msSIMCallback( TimerHandle_t pxTimer ) //toutes les 100 millisecondes
{
    /* Simulation RPM */
    //ESP_LOGI(TAG,"Starter power : %0.1f",get_starter_power(&turbine.starter)) ;
    uint32_t rpm1=0,pump1=0,pump2=0,rpm2=0,res=0,PUMP,STARTER_PWR ;

    if(ignited) // flamme
    {
        if(turbine.EGT < 250)
        {
                ignited = 0 ;
        }
        if(get_RPM(&turbine) == 0)
        {
                if(sim_egt > 25)
                sim_egt -= 1 ;
        }
        if( get_power(&turbine.pump1) == 0 && get_vanne_power(&turbine.vanne1) == 0 )
        {
            ignited = 0 ;
        }
        if(get_RPM(&turbine) < turbine_config.starter_rpm_start) {
            if(sim_egt < 800)
                    sim_egt += 1 ;
        }
        else 
        {
            if(sim_egt < 100)
                sim_egt += 2 ;
        }

        if(get_power(&turbine.pump1) <= turbine_config.min_pump1 && get_vanne_power(&turbine.vanne1) < turbine_config.max_vanne1)
        {
            /* Pompe eteinte et vanne éteinte extinction de la flamme */
            //ESP_LOGI(TAG,"Vanne gaz fermée et pompe kero éteinte") ;
            if(sim_egt > 50)
                sim_egt -= 1 ;
            ignited = 0 ;
        }

        if(get_power(&turbine.pump1) > turbine_config.min_pump1)// Pompe ON
        {
            
            if(get_RPM(&turbine) >= turbine_config.starter_rpm_start)
            {
                /*PUMP = get_power(&turbine.pump1)*10 ;
                for(int i=0;i<50;i++)
                {
                    if(PUMP >= turbine_config.power_table.pump[i])
                    {
                        pump1 = turbine_config.power_table.pump[i] ;
                        rpm1 = turbine_config.power_table.RPM[i] ;
                    }
                    else if(PUMP <= turbine_config.power_table.pump[i])
                    {
                        pump2 = turbine_config.power_table.pump[i] ;
                        rpm2 = turbine_config.power_table.RPM[i] ;
                        i = 50 ;
                    }
                    linear_interpolation(pump1,rpm1,pump2,rpm2,PUMP,&res) ;
                }
                sim_egt += (res - sim_rpm) / 10000 ;
                ESP_LOGI(TAG,"Pump1 : %ld RPM : %ld",PUMP,res) ;
                sim_rpm = ((sim_rpm * 9)+res ) / 10 ; */  
                STARTER_PWR = get_power(&turbine.starter) ; //Starter on
                linear_interpolation(0,0,100,turbine_config.starter_max_rpm,STARTER_PWR,&res) ;
                
                sim_frottement = get_RPM(&turbine) *0.1 ;
                sim_flow = get_RPM(&turbine) * 0.007142857 ;
                sim_energy = sim_energy + get_power(&turbine.pump1)*10 - sim_flow ;
                sim_heat = get_power(&turbine.pump1)*10 - sim_flow ;
                sim_egt = turbine.EGT + sim_heat * 0.02 ;
                sim_rpm = sim_energy - sim_frottement + res / 10 ;     
                ESP_LOGD(TAG,"Mode RUN frot : %f flow : %f nrj : %f pump : %0.1f heat : %0.1f egt : %lu rpm : %lu",sim_frottement,sim_flow,sim_energy,get_power(&turbine.pump1),sim_heat,sim_egt,sim_rpm) ;
            }
            else //Moteur ne tourne pas
            {
                if(sim_egt < 800)
                    sim_egt += 2 ;
                if(get_starter_power(&turbine.starter) <= turbine_config.starter_pwm_perc_min) //Starter off
                {
                    sim_rpm = ((sim_rpm * 4) ) / 5 ;    
                }
                else
                {        
                    STARTER_PWR = get_power(&turbine.starter) ; //Starter on
                    //void linear_interpolation(uint32_t rpm1,uint32_t pump1,uint32_t rpm2,uint32_t pump2,uint32_t rpm,uint32_t *res)
                    linear_interpolation(0,0,100,turbine_config.starter_max_rpm,STARTER_PWR,&res) ;
                    ESP_LOGD(TAG,"Starter power : %ld RPM : %ld",STARTER_PWR,res) ;
                    sim_rpm = ((sim_rpm * 2+res) ) / 3 ;
                }    
            }

        }else {
            if(get_starter_power(&turbine.starter) <= turbine_config.starter_pwm_perc_min) //Starter off
            {
                sim_rpm = ((sim_rpm * 4) ) / 5 ;    
            }
            else
            {        
                STARTER_PWR = get_power(&turbine.starter) ; //Starter on
                //void linear_interpolation(uint32_t rpm1,uint32_t pump1,uint32_t rpm2,uint32_t pump2,uint32_t rpm,uint32_t *res)
                linear_interpolation(0,0,100,turbine_config.starter_max_rpm,STARTER_PWR,&res) ;
                ESP_LOGI(TAG,"Starter power : %ld RPM : %ld",STARTER_PWR,res) ;
                sim_rpm = ((sim_rpm * 2+res) ) / 3 ;
            }
            sim_energy = 0 ;    
        }

    }
    else // pas de flamme
    {
        if(get_glow_power(&turbine.glow) >= turbine_config.glow_power)
        {
            //ESP_LOGI(TAG,"Bougie allumée") ;
            
            if(get_vanne_power(&turbine.vanne1) >= turbine_config.max_vanne1)
            {
                /* Ignition */
                //ESP_LOGI(TAG,"Vanne gaz ouverte") ;
                if(sim_egt < 50 )
                    sim_egt += 1 ;
                else 
                    ignited = 1 ;
            }

        }
        
        if(get_power(&turbine.pump1) <= turbine_config.min_pump1 && get_vanne_power(&turbine.vanne1) < turbine_config.max_vanne1)
        {
            /* Pompe eteinte et vanne éteinte pas de flamme */
            //ESP_LOGI(TAG,"Vanne gaz fermée et pompe kero éteinte") ;
            if(sim_egt > 25)
                sim_egt -= get_RPM(&turbine)/10000 +1 ;
        }
        
        if(get_starter_power(&turbine.starter) <= turbine_config.starter_pwm_perc_min) //Starter off
        {
            sim_rpm = ((sim_rpm * 4) ) / 5 ;    
        }
        else
        {        
            STARTER_PWR = get_power(&turbine.starter) ; //Starter on
            //void linear_interpolation(uint32_t rpm1,uint32_t pump1,uint32_t rpm2,uint32_t pump2,uint32_t rpm,uint32_t *res)
            linear_interpolation(0,0,100,turbine_config.starter_max_rpm,STARTER_PWR,&res) ;
            //ESP_LOGI(TAG,"Starter power : %ld RPM : %ld",STARTER_PWR,res) ;
            sim_rpm = ((sim_rpm * 2+res) ) / 3 ;
        }
        sim_energy = 0 ;

    }
    if(sim_rpm > turbine_config.jet_full_power_rpm)
        sim_rpm = turbine_config.jet_full_power_rpm ;
    if(sim_rpm >= turbine_config.starter_rpm_start)
        turbine.WDT_RPM = 1 ;
    turbine.RPM = sim_rpm ;
    if(sim_egt < 10)
        sim_egt = 10 ;
    if(sim_egt > 1000)
        sim_egt = 1000 ;    
    turbine.EGT = sim_egt ;
}

void create_SIMtimers(void)
{
    SIMxTimer100ms = xTimerCreate("SIMTimer100ms",       // Just a text name, not used by the kernel.
                            ( 100 /portTICK_PERIOD_MS ),   // The timer period in ticks.
                            pdTRUE,        // The timers will auto-reload themselves when they expire.
                            ( void * ) 4,  // Assign each timer a unique id equal to its array index.
                            vTimer100msSIMCallback // Each timer calls the same callback when it expires.
                            );
    ESP_LOGI(TAG,"Timer créé") ; 

}

void start_simulator()
{
    create_SIMtimers() ;
    vTaskSuspend(task_egt_h);
    ignited = 0 ;
    sim_egt = 25 ;
    
    xTimerStart( SIMxTimer100ms, 0 ) ;
    ESP_LOGI(TAG,"Simulator started") ; 
}

void stop_simulator()
{
    xTimerDelete(SIMxTimer100ms,0) ;
    ESP_LOGI(TAG,"Simulator stopped") ;
}