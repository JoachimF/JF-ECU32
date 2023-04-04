/*
  error.c

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

#include "error.h"
#include "inputs.h"

_errors_t errors_struct ;

bool search_id(_error_sources_t id)
{
    bool res = 0 ;
    for(int i=0 ;i<errors_struct.nb_error;i++)
      {
        if(errors_struct.error[i]->id == id)
            res = 1 ;
      }
      return res ;
}

void add_error_msg(_error_sources_t id,const char *msg) 
{
    int len ;
    if(!search_id(id))
    {
        if(errors_struct.nb_error < 5)
        {
            len = strlen(msg) ;
            errors_struct.error[errors_struct.nb_error] = malloc(sizeof(_error_t)) ;
            errors_struct.error[errors_struct.nb_error]->msg = malloc(sizeof(char)*len + 1) ;
            strcpy(errors_struct.error[errors_struct.nb_error]->msg,msg) ;
            errors_struct.error[errors_struct.nb_error]->id = id ;
            gptimer_get_raw_count(gptimer, &errors_struct.error[errors_struct.nb_error]->time) ; 
            errors_struct.nb_error++ ;
        }    
    }
}

void check_errors(void) 
{
      uint64_t time ;
      gptimer_get_raw_count(gptimer, &time) ;   
      for(int i = 0 ;i<errors_struct.nb_error;i++)
      {
        if(time - errors_struct.error[i]->time > 2000)
        {
            free(errors_struct.error[i]->msg) ;
            free(errors_struct.error[i]) ;
            for(int j = i ;j<errors_struct.nb_error-1;j++)
            {
                errors_struct.error[j] = errors_struct.error[j+1] ;
            }
            errors_struct.nb_error--;
        }
      }
}

void init_errors(void)
{
    errors_struct.nb_error = 0 ;
}

void get_errors(char *output)
{
    char buff[200] ;
    if(errors_struct.nb_error > 0)
    {
        sprintf(buff,"%s",errors_struct.error[0]->msg) ;
        strcpy(output,buff) ;
        for(int i = 1;i<errors_struct.nb_error;i++)
        {
            sprintf(buff,"%s - %s",output,errors_struct.error[i]->msg) ;
            strcpy(output,buff) ;
        }
    }
    else
        output[0] = 0 ;
}