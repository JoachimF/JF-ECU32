/*  
  inputs.h

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

#ifndef _INPUTS_H_
#define _INPUTS_H_

#include "driver/gptimer.h"
#include "driver/timer.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/rmt_rx.h"
#include "freertos/semphr.h"
#include "jf-ecu32.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"



#define RPM_PIN 34//21
#define RMT_RX_GPIO_NUM  26     /*!< GPIO number for Throttle */
#define RMT_AUX_GPIO_NUM 35//22     /*!< GPIO number for Aux */

// SPI capteur température
#define MISO_GPIO_NUM 19     /*!< GPIO number for MISO */
#define CLK_GPIO_NUM 18     /*!< GPIO number for CLK */
#define CS_GPIO_NUM 5       /*!< GPIO number for CS */

// I2C capteur courant bougie
#define I2C_PORT I2C_NUM_0
#define I2C_ADDR INA219_ADDR_GND_GND
#define SDA_GPIO 21
#define SCL_GPIO 22


#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0)
#define HOST    HSPI_HOST
#else
#define HOST    SPI2_HOST
#endif


/*    ds18b20          */
#ifdef DS18B20
#define ECU_ONEWIRE_BUS_GPIO    4
#define ECU_ONEWIRE_MAX_DS18B20 1
#endif

/* ADC */
#define ADC_ATTEN     ADC_ATTEN_DB_12
#define ADC1_CHAN0    ADC_CHANNEL_0


typedef struct {
    int timer_group;
    int timer_idx;
    int alarm_interval;
    bool auto_reload;
} RPM_timer_info_t;



extern SemaphoreHandle_t xRPMmutex;

extern gptimer_handle_t gptimer ;
extern gptimer_config_t timer_config ;

extern QueueHandle_t rpm_evt_queue ;
extern QueueHandle_t receive_queue ;
extern QueueHandle_t receive_queue_aux ;
extern rmt_channel_handle_t rx_ppm_chan ;
extern rmt_channel_handle_t rx_ppm_aux_chan ;

extern rmt_symbol_word_t raw_symbols[64]; // 
extern rmt_symbol_word_t aux_raw_symbols[64]; // 
extern rmt_receive_config_t receive_config ;

extern TaskHandle_t task_egt_h ;
extern TaskHandle_t task_glow_current_h ;
extern SemaphoreHandle_t SEM_glow_current ;
extern SemaphoreHandle_t SEM_EGT ;


void init_inputs(void) ;
//bool Get_RPM(uint32_t *rpm) ;
void Reset_RPM() ;
uint32_t get_gaz(_engine_t * engine) ;
uint32_t get_aux(_engine_t * engine) ;
uint32_t get_RPM(_engine_t * engine) ;
uint32_t get_EGT(_engine_t * engine) ;

/*Gestion des deltas*/
void set_delta_RPM(_engine_t * engine,int32_t) ;
int32_t get_delta_RPM(_engine_t * engine) ;
void set_delta_EGT(_engine_t * engine,int32_t) ;
int32_t get_delta_EGT(_engine_t * engine) ;

uint8_t get_WDT_RPM(_engine_t * engine) ;
float get_GLOW_CURRENT(_engine_t * engine) ;
#ifdef DS18B20
void init_ds18b20(void) ;
#endif

void task_glow_current(void *pvParameter) ;
void task_egt(void *pvParameter) ;

/* Tension batterie */
bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle);
//static void adc_calibration_deinit(adc_cali_handle_t handle);
uint8_t get_lipo_elements(void) ;
bool battery_check(void) ;
#endif
