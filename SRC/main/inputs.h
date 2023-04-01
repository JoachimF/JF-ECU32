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

#ifndef _INUTS_H_
#define _INUTS_H_

#include "driver/gptimer.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/rmt_rx.h"

#define RPM_PIN 21
#define PPM_GAZ_PIN 26
#define PPM_AUX_PIN 22

extern gptimer_handle_t gptimer ;
extern gptimer_config_t timer_config ;

extern QueueHandle_t rpm_evt_queue ;
extern QueueHandle_t gpio_gaz_evt_queue ;
extern QueueHandle_t gpio_aux_evt_queue ;
extern QueueHandle_t receive_queue ;

extern rmt_receive_config_t receive_config ;

void init_inputs(void)  ;

#endif