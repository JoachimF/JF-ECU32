#ifndef _WIFI_H
#define _WIFI_H

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

int wifi_init_ap() ;
int wifi_init_sta() ;
void initialise_mdns(void) ;

typedef struct {
    char ssid[33] ;
    char password[64] ;
    uint8_t retry ;
    uint32_t checksum ;    
}_wifi_params_t;

#endif