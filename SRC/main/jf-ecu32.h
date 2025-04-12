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
#include "config.h"
#include <ina219.h>

#ifdef DS18B20
#include "ds18b20.h"
#include "onewire_bus_impl_rmt.h"
#endif

typedef struct _pump_table{  
  uint32_t pump[50] ; //Table RPM/PWM
  uint32_t RPM[50] ;
  uint16_t Temps[5] ;
  uint32_t checksum ;
}_pump_table_t ;

enum glow_types {
    GAS,KERO
};

#define   PPM     1
#define   PWM     0
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
#define VANNE1_PIN  12
#define VANNE2_PIN  14
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

#define TESTGLOW_TIMEOUT 2000 // X10 pour avoir des millis
#define IGNITE_TIMEOUT 10000 //
#define PREHEAT_TIMEOUT 20000 //

enum yesno_type {
    NO,YES
};

typedef struct _BITsconfigECU_{                                 
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
    WAIT,START,KEROSTART,IDLE,PURGE,COOL,STOP,RUN
 };

enum startphases {
    INIT,TESTGLOW,IGNITE,PREHEAT,RAMP
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


typedef struct _configEngine_{
  char name[21] ; // Nom du moteur
  uint8_t log_count ; //Numéro du log dans le fichier
  uint32_t horametre ; // nombre de secondes de fonctionnement du moteur
  uint8_t glow_power ; // Puissance de la bougie gas ou kero
  uint32_t jet_full_power_rpm ; // trs/min plein gaz
  uint32_t jet_idle_rpm ; // trs/min ralenti
  uint32_t jet_min_rpm ; // trs/min fonctionnement
  uint16_t start_temp ; // température de démarrage apèrs préchauffe
  uint16_t max_temp ; // température max en fonctionnement
  uint8_t acceleration_delay ; // délais d'accélération
  uint8_t deceleration_delay ; // délais de décélération
  uint8_t stability_delay ; // coefficient I dans l'asservissement en vitesse
  
  //Pumps
  uint16_t max_pump1 ; // puissance max de la pompe 1
  uint16_t min_pump1 ; // puissance min de la pompe 1
  uint16_t max_pump2 ; // puissance max de la pompe 1
  uint16_t min_pump2 ; // puissance min de la pompe 1
  
  //Vannes
  uint16_t max_vanne1 ; // puissance max de la vanne 1
  uint16_t max_vanne2 ; // puissance max de la vanne 2
  
  // Paramètre du démarreur
  uint16_t starter_rpm_start ; // vitesse du starter pour l'allumage ex : start_starter
  float starter_pwm_perc_start ; // PWM à laquelle le démarreur commence a tourner
  float starter_pwm_perc_min ; // PWM à laquelle le démarreur cale
  uint32_t starter_max_rpm ; // RPM max du démarreur
  // Paremètre batterie
  uint8_t lipo_elements ;
  float Vmin_decollage ;

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
  void (*on)(_pwm_config *config);
  void (*off)(_pwm_config *config);
  float current ;
} _GLOW_t ;

typedef struct _PUMP_{
  uint32_t target ;
  float value ;
  bool state; // On - Off
  bool new_target ;
  _pwm_config config ;
  void (*set_power)( _pwm_config *config,float power);
  void (*set_power_us)( _pwm_config *config,uint32_t power);
  uint32_t (*get_power)(struct _PUMP_ * config) ;
  void (*off)(_pwm_config *config);
} _PUMP_t;

typedef struct _VALVE_{
  uint8_t value;
  bool state;
  _ledc_config config ;
  void (*set_power)(_ledc_config *config,uint32_t power);
  uint8_t (*get_power)(struct _VALVE_ * valve);
  void (*on)(_pwm_config *config);
  void (*off)(_pwm_config *config);
} _VALVE_t;

typedef struct _engine_ {
  //uint8_t minutes ;
  //uint8_t secondes ;
  uint32_t time ;
  uint32_t time_up ;
  uint32_t GAZ ;
  uint32_t Aux ;
  uint32_t RPM ;
  int32_t RPMs[10] ;
  int32_t RPM_delta ; //RPM/secondes
  uint64_t RPM_period ; 
  uint8_t WDT_RPM ;
  uint16_t RPM_Pulse ;
  uint32_t EGT ;
  int32_t EGTs[10] ;
  int32_t EGT_delta ; //Degrées/secondes
  _PUMP_t pump1 ;
  _PUMP_t pump2 ;
  _PUMP_t starter ;
  _GLOW_t glow ;
  _VALVE_t vanne1 ; //Vanne KEROSTART/GAZ
  _VALVE_t vanne2 ; //Vanne KERO
  uint8_t phase_fonctionnement ;
  uint8_t phase_start ;
  uint32_t phase_start_begin ; //tick
  uint8_t position_gaz ;
  bool batOk ;
  float Vbatt ;
  char error_message[50] ;
  uint8_t log_started ;
  #ifdef DS18B20
  float DS18B20_temp ;
  onewire_bus_handle_t bus ;
  onewire_bus_config_t bus_config;
  onewire_bus_rmt_config_t rmt_config ;
  ds18b20_device_handle_t ds18b20s[1]; //1 seul capteur
  onewire_device_iter_handle_t iter ;
  onewire_device_t next_onewire_device ;
  int ds18b20_device_num ;
  #endif
} _engine_t;

//Taches
extern TaskHandle_t xlogHandle ;
extern TaskHandle_t xWebHandle ;
extern TaskHandle_t xecuHandle ;
extern TaskHandle_t xinputsHandle ;

//Timers
extern TimerHandle_t xTimer1s ;
extern TimerHandle_t xTimer60s ;

// Semaphores
extern SemaphoreHandle_t xTimeMutex;
extern SemaphoreHandle_t xRPMmutex;
extern SemaphoreHandle_t xGAZmutex;
extern SemaphoreHandle_t log_task_start;
extern SemaphoreHandle_t ecu_task_start;

// Events
#define BIT_0 ( 1 << 0 )
#define BIT_4 ( 1 << 4 )
extern EventGroupHandle_t xLogEventGroup;


extern _engine_t turbine ;
extern _configEngine_t turbine_config ;
extern _BITsconfigECU_u config_ECU ;

void init(void);
void linear_interpolation(uint32_t x0,uint32_t y0,uint32_t x1,uint32_t y1,uint32_t rpm,uint32_t *res) ;
void set_kero_pump_target(uint32_t RPM) ;

void update_curve_file(void) ;
void head_logs_file(FILE *fd) ;

void create_timers(void) ;
void start_timers(void) ;

//void vTimer100msCallback( TimerHandle_t pxTimer ) ;
//void vTimer1sCallback( TimerHandle_t pxTimer ) ;
//void vTimer60sCallback( TimerHandle_t pxTimer ) ;

void log_task( void * pvParameters ) ;
void ecu_task(void * pvParameters ) ;
void inputs_task(void * pvParameters) ;

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

/* Batterie */
float get_vbatt(_engine_t *turbine) ;
void  set_vbatt(_engine_t *turbine,float tension) ;
uint8_t get_conf_lipo_elements(void) ;
void set_conf_lipo_elements(uint8_t) ;
bool isBatOk(void) ;
void set_batOk(bool set) ;
float get_Vmin_decollage(void) ;


/***Gestion du temps  */
void get_time_total(_engine_t *engine, uint8_t *sec, uint8_t *min, uint8_t *heure) ;
uint16_t get_heures_total(_engine_t * engine) ;
uint8_t get_minutes_total(_engine_t * engine) ;
uint8_t get_secondes_total(_engine_t * engine) ;
void get_time_up(_engine_t *engine,uint8_t *,uint8_t *,uint8_t *) ;
uint8_t get_heures_up(_engine_t * engine) ;
uint8_t get_minutes_up(_engine_t * engine) ;
uint8_t get_secondes_up(_engine_t * engine) ;


bool isEngineRun(void) ;
void init_ds18b20(void) ;

void phase_to_str(char *status) ;


#endif
