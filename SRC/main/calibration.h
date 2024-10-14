/*
  calibration.h

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

#ifndef _CALIBRATION_H_
#define _CALIBRATION_H_

#include "freertos/queue.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct _chart_data_ {
  uint32_t data;
  float label;
  bool end ;
  float power_start ;
  uint32_t rpmstart ;
  float powermin ;
  uint32_t rpmmax ;
  uint32_t rpm ;
  uint32_t time ;
} _chart_data_t ;

extern TaskHandle_t starter_calibration_h ;
//extern int calib_end ;
extern QueueHandle_t *Q_Calibration_Values ;

void starter_calibration() ;
void stop_starter_cal() ;


#endif