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
typedef struct {  
  uint32_t pump[50]; //Table RPM/PWM
  uint32_t RPM[50];
  uint32_t checksum_pump;
  uint32_t checksum_RPM;
}_pump_table_t;

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
  };
} _BITsconfigECU_u;

enum start_modes {
    MANUAL,AUTO_GAS,AUTO_KERO
 };

enum glow_types {
    GAS,KERO
 };

enum phases {
    WAIT,START,GLOW,KEROSTART,PREHEAT,RAMP,IDLE,PURGE,COOL
 };

typedef union {                            
  uint32_t data;                           
  struct { 
    uint32_t glow_type : 1 ;  // GLOW ou KERO             
    uint32_t start_mode : 2 ; // MANUAL ou AUTO-GAS ou AUTO-KERO
  };
} _BITsconfigstart_u;


typedef struct {
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
  _PUMP_t pump1 ;
  _PUMP_t pump2 ;
  _GLOW_t glow ;
  _VALVE_t vanne1 ; //Vanne KEROSTART/GAZ
  _VALVE_t vanne2 ; //Vanne KERO
  uint8_t phase_fonctionnement ;
} _engine_t;

void init(void);
void linear_interpolation(uint32_t x0,uint32_t y0,uint32_t x1,uint32_t y1,uint32_t rpm,uint32_t *res) ;
void set_kero_pump_target(uint32_t RPM) ;
void init_power_table(void) ;
void init_random_pump(void) ;
void write_nvs(void) ;
void read_nvs(void) ;
uint32_t checksum_power_table(void) ; 