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

#include "jf-ecu32.h"
#include "http_server.h"
#include "wifi.h"
#include "nvs_ecu.h"

#define TAG	"MAIN"

QueueHandle_t xQueueHttp;

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

void http_server_task(void *pvParameters);

void app_main()
{
	int res ;
	// Initialize NVS
	init_nvs() ;
	// read wifi conf.
	read_nvs_wifi() ;
	
	create_timers() ;
	// Initialize WiFi
	res = wifi_init_sta();
	//res = wifi_init_ap() ;

	// Initialize mDNS
	initialise_mdns();

	//Init ECU
	init() ;

	// Initialize SPIFFS
	ESP_LOGI(TAG, "Initializing SPIFFS");
	if (mountSPIFFS("/html", "storage", 6) != ESP_OK)
	{
		ESP_LOGE(TAG, "SPIFFS mount failed");
		while(1) { vTaskDelay(1); }
	}

	// Create Queue
	xQueueHttp = xQueueCreate( 10, sizeof(URL_t) );
	configASSERT( xQueueHttp );

	/* Get the local IP address */
	esp_netif_ip_info_t ip_info;
	if(res == 1 )
		ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info));
	else
		ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info));
	
	char cparam0[64];
	sprintf(cparam0, IPSTR, IP2STR(&ip_info.ip));
	xTaskCreate(http_server_task, "HTTP", 1024*6, (void *)cparam0, 2, &xWebHandle);
	configASSERT( xWebHandle ) ;
	
	head_logs_file();
	
	xTaskCreate(log_task, "LOG", 1024*6, NULL, 2, &xlogHandle);
	configASSERT( xlogHandle );
	vTaskSuspend( xlogHandle ); 
	
	xTaskCreatePinnedToCore(ecu_task, "ECU", 1024*6, NULL, ( 1UL | portPRIVILEGE_BIT ), &xecuHandle,1);
	configASSERT( xecuHandle );


	vTaskDelay(10);
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