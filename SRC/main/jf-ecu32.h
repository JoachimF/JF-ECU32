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

// ESP IDF 4.3.4

#ifndef _JF_ECU32_H_
#define _JF_ECU32_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"


typedef struct {  
  uint32_t pump[50] ; //Table RPM/PWM
  uint32_t RPM[50] ;
  uint16_t Temps[5] ;
  uint32_t checksum ;
}_pump_table_t ;

enum glow_types {
    GAS,KERO
};

#define   PPM     0
#define   PWM     1
#define   SBUS    1
#define   NONE    2
#define   HOTT    3
#define   FRSKY   1
#define   FUTABA  0
#define   MANUAL  0
#define   AUTO    1

#define _8BITS  8
#define _10BITS 10

#define STARTER_PIN 32
#define PUMP1_PIN   33
#define PUMP2_PIN   25
#define VANNE1_PIN  23
#define VANNE2_PIN  26
#define GLOW_PIN    27


#define SERVO_MIN_PULSEWIDTH 900  // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH 2100  // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE        -90   // Minimum angle
#define SERVO_MAX_DEGREE        90    // Maximum angle

#define SERVO_PULSE_GPIO             0        // GPIO connects to the PWM signal line
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

#define PWM_TIMER MCPWM_TIMER_0
#define PPM_TIMER MCPWM_TIMER_1

enum yesno_type {
    NO,YES
};

typedef struct {                                 
    uint8_t input_type ; //PPM - SBUS
    uint8_t glow_type ;  // GLOW ou KERO             
    uint8_t start_type ; // MANUAL ou AUTO-GAS ou AUTO-KERO
    uint8_t output_pump1 ; // PPM ou PWM
    uint8_t output_pump2 ; // PPM ou PWM OU OFF
    uint8_t output_starter ; // PPM ou PWM
    uint8_t use_telem ; // NON - FUTABA - FRSKY - HOT
    uint8_t use_led ; // OUI - NON
    uint8_t use_input2 ; // OUI - NON
    uint32_t checksum ;
}_BITsconfigECU_u;





enum phases {
    WAIT,START,GLOW,KEROSTART,PREHEAT,RAMP,IDLE,PURGE,COOL
 };

enum manche_de_gaz {
    COUPE,RALENTI,MIGAZ,PLEINGAZ
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
  char name[21] ; // Nom du moteur
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

typedef struct _pwm_config_{
  uint16_t frequency ;
  uint8_t ppm_pwm ;
  uint8_t nbits ;
  uint8_t gpio_num ;
  uint8_t MCPWM_UNIT ;
  uint8_t MCPWM ;
  uint8_t MCPWM_TIMER ;
  uint8_t MCPWM_GEN ;
}_pwm_config ;

typedef struct _ledc_config_{
  uint16_t frequency ;
  uint8_t gpio_num ;
  uint8_t ledc_channel ;
}_ledc_config ;

typedef struct _GLOW_{
  uint8_t value;
  bool state;  // On - Off
  _ledc_config config ;
  void (*set_power)(_ledc_config *config,uint32_t power);
  uint32_t (*get_power)(struct _GLOW_ * glow);
  void (*off)(_pwm_config *config);
} _GLOW_t ;

typedef struct _PUMP_{
  uint32_t target ;
  uint32_t value ;
  bool state; // On - Off
  bool new_target ;
  _pwm_config config ;
  void (*set_power)( _pwm_config *config,float power);
  void (*set_power_us)( _pwm_config *config,uint32_t power);
  uint16_t (*get_power)(struct _PUMP_ * pump) ;
  void (*off)(_pwm_config *config);
} _PUMP_t;

typedef struct _VALVE_{
  uint8_t value;
  bool state;
  _ledc_config config ;
  void (*set_power)(_ledc_config *config,uint32_t power);
  uint16_t (*get_power)(struct _VALVE_ * valve);
  void (*on)(_pwm_config *config);
  void (*off)(_pwm_config *config);
} _VALVE_t;

typedef struct _engine_ {
  uint8_t minutes ;
  uint8_t secondes ;
  uint32_t GAZ ;
  uint32_t Aux ;
  uint32_t RPM ;
  uint32_t EGT ;
  _PUMP_t pump1 ;
  _PUMP_t pump2 ;
  _PUMP_t starter ;
  _GLOW_t glow ;
  _VALVE_t vanne1 ; //Vanne KEROSTART/GAZ
  _VALVE_t vanne2 ; //Vanne KERO
  uint8_t phase_fonctionnement ;
  uint8_t position_gaz ;
} _engine_t;


void init(void);
void linear_interpolation(uint32_t x0,uint32_t y0,uint32_t x1,uint32_t y1,uint32_t rpm,uint32_t *res) ;
void set_kero_pump_target(uint32_t RPM) ;

void update_curve_file(void) ;
void head_logs_file(void) ;
void log_task( void * pvParameters ) ;
void create_timers(void) ;
void vTimer1sCallback( TimerHandle_t pxTimer ) ;
void vTimer60sCallback( TimerHandle_t pxTimer ) ;
void ecu_task(void * pvParameters ) ;
void init_mcpwm(void) ;
void set_power_func_us(_pwm_config *config ,int32_t value) ;

#endif
