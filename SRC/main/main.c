#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"
#include "mdns.h"
#include "lwip/dns.h"
#include "driver/gpio.h"
//#include "esp_heap_trace.h"
#include <esp_ota_ops.h>
#include <esp_chip_info.h>


#include "jf-ecu32.h"
#include "http_server.h"
#include "wifi.h"
#include "nvs_ecu.h"

#define TAG	"MAIN"
#define NUM_OF_SPIN_TASKS   6
#define SPIN_ITER           500000  //Actual CPU cycles used will depend on compiler optimization
#define SPIN_TASK_PRIO      2
#define STATS_TASK_PRIO     3
#define STATS_TICKS         pdMS_TO_TICKS(1000)
#define ARRAY_SIZE_OFFSET   5   //Increase this if print_real_time_stats returns ESP_ERR_INVALID_SIZE

static char task_names[NUM_OF_SPIN_TASKS][configMAX_TASK_NAME_LEN];
static SemaphoreHandle_t sync_spin_task;
static SemaphoreHandle_t sync_stats_task;

extern SemaphoreHandle_t http_task_start;
extern SemaphoreHandle_t log_task_start;
extern SemaphoreHandle_t ecu_task_start;


static esp_err_t print_real_time_stats(TickType_t xTicksToWait)
{
    TaskStatus_t *start_array = NULL, *end_array = NULL;
    UBaseType_t start_array_size, end_array_size;
    uint32_t start_run_time, end_run_time;
    esp_err_t ret;

    //Allocate array to store current task states
	//ESP_LOGI(TAG, "Allocate array to store current task states") ;
    start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    start_array = malloc(sizeof(TaskStatus_t) * start_array_size);
    if (start_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    //Get current task states
	//ESP_LOGI(TAG, "Get current task states") ;
    start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
    if (start_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    vTaskDelay(xTicksToWait);

    //Allocate array to store tasks states post delay
	//ESP_LOGI(TAG, "Allocate array to store tasks states post delay") ;
    end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    end_array = malloc(sizeof(TaskStatus_t) * end_array_size);
    if (end_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    //Get post delay task states
	//ESP_LOGI(TAG, "Get post delay task states") ;
    end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
    if (end_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    //Calculate total_elapsed_time in units of run time stats clock period.
	//ESP_LOGI(TAG, "Calculate total_elapsed_time in units of run time stats clock period.") ;
    uint32_t total_elapsed_time = (end_run_time - start_run_time);
    if (total_elapsed_time == 0) {
        ret = ESP_ERR_INVALID_STATE;
        goto exit;
    }

    printf("| Task | Run Time | Percentage\n");
    //Match each task in start_array to those in the end_array
    for (int i = 0; i < start_array_size; i++) {
        int k = -1;
        for (int j = 0; j < end_array_size; j++) {
            if (start_array[i].xHandle == end_array[j].xHandle) {
                k = j;
                //Mark that task have been matched by overwriting their handles
                start_array[i].xHandle = NULL;
                end_array[j].xHandle = NULL;
                break;
            }
        }
        //Check if matching task found
        if (k >= 0) {
            uint32_t task_elapsed_time = end_array[k].ulRunTimeCounter - start_array[i].ulRunTimeCounter;
            uint32_t percentage_time = (task_elapsed_time * 100UL) / (total_elapsed_time * portNUM_PROCESSORS);
            printf("| %s | %ld | %ld%%\n", start_array[i].pcTaskName, task_elapsed_time, percentage_time);
        }
    }

    //Print unmatched tasks
    for (int i = 0; i < start_array_size; i++) {
        if (start_array[i].xHandle != NULL) {
            printf("| %s | Deleted\n", start_array[i].pcTaskName);
        }
    }
    for (int i = 0; i < end_array_size; i++) {
        if (end_array[i].xHandle != NULL) {
            printf("| %s | Created\n", end_array[i].pcTaskName);
        }
    }
    ret = ESP_OK;

exit:    //Common return path
    free(start_array);
    free(end_array);
    return ret;
}

static void directorySPIFFS(char * path) {
	DIR* dir = opendir(path);
	assert(dir != NULL);
	while (true) {
		struct dirent*pe = readdir(dir);
		if (!pe) break;
		ESP_LOGI(TAG,"d_name=%s/%s d_ino=%d d_type=%x", path, pe->d_name,pe->d_ino, pe->d_type);
	}
	closedir(dir);
}

esp_err_t mountSPIFFS(char * path, char * label, int max_files) {
	esp_vfs_spiffs_conf_t conf = {
		.base_path = path,
		.partition_label = label,
		.max_files = max_files,
		.format_if_mount_failed =true
	};

	// Use settings defined above toinitialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is anall-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret ==ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret== ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)",esp_err_to_name(ret));
		}
		return ret;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(conf.partition_label, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)",esp_err_to_name(ret));
	} else {
		ESP_LOGI(TAG, "Mount %s to %s success", path, label);
		ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
		directorySPIFFS(path);
	}

	return ret;
}

static void stats_task(void *arg)
{
    xSemaphoreTake(sync_stats_task, portMAX_DELAY);

    //Start all the spin tasks
    /*for (int i = 0; i < NUM_OF_SPIN_TASKS; i++) {
        xSemaphoreGive(sync_spin_task);
    }*/

    //Print real time stats periodically
    while (1) {
        printf("\n\nGetting real time stats over %ld ticks\n", STATS_TICKS);
        if (print_real_time_stats(STATS_TICKS) == ESP_OK) {
            printf("Real time stats obtained\n");
        } else {
            printf("Error getting real time stats\n");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void http_server_task(void *pvParameters);

long int timer_old,Timer1,time_ecu ;

//#define NUM_RECORDS 100
//static heap_trace_record_t trace_record[NUM_RECORDS]; // This buffer must be in internal RAM

void app_main()
{
	int res ;
	/*
	gpio_pad_select_gpio(STARTER_PIN);
	gpio_set_direction(STARTER_PIN, GPIO_MODE_OUTPUT);
	gpio_set_level(STARTER_PIN, 0);
	gpio_pad_select_gpio(PUMP1_PIN);
	gpio_set_direction(PUMP1_PIN, GPIO_MODE_OUTPUT);
	gpio_set_level(PUMP1_PIN, 0);
	gpio_pad_select_gpio(PUMP2_PIN);
	gpio_set_direction(PUMP2_PIN, GPIO_MODE_OUTPUT);
	gpio_set_level(PUMP2_PIN, 0);
	gpio_pad_select_gpio(VANNE1_PIN);
	gpio_set_direction(VANNE1_PIN, GPIO_MODE_OUTPUT);
	gpio_set_level(VANNE1_PIN, 0);
	gpio_pad_select_gpio(VANNE2_PIN);
	gpio_set_direction(VANNE2_PIN, GPIO_MODE_OUTPUT);
	gpio_set_level(VANNE2_PIN, 0);
	gpio_pad_select_gpio(GLOW_PIN);
	gpio_set_direction(GLOW_PIN, GPIO_MODE_OUTPUT);
	gpio_set_level(GLOW_PIN, 0);
	*/
	const esp_partition_t *partition = esp_ota_get_running_partition();
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
	printf("This is %s chip with %d CPU cores, WiFi%s%s, ",
			CONFIG_IDF_TARGET,
			chip_info.cores,
			(chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
			(chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

	printf("silicon revision %d, ", chip_info.revision);
	printf("Currently running partition: %s\r\n", partition->label);
	// Initialize NVS
	//ESP_ERROR_CHECK( heap_trace_init_standalone(trace_record, NUM_RECORDS) );

	ESP_LOGI(TAG, "Initializing NVS");
	init_nvs() ;
	// read wifi conf.
	ESP_LOGI(TAG, "Initializing NVS Wifi");
	read_nvs_wifi() ;
	ESP_LOGI(TAG, "Initializing Timer");
	create_timers() ;
	ESP_LOGI(TAG, "Initializing Wifi");
	// Initialize WiFi
	res = wifi_init_sta();
	//res = wifi_init_ap() ;
	ESP_LOGI(TAG, "Initializing mDNS");
	// Initialize mDNS
	initialise_mdns();

	//Init ECU
	init() ;
	start_timers() ;

	// Initialize SPIFFS
	ESP_LOGI(TAG, "Initializing SPIFFS");
	if (mountSPIFFS("/html", "storage", 6) != ESP_OK)
	{
		ESP_LOGE(TAG, "SPIFFS mount failed");
		while(1) { vTaskDelay(1); }
	}

	// Create Queue
	/*
	xQueueHttp = xQueueCreate( 10, sizeof(URL_t) );
	configASSERT( xQueueHttp );*/

	/* Get the local IP address */
	esp_netif_ip_info_t ip_info;
	if(res == 1 )
		ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info));
	else
		ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info));
	
	/*Semaphore de démarrage*/
	ESP_LOGI(TAG, "Initializing Semaphores");
	ecu_task_start = xSemaphoreCreateBinary() ;
	if(ecu_task_start == NULL)
		ESP_LOGI(TAG, "ecu_task_start Semaphore fail");
	//configASSERT(ecu_task_start) ;
	http_task_start = xSemaphoreCreateBinary() ;
	if(http_task_start == NULL)
		ESP_LOGI(TAG, "http_task_start Semaphore fail");

	//configASSERT(http_task_start) ;
	log_task_start = xSemaphoreCreateBinary() ;
	if(log_task_start == NULL)
		ESP_LOGI(TAG, "log_task_start Semaphore fail");

	//configASSERT(log_task_start) ;

	ESP_LOGI(TAG, "Initializing Task HTTP");
	char cparam0[64];
	sprintf(cparam0, IPSTR, IP2STR(&ip_info.ip));
	xTaskCreate(http_server_task, "HTTP", 1024*6, (void *)cparam0, 2, &xWebHandle);
	configASSERT( xWebHandle ) ;
	
	head_logs_file();
	
	ESP_LOGI(TAG, "Initializing Task Log");
	xTaskCreate(log_task, "LOG", 1024*6, NULL, 2, &xlogHandle);
	configASSERT( xlogHandle );
	//vTaskSuspend( xlogHandle ); 
	
	ESP_LOGI(TAG, "Initializing Task ECU");
	xTaskCreatePinnedToCore(ecu_task, "ECU", 4096, NULL, (configMAX_PRIORITIES -2 )	|( 1UL | portPRIVILEGE_BIT ), &xecuHandle,1);
	configASSERT( xecuHandle );
	//vTaskSuspend( xecuHandle ); 
	
	ESP_LOGI(TAG, "Initializing Task Inputs");
	xTaskCreatePinnedToCore(inputs_task, "INPUTS", 4096, NULL, (configMAX_PRIORITIES -1 )|( 1UL | portPRIVILEGE_BIT ), &xinputsHandle,1);
	configASSERT( xinputsHandle );

	/* Htop */
	ESP_LOGI(TAG, "Initializing Task Htop");
	sync_spin_task = xSemaphoreCreateCounting(NUM_OF_SPIN_TASKS, 0);
    sync_stats_task = xSemaphoreCreateBinary();
	xTaskCreatePinnedToCore(stats_task, "stats", 4096, NULL, STATS_TASK_PRIO, NULL, tskNO_AFFINITY);
    //xSemaphoreGive(sync_stats_task);
	/* Htop */

	/* Demarrage des taches*/
	//xSemaphoreGive(ecu_task_start) ;
	xSemaphoreGive(http_task_start) ;
	//xSemaphoreGive(log_task_start) ;
	
	vTaskDelay(2000 / portTICK_PERIOD_MS);
	turbine.EGT = 1000 ;
	turbine.GAZ = 1000 ;
	Timer1 = esp_timer_get_time();

	//int32_t time =     //printf("Timer: %lld μs\n", Timer1/1000); 
	while(1){
	for(int32_t i=0;i<1000;i++)
	{
		    turbine.EGT -- ;
            //turbine.GAZ ++ ;
			time_ecu = esp_timer_get_time() - timer_old ;
			timer_old = esp_timer_get_time();
            //ESP_LOGI(TAG,"EGT : %d ; GAZ : %d",turbine.EGT,turbine.GAZ) ;
    		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
	for(int32_t i=0 ; i<1000;i++)
	{
        //if( xSemaphoreTake(xTimeMutex,( TickType_t ) 10) == pdTRUE ) {
            turbine.EGT ++ ;
            //turbine.GAZ -- ;
            //ESP_LOGI(TAG,"EGT : %d ; GAZ : %d",turbine.EGT,turbine.GAZ) ;
        //}xSemaphoreGive(xTimeMutex) ;
    		vTaskDelay(10 / portTICK_PERIOD_MS);
	}}
	/*
	set_kero_pump_target(36000);
	vTaskDelay(3000 / portTICK_PERIOD_MS);
    set_kero_pump_target(54000);
	vTaskDelay(3000 / portTICK_PERIOD_MS);
    set_kero_pump_target(70000);
	vTaskDelay(3000 / portTICK_PERIOD_MS);
    set_kero_pump_target(91000);
	vTaskDelay(3000 / portTICK_PERIOD_MS);
    set_kero_pump_target(143000);
	vTaskDelay(3000 / portTICK_PERIOD_MS);*/
}