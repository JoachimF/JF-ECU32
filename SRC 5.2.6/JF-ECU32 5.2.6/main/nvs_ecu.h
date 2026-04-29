/*  nvs-ecu.h

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

#ifndef _NVS_ECU_H_
#define _NVS_ECU_H_

void init_nvs(void) ;
void write_nvs_wifi(void) ;
void read_nvs_wifi(void) ;

void read_nvs(void) ;
void write_nvs_ecu(void) ;
void write_nvs_turbine(void) ;

void init_power_table(void) ;
void init_random_pump(void) ;

void horametre_save(void) ;

void set_defaut_turbine(void) ;

#endif