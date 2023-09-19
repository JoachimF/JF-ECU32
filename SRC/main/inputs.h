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


#define RPM_PIN 21
#define RMT_RX_GPIO_NUM  26     /*!< GPIO number for Throttle */
#define RMT_AUX_GPIO_NUM 22     /*!< GPIO number for Aux */

#define MISO_GPIO_NUM 19     /*!< GPIO number for MISO */
#define CLK_GPIO_NUM 18     /*!< GPIO number for CLK */
#define CS_GPIO_NUM 5       /*!< GPIO number for CS */

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0)
#define HOST    HSPI_HOST
#else
#define HOST    SPI2_HOST
#endif

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

void init_inputs(void) ;
bool Get_RPM(uint32_t *rpm) ;
void Reset_RPM() ;

#endif