/*
  jf-ecu32.h - setting variables for Tasmota

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

typedef union {                            
  uint32_t data;                           
  struct {                                 
    uint32_t input_ppm : 1;
    uint32_t input_sbus : 1;
    uint32_t output_pump1_pwm : 1;
    uint32_t output_pump1_ppm : 1;
    uint32_t output_pump2_pwm : 1;
    uint32_t output_pump2_ppm : 1;
    uint32_t output_starter_pwm : 1;
    uint32_t output_starter_ppm : 1;
    uint32_t use_frsky_telem : 1;
    uint32_t use_futaba_telem : 1;
    uint32_t use_led : 1;
    uint32_t use_input2 : 1;
    uint32_t use_pump2 : 1;
    uint32_t use_led : 1;
  };
} BITsconfigECU;

enum start_modes {
    MANUAL,AUTO-GAS,AUTO-KERO
 };

enum glow_types {
    GAS,KERO
 };


typedef union {                            
  uint32_t data;                           
  struct { 
    uint32_t glow_type : 1 ;  // GLOW ou KERO             
    uint32_t start_mode : 2 ; // MANUAL ou AUTO-GAS ou AUTO-KERO
  };
} BITsconfigstart;


typedef typedef struct {
  uint8_t glow_power ; // Puissance de la bougie gas ou kero
  uint16_t jet_full_power_rpm ; // trs/min plein gaz
  uint16_t jet_idle_rpm ; // trs/min ralenti
  uint16_t jet_min_rpm ; // trs/min fonctionnement
  uint16_t start_temp ; // température de démarrage apèrs préchauffe
  uint16_t max_temp ; // température max en fonctionnement
  uint8_t acceleration_delay ; // délais d'accélération
  uint8_t deceleration_delay ; // délais de décélération
  uint8_t stability_delay ; // coefficient I dans l'asservissement en vitesse
  uint16_t max_pump1 ; // puissance max de la pompe 1
  uint16_t min_pump1 ; // puissance min de la pompe 1
  uint16_t max_pump2 ; // puissance max de la pompe 1
  uint16_t min_pump2 ; // puissance min de la pompe 1
  };
} configEngine;