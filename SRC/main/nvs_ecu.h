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
#endif