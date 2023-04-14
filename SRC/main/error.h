/*  
  error.h

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

#ifndef _ERROR_H_
#define _ERROR_H_

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "driver/gptimer.h"

typedef enum _error_sources_ {
    E_K,E_RPM,E_RC_SIGNAL,E_AUX_SIGNAL,E_EGT,E_GLOW
}_error_sources_t ;

typedef struct _error_
{
    char *msg ;
    uint64_t time ;
    _error_sources_t id ;
}_error_t;

typedef struct _errors_
{
    _error_t *error[5] ;
    int nb_error ;
}_errors_t;


void add_error_msg(_error_sources_t id,const char *) ;
void check_errors(void) ;
void init_errors(void) ;
void get_errors(char *output) ;

#endif