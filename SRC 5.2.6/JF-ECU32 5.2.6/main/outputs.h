/*  
  outputs.h

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

#ifndef _OUTPUTS_H_
#define _OUTPUTS_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

#include "jf-ecu32.h"
#include "config.h"

void init_mcpwm(void) ;
//void set_power_func_us(_PUMP_t *config ,float value) ;
//void set_power_func(_PUMP_t *config ,float value) ;
void set_power(_PUMP_t *starter ,float value) ;
void set_power_vanne(_VALVE_t *vanne, uint32_t value) ;
void set_power_glow(_VALVE_t *vanne, uint32_t value) ;

float get_pump_power_float(_PUMP_t *config) ;
float get_starter_power(_PUMP_t *config) ;

uint32_t get_pump_power_int(_PUMP_t *config) ;
uint8_t get_glow_power(_GLOW_t *config) ;
float get_glow_current(_GLOW_t *glow) ;
void set_glow_current(_GLOW_t *glow, float current) ;
uint8_t get_vanne_power(_VALVE_t *config) ;
float get_starter_power(_PUMP_t *config) ;
float get_power(_PUMP_t *starter) ;

void set_kero_pump_target(uint32_t RPM) ;

#endif