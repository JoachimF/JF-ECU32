/*  jf-ecu32.h

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
#ifndef _JF_ECU32_H_
#define _JF_ECU32_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"


typedef struct {  
  uint32_t pump[50]; //Table RPM/PWM
  uint32_t RPM[50];
  uint32_t checksum_pump;
  uint32_t checksum_RPM;
}_pump_table_t;

enum glow_types {
    GAS,KERO
};

#define   PPM     0
#define   PWM     1
#define   SBUS   1
#define   NONE    2
#define   FRSKY   0
#define   FUTABA  0
#define   MANUAL  0
#define   AUTO    1

enum yesno_type {
    NO,YES
};

typedef struct {                                 
    uint8_t input_type ;
    uint8_t glow_type ;  // GLOW ou KERO             
    uint8_t start_type ; // MANUAL ou AUTO-GAS ou AUTO-KERO
    uint8_t output_pump1 ;
    uint8_t output_pump2 ;
    uint8_t output_starter ;
    uint8_t use_telem ;
    uint8_t use_led ;
    uint8_t use_input2 ;
}_BITsconfigECU_u;





enum phases {
    WAIT,START,GLOW,KEROSTART,PREHEAT,RAMP,IDLE,PURGE,COOL
 };
/*
typedef union {                            
  uint32_t data;                           
  struct { 
    uint32_t glow_type : 1 ;  // GLOW ou KERO             
    uint32_t start_mode : 2 ; // MANUAL ou AUTO-GAS ou AUTO-KERO
  };
} _BITsconfigstart_u;*/


typedef struct {
  char name[20] ; // Nom du moteur
  uint8_t log_count ; //Numéro du log dans le fichier
  uint8_t glow_power ; // Puissance de la bougie gas ou kero
  uint32_t jet_full_power_rpm ; // trs/min plein gaz
  uint32_t jet_idle_rpm ; // trs/min ralenti
  uint32_t jet_min_rpm ; // trs/min fonctionnement
  uint16_t start_temp ; // température de démarrage apèrs préchauffe
  uint16_t max_temp ; // température max en fonctionnement
  uint8_t acceleration_delay ; // délais d'accélération
  uint8_t deceleration_delay ; // délais de décélération
  uint8_t stability_delay ; // coefficient I dans l'asservissement en vitesse
  uint16_t max_pump1 ; // puissance max de la pompe 1
  uint16_t min_pump1 ; // puissance min de la pompe 1
  uint16_t max_pump2 ; // puissance max de la pompe 1
  uint16_t min_pump2 ; // puissance min de la pompe 1
  _pump_table_t power_table;
  uint32_t checksum ;
} _configEngine_t;

typedef struct{
  uint8_t gpio_num ;
  uint8_t ledc_channel ;
}_pwm_config ;


typedef struct {
  uint16_t value;
  bool state;
  _pwm_config config ;
  void (*set_power)(_pwm_config *config,uint16_t power);
  uint16_t (*get_power)(struct GLOW_t * glow);
  void (*off)(_pwm_config *config);
} _GLOW_t ;

typedef struct {
  uint32_t target ;
  uint16_t value ;
  bool state;
  bool new_target ;
  _pwm_config config ;
  void (*set_power)( _pwm_config *config,uint16_t power);
  uint16_t (*get_power)(struct _PUMP_t * pump) ;
  void (*off)(_pwm_config *config);
} _PUMP_t;

typedef struct {
  uint16_t value;
  bool state;
  _pwm_config config ;
  void (*set_power)(_pwm_config *config,uint16_t power);
  uint16_t (*get_power)(struct _VALVE_t * valve);
  void (*on)(_pwm_config *config);
  void (*off)(_pwm_config *config);
} _VALVE_t;

typedef struct {
  uint8_t minutes ;
  uint8_t secondes ;
  uint16_t GAZ ;
  uint16_t Aux ;
  uint32_t RPM ;
  uint16_t EGT ;
  _PUMP_t pump1 ;
  _PUMP_t pump2 ;
  _GLOW_t glow ;
  _VALVE_t vanne1 ; //Vanne KEROSTART/GAZ
  _VALVE_t vanne2 ; //Vanne KERO
  uint8_t phase_fonctionnement ;
} _engine_t;

TaskHandle_t xlogHandle ;
TimerHandle_t xTimer1s ;
SemaphoreHandle_t xTimeMutex;


void init(void);
void linear_interpolation(uint32_t x0,uint32_t y0,uint32_t x1,uint32_t y1,uint32_t rpm,uint32_t *res) ;
void set_kero_pump_target(uint32_t RPM) ;
void init_power_table(void) ;
void init_random_pump(void) ;
void init_nvs(void) ;
void write_nvs_turbine(void) ;
void write_nvs_ecu(void) ;
void read_nvs(void) ;
uint32_t checksum_power_table(void) ; 
void update_curve_file(void) ;
void head_logs_file(void) ;
void log_task( void * pvParameters ) ;
void create_timers(void) ;
void vTimer1sCallback( TimerHandle_t pxTimer ) ;

#endif