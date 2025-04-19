/*  
  logs.c

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


#include "logs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <dirent.h>
#include "esp_vfs.h"

#include "sdcard.h"
#include "outputs.h"
#include "inputs.h"
#include "error.h"

#define TAG "LOG"

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

void head_logs_file(FILE *fd)
{
	if (!fd) {
       ESP_LOGI("File", "Failed to open file");
    } else {
        fprintf(fd,"FunTime;Ticks;Time;RPM;RPMDelta;RPMPeriod;EGT;EGT_delta;Pompe1;Cible Pompe1;Pompe2;Glow;Vanne1;Vanne2;Voie Gaz;Voie Aux;Vbatt;Glow current;Stater;ErrorMsg;PhaseF;PhaseS\n");
    }
}

void update_logs_file(FILE *fd,uint32_t func_time)
{   
    uint8_t minutes,secondes ;
    uint32_t ticks ;
    char errors[100] ;
    ticks = xTaskGetTickCount() ;
    get_time_up(&turbine,&secondes,&minutes,NULL) ;

/*    fd = fopen(logpath, "a");
    if (!fd) {
        ESP_LOGI("File", "Failed to read existing file : logs.txt");
    } else {*/
            get_errors(errors) ;
               /*Time;RPM;RPMDelta;RPMPeriod;EGT;EGT_Delta;Pompe1;Cible Pompe1;Pompe2;Glow;Vanne1;Vanne2;Voie Gaz;Voie Aux;Vbatt;Glow current;Stater;ErrorMsg;PhaseF;PhaseS;PhaseSB\n"*/
            fprintf(fd,"%lu;%lu;%02u:%02u;%lu;%ld;%llu;%lu;%ld;%f;Cible Pompe1;%f;%u;%u;%u;%lu;%lu;%0.2f;%0.3f;%0.2f;%s;%d;%d\n",
                    func_time,ticks,minutes,secondes,get_RPM(&turbine),get_delta_RPM(&turbine),turbine.RPM_period,get_EGT(&turbine),get_delta_EGT(&turbine),
                    get_pump_power_float(&turbine.pump1),get_pump_power_float(&turbine.pump2),get_glow_power(&turbine.glow),get_vanne_power(&turbine.vanne1),get_vanne_power(&turbine.vanne2),
                    get_gaz(&turbine),get_aux(&turbine),get_vbatt(&turbine),get_glow_current(&turbine.glow),get_starter_power(&turbine.starter),errors,turbine.phase_fonctionnement,
                    turbine.phase_start
            );
/*        fclose(fd);                                                                                            
    }*/
}

void log_task( void * pvParameters )
{
    
    EventBits_t uxBits;
    const TickType_t xTicksToWait = 100 / portTICK_PERIOD_MS;

    char entrypath[FILE_PATH_MAX];
    char entrysize[16];
    char filetmp[10] ;
    char *point;
    char *log_number ;
    uint8_t number = 0;
    uint8_t last_number = 0 ;
    uint8_t first_number = 255 ;
    char last_log[10] ;
    char first_log[10] ;
    char startwith[6] ;
    char newLog[FILE_PATH_MAX] ;
    uint32_t func_time,func_time_prec ;
    
    func_time = xTaskGetTickCount() ;

    struct dirent *entry;
    FILE *fd = NULL;
    const char *entrytype;
    
    turbine.log_started = 0 ;
    //xSemaphoreTake(log_task_start, portMAX_DELAY);
 	ESP_LOGI(TAG, "Start Logtask");

     
     // Was the event group created successfully?
    if( xLogEventGroup == NULL )
    {
        ESP_LOGE(TAG, "Fail to create Log event");
    }

    /*cherche le dernier log*/
    const size_t dirpath_len = strlen(MOUNT_POINT"/logs/");
    DIR *dir = opendir(MOUNT_POINT"/logs/");
    
    func_time = 0 ;
    while(1) {
        /* Wait for start log*/
        uxBits = xEventGroupGetBits(xLogEventGroup);
        func_time_prec = xTaskGetTickCount() ;
        if( uxBits == 1 && turbine.log_started == 0 ) // Start logging ?
        {
            //ESP_LOGI("LOG", "New Log");
            turbine.log_started = 5 ;
            while ((entry = readdir(dir)) != NULL) {
                entrytype = (entry->d_type == DT_DIR ? "directory" : "file");
                //ESP_LOGI(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);
                if(entry->d_type != DT_DIR) //Fichier
                {
                    strlcpy(startwith,entry->d_name,4) ;
                    //ESP_LOGI(TAG,"startwith : %s",startwith) ;
                    if(strcmp(startwith, "LOG") == 0) {
                        strcpy(filetmp,entry->d_name);
                        point = strstr(entry->d_name,".CSV") ;
                        if(point) { //Si la fin est trouvée
                            *point = '\0' ;
                            log_number = filetmp + 3 ;
                            number = atoi(log_number) ;
                            if(number > last_number) {
                                last_number = number ;
                                strcpy(last_log,entry->d_name);
                            }
                            if(number < first_number && number > 0) {
                                first_number = number ;
                                strcpy(first_log,entry->d_name);
                            }
                        }
                        //ESP_LOGI(TAG, "%s : number %d",entry->d_name, number);
                    }
                }
            }
            //ESP_LOGI(TAG,"Last LOG : %s - First LOG %s",last_log, first_log);
            number = last_number+1 ;
            sprintf(newLog,MOUNT_POINT"/logs/log%d.CSV",number) ;
            fd = fopen(newLog, "w");
            if (fd) {
                head_logs_file(fd) ;
            }
        }

        if( turbine.log_started >= 1 ) // Start logging
        {
            if(fd)
                update_logs_file(fd,func_time) ;
            func_time = xTaskGetTickCount() - func_time_prec ;
        }
        
        if( uxBits == 0 ) //Stop logging
        {
            if(turbine.log_started > 0 ) {
                ESP_LOGI("LOG", "End Log");
                turbine.log_started-- ;
            } else {
                if(fd){
                    //ESP_LOGI("LOG", "Close Log File");
                    fclose(fd);
                    fd = NULL ;
                }
                    
            }
        } 
        vTaskDelay( pdMS_TO_TICKS(200));
    }
    //ESP_LOGI(TAG, "finish");
	vTaskDelete(xlogHandle);
}