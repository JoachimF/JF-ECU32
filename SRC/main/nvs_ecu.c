/*  nvs-ecu.c

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

#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "jf-ecu32.h"
#include "nvs_ecu.h"
#include "wifi.h"

static const char *TAG = "NVS";

nvs_handle my_handle;
extern _wifi_params_t wifi_params ;
extern _configEngine_t turbine_config ;
extern  _BITsconfigECU_u config_ECU ;

//Test du checksum retourne 1 i OK, retourne 0 si pas ok et ecrit le checkum
static int test_checksum_ecu(_BITsconfigECU_u *params,uint32_t *checksum)
{
    uint32_t check = 0;
    uint16_t len = sizeof(_BITsconfigECU_u)-sizeof(uint32_t) ;
    ESP_LOGI(TAG, "Len = %d",len);
    unsigned char *ptr = (unsigned char *)params ;
    for(int i=0;i<len;i++)
    {
        check += ptr[i]  ;
    }
    ESP_LOGI(TAG, "Checksum calculé = %d",check);
    if(check == *checksum){
        ESP_LOGI(TAG, "Checkum OK");
        return(1) ;
    } else {
        ESP_LOGI(TAG, "Checksum False");
        *checksum = check ;
        return(0) ;
    }
}

int test_checksum_table_pump(_pump_table_t *params,uint32_t *checksum)
{
    uint32_t check = 0;
    uint16_t len = sizeof(_pump_table_t)-sizeof(uint32_t) ;
    ESP_LOGI(TAG, "Len = %d",len);
    unsigned char *ptr = (unsigned char *)params ;
    for(int i=0;i<len;i++)
    {
        check += ptr[i]  ;
    }
    ESP_LOGI(TAG, "Checksum calculé = %d",check);
    if(check == *checksum){
        ESP_LOGI(TAG, "Checkum OK");
        return(1) ;
    } else {
        ESP_LOGI(TAG, "Checksum False");
        *checksum = check ;
        return(0) ;
    }
}

static int test_checksum_turbine(_configEngine_t *params,uint32_t *checksum)
{
    uint32_t check = 0;
    uint16_t len = sizeof(_configEngine_t)-sizeof(uint32_t) ;
    ESP_LOGI(TAG, "Len = %d",len);
    unsigned char *ptr = (unsigned char *)params ;
    for(int i=0;i<len;i++)
    {
        check += ptr[i]  ;
    }
    ESP_LOGI(TAG, "Checksum calculé = %d",check);
    if(check == *checksum){
        ESP_LOGI(TAG, "Checkum OK");
        return(1) ;
    } else {
        ESP_LOGI(TAG, "Checksum False");
        *checksum = check ;
        return(0) ;
    }
}

static int test_checksum_wifi(_wifi_params_t *params,uint32_t *checksum)
{
    uint32_t check = 0;
    uint16_t len = sizeof(_wifi_params_t)-sizeof(uint32_t) ;
    ESP_LOGI(TAG, "Len = %d",len);
    unsigned char *ptr = (unsigned char *)params ;
    for(int i=0;i<len;i++)
    {
        check += ptr[i]  ;
    }
    ESP_LOGI(TAG, "Checksum calculé = %d",check);
    if(check == *checksum){
        ESP_LOGI(TAG, "Checkum OK");
        return(1) ;
    } else {
        ESP_LOGI(TAG, "Checksum False");
        *checksum = check ;
        return(0) ;
    }
}

void init_random_pump(void)
{
  uint32_t interval = 20 ;
  turbine_config.power_table.checksum = 0 ;
  turbine_config.power_table.pump[0] = 50 ;
  for(int i=1;i<50;i++)
  {
        turbine_config.power_table.pump[i] = turbine_config.power_table.pump[0] + interval*i;
  }
  test_checksum_table_pump(&turbine_config.power_table,&turbine_config.power_table.checksum) ;
}

void init_power_table(void)
{
  uint32_t interval ;
  turbine_config.power_table.checksum = 0 ;
  interval = (turbine_config.jet_full_power_rpm - turbine_config.jet_idle_rpm) / 48 ;
  turbine_config.power_table.RPM[0] = turbine_config.jet_idle_rpm ;
  for(int i=1;i<49;i++)
  {
        turbine_config.power_table.RPM[i] = turbine_config.jet_idle_rpm + interval*i;
  }
  turbine_config.power_table.RPM[49] = turbine_config.jet_full_power_rpm ;
  test_checksum_table_pump(&turbine_config.power_table,&turbine_config.power_table.checksum) ;
}

static void set_defaut_turbine(void)
{
    strcpy(turbine_config.name,"Nom du moteur") ;
    turbine_config.log_count = 1 ;
    turbine_config.glow_power = 25 ;
    turbine_config.jet_full_power_rpm = 145000 ;
    turbine_config.jet_idle_rpm = 35000 ;
    turbine_config.start_temp = 100 ;
    turbine_config.max_temp = 750 ;
    turbine_config.acceleration_delay = 10 ;
    turbine_config.deceleration_delay = 12 ;
    turbine_config.stability_delay = 5 ;
    turbine_config.max_pump1 = 1024 ;
    turbine_config.min_pump1 = 0 ;
    turbine_config.max_pump2 = 512 ;
    turbine_config.jet_min_rpm = 0 ;
    init_power_table() ;
    init_random_pump() ;
    test_checksum_turbine(&turbine_config,&turbine_config.checksum) ;
}

static void set_defaut_ecu(void)
{
    config_ECU.input_type = PPM ;
    config_ECU.glow_type = GAS ;
    config_ECU.start_type = AUTO ;
    config_ECU.output_pump1 = PWM;
    config_ECU.output_pump2 = NONE ;
    config_ECU.output_starter = PPM ;
    config_ECU.use_telem = NONE ;
    config_ECU.use_input2 = NO ;
    config_ECU.use_led = NO ;
    test_checksum_ecu(&config_ECU,&config_ECU.checksum) ;
}

static void set_defaut_wifi(void)
{
    ESP_LOGI(TAG, "Taille de wifi_params %d",sizeof(_wifi_params_t));
    strcpy(wifi_params.ssid,"SSID") ;
    ESP_LOGI(TAG, "SSID : %s",wifi_params.ssid);
    strcpy(wifi_params.password,"PASSWORD") ;
    ESP_LOGI(TAG, "Password : %s",wifi_params.password);
    wifi_params.retry = 5 ;
    ESP_LOGI(TAG, "Retry : %d",wifi_params.retry);
    
    test_checksum_wifi(&wifi_params,&wifi_params.checksum) ;
}

void write_nvs_wifi(void)
{
        esp_err_t err ;//
        test_checksum_wifi(&wifi_params,&wifi_params.checksum) ;
        ESP_LOGI(TAG, "Ouverture du handle");
        err = nvs_open("storage", NVS_READWRITE, &my_handle) ;
        if (err != ESP_OK) {
            ESP_LOGE(TAG,"Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        } 
        // Write
        ESP_LOGI(TAG, "Ecriture wifi");
        size_t required_size = sizeof(_wifi_params_t);
        ESP_LOGI(TAG,"Save config to wifi_params Struct... Size : %d ",required_size);
        err = nvs_set_blob(my_handle, "configWifi", (const void*)&wifi_params, required_size);
        printf((err != ESP_OK) ? "Failed!" : "Done");
        ESP_LOGI(TAG,"Committing wifi_params in NVS ... ");
        err = nvs_commit(my_handle);
        printf((err != ESP_OK) ? "Failed!" : "Done");
        nvs_close(my_handle);
}

void read_nvs_wifi(void)
{
   esp_err_t err ;// = nvs_flash_init();
    size_t required_size;
    ESP_LOGI(TAG, "Ouverture du handle");
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } 
    ESP_LOGI(TAG, "Lecture config wifi");
    err = nvs_get_blob(my_handle, "configWifi", NULL, &required_size );
    err = nvs_get_blob(my_handle, "configWifi", (void *)&wifi_params, &required_size);
    switch (err) {
            case ESP_OK:
                ESP_LOGI(TAG,"SSID = %s", wifi_params.ssid);
                ESP_LOGI(TAG,"Password = %s", wifi_params.password);
                ESP_LOGI(TAG,"Retry = %d", wifi_params.retry);
                ESP_LOGI(TAG,"Checksum = %d", wifi_params.checksum);
                if(test_checksum_wifi(&wifi_params,&wifi_params.checksum) == 0){
                    set_defaut_wifi() ;
                    write_nvs_wifi() ;
                }
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGI(TAG, "ESP_ERR_NVS_NOT_FOUND");
                set_defaut_wifi() ;
                write_nvs_wifi() ;
                ESP_LOGI(TAG, "Ouverture du handle");
                err = nvs_open("storage", NVS_READWRITE, &my_handle);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG,"Error (%s) opening NVS handle!\n", esp_err_to_name(err));
                } 
                break;
            default :
                ESP_LOGE(TAG,"Error (%s) reading turbine_config!\n", esp_err_to_name(err));
    }
    nvs_close(my_handle);
}

void init_nvs(void)
{
    ESP_LOGI(TAG, "Init...");
    //nvs_flash_erase() ; // en cas de remise a 0
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    printf("\n");
    ESP_LOGI(TAG, "Init OK");
}

void read_nvs(void)
{
    esp_err_t err ;// = nvs_flash_init();
    size_t required_size;
    ESP_LOGI("NVS", "Ouverture du handle");
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,"Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } 
    ESP_LOGI("NVS", "Lecture config turbine");
    err = nvs_get_blob(my_handle, "config", NULL, &required_size );
    err = nvs_get_blob(my_handle, "config", (void *)&turbine_config, &required_size);
        ESP_LOGI("NVS", "Fermeture du handle");
    nvs_close(my_handle);  
    switch (err) {
            case ESP_OK:
                ESP_LOGI(TAG,"Name = %s", turbine_config.name);
                ESP_LOGI(TAG,"Log count = %d", turbine_config.log_count);
                ESP_LOGI(TAG,"glow power = %d", turbine_config.glow_power);
                ESP_LOGI(TAG,"Max rpm = %d", turbine_config.jet_full_power_rpm);
                ESP_LOGI(TAG,"idle rpm = %d", turbine_config.jet_idle_rpm);
                ESP_LOGI(TAG,"start_temp = %d", turbine_config.start_temp);
                ESP_LOGI(TAG,"max_temp = %d", turbine_config.max_temp);
                ESP_LOGI(TAG,"acceleration_delay = %d", turbine_config.acceleration_delay);
                ESP_LOGI(TAG,"deceleration_delay = %d", turbine_config.deceleration_delay);
                ESP_LOGI(TAG,"stability_delay = %d", turbine_config.stability_delay);
                ESP_LOGI(TAG,"max_pump1 = %d", turbine_config.max_pump1);
                ESP_LOGI(TAG,"min_pump1= %d", turbine_config.min_pump1);
                ESP_LOGI(TAG,"max_pump2 = %d", turbine_config.max_pump2);
                ESP_LOGI(TAG,"min_pump2= %d", turbine_config.jet_min_rpm);
                /*for(int i=0;i<50;i++)
                {
                    ESP_LOGI(TAG,"pump = %d - ", turbine_config.power_table.pump[i]);
                    ESP_LOGI(TAG,"rpm = %d\n", turbine_config.power_table.RPM[i]);
                }*/
                
                if(test_checksum_turbine(&turbine_config,&turbine_config.checksum) == 0){
                    ESP_LOGI(TAG,"Paramètre turbine érronés") ;
                    set_defaut_turbine() ;
                    write_nvs_turbine() ;
                }else if(test_checksum_table_pump(&turbine_config.power_table,&turbine_config.power_table.checksum) == 0){
                    ESP_LOGI(TAG,"Paramètre table pompe érronés") ;
                    init_power_table() ;
                    init_random_pump() ;
                    write_nvs_turbine() ;
                }
                /*ESP_LOGI(TAG,"\nChecksum2 = %d\n", checksum_power_table());
                if(checksum_power_table() != turbine_config.power_table.checksum_RPM)
                    {
                    init_power_table() ;
                    init_random_pump() ;
                    write_nvs_turbine() ;
                    }
                if(test_checksum_turbine(&turbine_config,&turbine_config.checksum) == 0){
                    set_defaut_turbine() ;
                    write_nvs_turbine() ;
                }*/
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGI(TAG,"Les valeurs config turbine ne sont pas initialisées\n");
                required_size = sizeof(turbine_config);
                ESP_LOGI("NVS", "Taille de turbine_config %d",required_size);
                set_defaut_turbine() ;
                write_nvs_turbine() ;
                ESP_LOGI("NVS", "Ouverture du handle");
                err = nvs_open("storage", NVS_READWRITE, &my_handle);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG,"Error (%s) opening NVS handle!\n", esp_err_to_name(err));
                } 
                break;
            default :
                ESP_LOGI(TAG,"Error (%s) reading turbine_config!\n", esp_err_to_name(err));
                set_defaut_turbine() ;
                write_nvs_turbine() ;
        }
    ESP_LOGI("NVS", "Ouverture du handle");
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,"Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } 
    ESP_LOGI("NVS", "Lecture config ECU");
    err = nvs_get_blob(my_handle, "configECU", NULL, &required_size );
    err = nvs_get_blob(my_handle, "configECU", (void *)&config_ECU, &required_size);
    ESP_LOGI("NVS", "Fermeture du handle");
    nvs_close(my_handle);  
    switch (err) {
            case ESP_OK:
                ESP_LOGI(TAG,"config_ECU Done");
                if(test_checksum_ecu(&config_ECU,&config_ECU.checksum) == 0){
                    set_defaut_ecu() ;
                    write_nvs_ecu() ;
                }
                ESP_LOGI("CONFIG_ECU","input : %d",config_ECU.input_type) ;
                ESP_LOGI("CONFIG_ECU","glow_type : %d",config_ECU.glow_type) ;	
                ESP_LOGI("CONFIG_ECU","start_type : %d",config_ECU.start_type) ;
                ESP_LOGI("CONFIG_ECU","output_pump1 : %d",config_ECU.output_pump1) ;
                ESP_LOGI("CONFIG_ECU","output_pump2 : %d",config_ECU.output_pump2) ;
                ESP_LOGI("CONFIG_ECU","output_starter : %d",config_ECU.output_starter) ;
                ESP_LOGI("CONFIG_ECU","use_telem : %d",config_ECU.use_telem) ;
                ESP_LOGI("CONFIG_ECU","use_input2 : %d",config_ECU.use_input2) ;
                ESP_LOGI("CONFIG_ECU","use_led : %d",config_ECU.use_led) ;
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGI(TAG,"config_ECU The value is not initialized yet!");
                set_defaut_ecu() ;
                write_nvs_ecu() ;
                break;
            default :
                set_defaut_ecu() ;
                write_nvs_ecu() ;
                ESP_LOGI(TAG,"Error (%s) reading config_ECU!", esp_err_to_name(err));
        }  
    
}

void write_nvs_turbine(void)
{
        esp_err_t err ;//
        test_checksum_turbine(&turbine_config,&turbine_config.checksum) ;
        ESP_LOGI("NVS", "Ouverture du handle");
        err = nvs_open("storage", NVS_READWRITE, &my_handle);
        if (err != ESP_OK) {
            ESP_LOGI(TAG,"Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        } 
        // Write
        ESP_LOGI("NVS", "Ecriture turbine");
        size_t required_size = sizeof(_configEngine_t);
        ESP_LOGI(TAG,"Save config to turbine_config Struct... Size : %d ",required_size);
        err = nvs_set_blob(my_handle, "config", (const void*)&turbine_config, required_size);
        printf((err != ESP_OK) ? "Failed!" : "Done");
        ESP_LOGI(TAG,"Committing turbine_config in NVS ... ");
        err = nvs_commit(my_handle);
        printf((err != ESP_OK) ? "Failed!" : "Done");
        nvs_close(my_handle);
}

void write_nvs_ecu(void)
{
        esp_err_t err ;//
        test_checksum_ecu(&config_ECU,&config_ECU.checksum) ;
        ESP_LOGI("NVS", "Ouverture du handle");
        err = nvs_open("storage", NVS_READWRITE, &my_handle);
        if (err != ESP_OK) {
            ESP_LOGI(TAG,"Error (%s) opening NVS handle!", esp_err_to_name(err));
        } 
        // Write
        ESP_LOGI("NVS", "Ecriture ECU");        
        size_t required_size = sizeof(_BITsconfigECU_u);
        ESP_LOGI(TAG,"Save config to config_ECU Struct... Size : %d ",required_size);
        err = nvs_set_blob(my_handle, "configECU", (const void*)&config_ECU, required_size);
        printf((err != ESP_OK) ? "Failed!" : "Done");
        ESP_LOGI(TAG,"Committing config_ECU in NVS ... ");
        err = nvs_commit(my_handle);
        printf((err != ESP_OK) ? "Failed!" : "Done");
        // Close
        ESP_LOGI("NVS", "Fermeture du handle");
        nvs_close(my_handle);
}