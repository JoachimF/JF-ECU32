/*
  outputs.c

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

#include "jf-ecu32.h"
#include "outputs.h"
#include "Langues/fr_FR.h"

#define TAG "OUTPUTS"

/*Configure la valeur de puissance de 0.0-100.0 en PWM, en PPM la valeur est convertie entre 1000us et 2000us*/
void set_power(_PUMP_t *peripheral, float value)
{
    if(peripheral->config.ppm_pwm == PWM) // Valeur en pourcent 0-100
    {
        ESP_LOGI("set_power", "PWM Value = %f", value);
        mcpwm_set_duty(peripheral->config.MCPWM_UNIT, peripheral->config.MCPWM_TIMER, peripheral->config.MCPWM_GEN, value);        
    }
    else    // Valeur en microsecondes 1000-2000
    {
        ESP_LOGI("set_power", "PPM Value = %f", value);
        uint32_t value_us;
        value_us = (value * 10) + 1000;
        mcpwm_set_duty_in_us(peripheral->config.MCPWM_UNIT, peripheral->config.MCPWM_TIMER, peripheral->config.MCPWM_GEN, value_us);
    }
    peripheral->value = value  ;
}

uint32_t get_pump_power_int(_PUMP_t *pump)
{
    return pump->value ;
}

float get_pump_power_float(_PUMP_t *pump)
{
    return pump->value ;
}

float get_starter_power(_PUMP_t *starter)
{
    return starter->value ;
}

/*Renvoie la valeur en pourcent de la puissance demandé en cours*/
float get_power(_PUMP_t *starter)
{
    return starter->value ;
}

/*Renvoie la valeur de la bouige de la puissance demandé en cours de 0-255*/
uint8_t get_glow_power(_GLOW_t *glow)
{
    return glow->value ;
}

/*Renvoie le courant dans la bougie */
float get_glow_current(_GLOW_t *glow)
{
    //ESP_LOGI(TAG, "Get Glow current : %0.3fA", glow->current) ;
    return glow->current ;
}

/*Met a jour le courrant de la bougie 0.0f*/
void set_glow_current(_GLOW_t *glow, float current)
{
    //ESP_LOGI(TAG, "Set Glow current : %0.3fA", current) ;
    glow->current = current ;
}


/*Renvoie la tension de la batterie 0.0f*/
float get_vbatt(_engine_t *turbine)
{
    return turbine->Vbatt ;
}

/*Met a jour la tension de la batterie 0.0f*/
void set_vbatt(_engine_t *turbine, float tension)
{
    turbine->Vbatt = tension ;
}


/*Renvoie la valeur d'une vanne de la puissance demandé en cours de 0-1024*/
uint8_t get_vanne_power(_VALVE_t *vanne)
{
    return vanne->value ;
}

/*Configure la valeur d'une vanne ou bougie de puissance de 0-1024*/
void set_power_vanne(_VALVE_t *vanne, uint32_t value)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, vanne->config.ledc_channel, value));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, vanne->config.ledc_channel));
    vanne->value = value ;
}

void set_power_glow(_VALVE_t *vanne, uint32_t value)
{
    set_power_vanne(vanne, value) ;
}

/*Configure la valeur de puissance de 0-255*/
void set_power_ledc(_ledc_config *config, uint32_t value)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, config->ledc_channel, value));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, config->ledc_channel));
    //ESP_LOGI(TAG,"ledc_channel %d ; value : %d ; pin : %d",config->ledc_channel,value,config->gpio_num);
    //ESP_LOGI(TAG,"ledc_channel %d ; value : %d ; pin : %d",config->ledc_channel,ledc_get_duty(LEDC_LOW_SPEED_MODE, config->ledc_channel),config->gpio_num);
}

ledc_timer_config_t ledc_timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .timer_num  = LEDC_TIMER_0,
    .duty_resolution = LEDC_TIMER_8_BIT,
    .freq_hz = 2000,
    .clk_cfg = LEDC_AUTO_CLK
};

static ledc_channel_config_t ledc_channel[3];
static mcpwm_config_t pwm_config[2]; //IDF 4.3.4
//mcpwm_timer_handle_t timer[2] = NULL; //IDF 5.0
//mcpwm_timer_config_t timer_config[2] = NULL ;

void init_pwm_outputs(_pwm_config *config)
{
    mcpwm_gpio_init(config->MCPWM_UNIT, config->MCPWM, config->gpio_num);   
    ESP_LOGI(TAG,"MCPWM_UNIT : %d ; MCPWM_TIMER : %d ; MCPWM_GEN : %d ; MCPWM_TIMER : %d ; pin : %d",config->MCPWM_UNIT,config->MCPWM_TIMER,config->MCPWM_GEN,config->MCPWM_TIMER,config->gpio_num); 
}




// IDF 4.3.4
void init_mcpwm(void) // IDF 4.3.4
{
    ESP_LOGI(TAG,"initializing mcpwm servo control gpio......");
    
    /* Init PUMP 1*/
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
    
    /* Init PUMP 2*/
    ESP_LOGI(TAG,"MCPWM_UNIT init outputs %s %d",ST_PUMP2,turbine.pump2.config.ppm_pwm) ;
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

    /* Init STARTER*/
    turbine.starter.config.MCPWM_TIMER = MCPWM_TIMER_2 ;
    turbine.starter.config.MCPWM_GEN = MCPWM_GEN_A ; // Test GENB au lieu de GENA crash fopen() quand duty des > 1ms
    turbine.starter.config.MCPWM = MCPWM2A ;
    if(config_ECU.output_starter == PWM)
        turbine.starter.config.MCPWM_UNIT = MCPWM_UNIT_0 ;
    else
        turbine.starter.config.MCPWM_UNIT = MCPWM_UNIT_1 ;
    
    ESP_LOGI(TAG,"MCPWM_UNIT init outputs %s %s",ST_PUMP1,turbine.pump1.config.ppm_pwm ? "PPM" : "PWM" ) ;
    init_pwm_outputs(&turbine.pump1.config) ;
    ESP_LOGI(TAG,"MCPWM_UNIT init outputs %s %s",ST_STARTER,turbine.starter.config.ppm_pwm ? "PPM" : "PWM") ;
    init_pwm_outputs(&turbine.starter.config) ;


    
    //2. initial mcpwm configuration
    ESP_LOGI(TAG,"Configuring Initial Parameters of mcpwm......");

    //Config pour PWM
    pwm_config[0].frequency = 10000;    //frequency = 10000Hz, pour les moteur DC
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
    if(config_ECU.output_starter == PWM) {
        mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_2, &pwm_config[0]);    //Configure PWM2A 10KHz (Starter)
        mcpwm_set_frequency(MCPWM_UNIT_0, MCPWM_TIMER_2,pwm_config[0].frequency);
    }
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

void set_kero_pump_target(uint32_t RPM)
{
    uint32_t rpm1=0,pump1=0,pump2=0,rpm2=0,res=0 ;
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