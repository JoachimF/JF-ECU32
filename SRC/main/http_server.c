/*  http_server.c

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

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "sys/param.h" //Fonction MIN()
#include <mbedtls/base64.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "nvs.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "cJSON.h"

//#include "esp_heap_trace.h"

#include <esp_ota_ops.h>

#include "jf-ecu32.h"
#include "inputs.h"
#include "nvs_ecu.h"
#include "http_server.h"
#include "wifi.h"
#include "error.h"

extern TimerHandle_t xTimer60s ;
static const char *TAG = "HTTP";

#define STORAGE_NAMESPACE "storage"

SemaphoreHandle_t http_task_start;

extern QueueHandle_t xQueueHttp;
//extern _BITsconfigECU_u config_ECU ;
//extern _configEngine_t turbine_config ;
extern _wifi_params_t wifi_params ;
//extern _engine_t turbine ;
//extern TaskHandle_t xlogHandle ;
//extern TaskHandle_t xWebHandle ;
//extern TaskHandle_t xecuHandle ;

esp_err_t save_key_value(char * key, char * value)
{
	nvs_handle_t my_handle;
	esp_err_t err;

	// Open
	err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
	if (err != ESP_OK) return err;

	// Write
	err = nvs_set_str(my_handle, key, value);
	if (err != ESP_OK) return err;

	// Commit written value.
	// After setting any values, nvs_commit() must be called to ensure changes are written
	// to flash storage. Implementations may write to storage at other times,
	// but this is not guaranteed.
	err = nvs_commit(my_handle);
	if (err != ESP_OK) return err;

	// Close
	nvs_close(my_handle);
	return ESP_OK;
}

esp_err_t load_key_value(char * key, char * value, size_t size)
{
	nvs_handle_t my_handle;
	esp_err_t err;

	// Open
	err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
	if (err != ESP_OK) return err;

	// Read
	size_t _size = size;
	err = nvs_get_str(my_handle, key, value, &_size);
	ESP_LOGI(TAG, "nvs_get_str err=%d", err);
	//if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
	if (err != ESP_OK) return err;
	ESP_LOGI(TAG, "err=%d key=[%s] value=[%s] _size=%d", err, key, value, _size);

	// Close
	nvs_close(my_handle);
	//return ESP_OK;
	return err;
}

int find_value(char * key, char * parameter, char * value) 
{
	//char * addr1;
	char * addr1 = strstr(parameter, key);
	if (addr1 == NULL) return 0;
	//ESP_LOGI(TAG, "addr1=%s", addr1);

	char * addr2 = addr1 + strlen(key);
	//ESP_LOGI(TAG, "addr2=[%s]", addr2);

	char * addr3 = strstr(addr2, "&");
	//ESP_LOGI(TAG, "addr3=%p", addr3);
	if (addr3 == NULL) {
		strcpy(value, addr2);
	} else {
		int length = addr3-addr2;
		//ESP_LOGI(TAG, "addr2=%p addr3=%p length=%d", addr2, addr3, length);
		strncpy(value, addr2, length);
		value[length] = 0;
	}
	//ESP_LOGI(TAG, "key=[%s] value=[%s]", key, value);
	return strlen(value);
}

static esp_err_t Text2Html(httpd_req_t *req, char * filename) {
	ESP_LOGI(TAG, "Reading %s", filename);
	FILE* fhtml = fopen(filename, "r");
	if (fhtml == NULL) {
		ESP_LOGE(TAG, "fopen fail. [%s]", filename);
		return ESP_FAIL;
	} else {
		char buffer[64];
			while(1) {
			size_t bufferSize = fread(buffer, 1, sizeof(buffer), fhtml);
			ESP_LOGD(TAG, "bufferSize=%d", bufferSize);
			if (bufferSize > 0) {
				httpd_resp_send_chunk(req, buffer, bufferSize);
			} else {
				break;
			}
		}
		fclose(fhtml);
	}
	return ESP_OK;
}

/*static esp_err_t Text2Html(httpd_req_t *req, char * filename) {
	ESP_LOGI(TAG, "Reading %s", filename);
	FILE* fhtml = fopen(filename, "r");
	if (fhtml == NULL) {
		ESP_LOGE(TAG, "fopen fail. [%s]", filename);
		return ESP_FAIL;
	} else {
		char line[64];
		while (fgets(line, sizeof(line), fhtml) != NULL) {
			size_t linelen = strlen(line);
			//remove EOL (CR or LF)
			for (int i=linelen;i>0;i--) {
				if (line[i-1] == 0x0a) {
					line[i-1] = 0;
				} else if (line[i-1] == 0x0d) {
					line[i-1] = 0;
				} else {
					break;
				}
			}
			ESP_LOGI(TAG, "line=[%s]", line);
			esp_err_t ret = httpd_resp_sendstr_chunk(req, line);
			if (ret != ESP_OK) {
				ESP_LOGE(TAG, "httpd_resp_sendstr_chunk fail %d", ret);
			}
		}
		fclose(fhtml);
	}
	return ESP_OK;
}*/

// Calculate the size after conversion to base64
// http://akabanessa.blog73.fc2.com/blog-entry-83.html
int32_t calcBase64EncodedSize(int origDataSize)
{
	// Number of blocks in 6-bit units (rounded up in 6-bit units)
	int32_t numBlocks6 = ((origDataSize * 8) + 5) / 6;
	// Number of blocks in units of 4 characters (rounded up in units of 4 characters)
	int32_t numBlocks4 = (numBlocks6 + 3) / 4;
	// Number of characters without line breaks
	int32_t numNetChars = numBlocks4 * 4;
	// Size considering line breaks every 76 characters (line breaks are "\ r \ n")
	//return numNetChars + ((numNetChars / 76) * 2);
	return numNetChars;
}

esp_err_t Image2Base64(char * imageFileName, char * base64FileName)
{
	struct stat st;
	if (stat(imageFileName, &st) != 0) {
		ESP_LOGE(TAG, "[%s] not found", imageFileName);
		return ESP_FAIL;
	}
	ESP_LOGI(TAG, "%s st.st_size=%ld", imageFileName, st.st_size);

	// Allocate image memory
	unsigned char*	image_buffer = NULL;
	size_t image_buffer_len = st.st_size;
	image_buffer = malloc(image_buffer_len);
	if (image_buffer == NULL) {
		ESP_LOGE(TAG, "malloc fail. image_buffer_len %d", image_buffer_len);
		return ESP_FAIL;
	}

	// Read image file
	FILE * fp_image = fopen(imageFileName,"rb");
	if (fp_image == NULL) {
		ESP_LOGE(TAG, "[%s] fopen fail.", imageFileName);
		free(image_buffer);
		return ESP_FAIL;
	}
	for (int i=0;i<st.st_size;i++) {
		fread(&image_buffer[i], sizeof(char), 1, fp_image);
	}
	fclose(fp_image);

	// Allocate base64 memory
	int32_t base64Size = calcBase64EncodedSize(st.st_size);
	//ESP_LOGI(TAG, "base64Size=%d", base64Size);

	unsigned char* base64_buffer = NULL;
	size_t base64_buffer_len = base64Size + 1;
	base64_buffer = malloc(base64_buffer_len);
	if (base64_buffer == NULL) {
		ESP_LOGE(TAG, "malloc fail. base64_buffer_len %d", base64_buffer_len);
		return ESP_FAIL;
	}

	// Convert from JPEG to BASE64
	size_t encord_len;
	esp_err_t ret = mbedtls_base64_encode(base64_buffer, base64_buffer_len, &encord_len, image_buffer, st.st_size);
	ESP_LOGI(TAG, "mbedtls_base64_encode=%d encord_len=%d", ret, encord_len);

	// Write Base64 file
	FILE * fp_base64 = fopen(base64FileName,"w");
	if (fp_base64 == NULL) {
		ESP_LOGE(TAG, "[%s] open fail", base64FileName);
		return ESP_FAIL;
	}
	fwrite(base64_buffer,base64_buffer_len,1,fp_base64);
	fclose(fp_base64);

	free(image_buffer);
	free(base64_buffer);
	return ESP_OK;
}

esp_err_t Image2Html(httpd_req_t *req, char * filename, char * type)
{
	FILE * fhtml = fopen(filename, "r");
	if (fhtml == NULL) {
		ESP_LOGE(TAG, "fopen fail. [%s]", filename);
		return ESP_FAIL;
	}else{
		char buffer[64];

		if (strcmp(type, "jpeg") == 0) {
			httpd_resp_sendstr_chunk(req, "<img src=\"data:image/jpeg;base64,");
		} else if (strcmp(type, "jpg") == 0) {
			httpd_resp_sendstr_chunk(req, "<img src=\"data:image/jpeg;base64,");
		} else if (strcmp(type, "png") == 0) {
			httpd_resp_sendstr_chunk(req, "<img src=\"data:image/png;base64,");
		} else {
			ESP_LOGW(TAG, "file type fail. [%s]", type);
			httpd_resp_sendstr_chunk(req, "<img src=\"data:image/png;base64,");
		}
		while(1) {
			size_t bufferSize = fread(buffer, 1, sizeof(buffer), fhtml);
			ESP_LOGD(TAG, "bufferSize=%d", bufferSize);
			if (bufferSize > 0) {
				httpd_resp_send_chunk(req, buffer, bufferSize);
			} else {
				break;
			}
		}
		fclose(fhtml);
		httpd_resp_sendstr_chunk(req, "\">");
	}
	return ESP_OK;
}

static void send_head(httpd_req_t *req)
{
	httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html>");
	Text2Html(req, "/html/head.html");
	Text2Html(req, "/html/head2.html");

	httpd_resp_sendstr_chunk(req, "<h2>");
	httpd_resp_sendstr_chunk(req, turbine_config.name);
	httpd_resp_sendstr_chunk(req, "</h2>");
}

esp_err_t update_post_handler(httpd_req_t *req)
{
	char buf[1000];
	esp_ota_handle_t ota_handle;
	int remaining = req->content_len;

	const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
	ESP_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle));

	while (remaining > 0) {
		int recv_len = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));

		// Timeout Error: Just retry
		if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
			continue;

		// Serious Error: Abort OTA
		} else if (recv_len <= 0) {
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
			return ESP_FAIL;
		}

		// Successful Upload: Flash firmware chunk
		if (esp_ota_write(ota_handle, (const void *)buf, recv_len) != ESP_OK) {
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Error");
			return ESP_FAIL;
		}

		remaining -= recv_len;
	}

	// Validate and switch to new OTA image and reboot
	if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK) {
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error");
			return ESP_FAIL;
	}

	httpd_resp_sendstr(req, "Firmware update complete, rebooting now!\n");

	vTaskDelay(500 / portTICK_PERIOD_MS);
	esp_restart();

	return ESP_OK;
}

/* HTTP post handler */
static esp_err_t root_post_handler(httpd_req_t *req)
{
    /* Destination buffer for content of HTTP POST request.
     * httpd_req_recv() accepts char* only, but content could
     * as well be any binary data (needs type casting).
     * In case of string data, null termination will be absent, and
     * content length would give length of string */
    char filepath[20];
	char content[200];
	char param[30] ;
	uint32_t param_int32 ;
	int16_t len ;

	const char *filename = get_path_from_uri(filepath, req->uri, sizeof(filepath));
    if (!filename) 
    {
        ESP_LOGE(TAG, "Filename is too long");
        // retourne une erreur 500 (Internal Server Error)
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }
	if(strcmp(filename, "/update") == 0) 
		update_post_handler(req) ;
	else {
		//ESP_LOGI(TAG,"Post received URI = %s",req->uri) ;
		//ESP_LOGI(TAG,"Post received len = %d",req->content_len) ;
		/* Truncate if content length larger than the buffer */
		size_t recv_size = MIN(req->content_len,sizeof(content));
		//ESP_LOGI(TAG,"Post len = %d",recv_size) ;
		int ret = httpd_req_recv(req, content, recv_size);
		content[recv_size-1] = 0 ;
		if (ret <= 0) {  /* 0 return value indicates connection closed */
			/* Check if timeout occurred */
			if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
				/* In case of timeout one can choose to retry calling
				* httpd_req_recv(), but to keep it simple, here we
				* respond with an HTTP 408 (Request Timeout) error */
				httpd_resp_send_408(req);
			}
			/* In case of error, returning ESP_FAIL will
			* ensure that the underlying socket is closed */
			return ESP_FAIL;
		}
		len = find_value("pwmSliderValue1=",content,param) ;
		if(len > 0)
		{
			turbine.pump1.value = atoi(param) ;
			if(turbine.pump1.config.ppm_pwm == PPM)
				set_power_func_us(&turbine.pump1,atoi(param)) ;
			else
				set_power_func(&turbine.pump1,atof(param)/20) ;
		}

		len = find_value("pwmSliderValue2=",content,param) ;
		if(len > 0)
		{
			turbine.pump2.value = atoi(param) ;
			if(turbine.pump2.config.ppm_pwm == PPM)
				set_power_func_us(&turbine.pump2,atoi(param)) ;
			else
				set_power_func(&turbine.pump2,atof(param)/20) ;
		}

		len = find_value("pwmSliderValue3=",content,param) ;
		if(len > 0)
		{
			turbine.starter.value = atoi(param) ;
			if(turbine.starter.config.ppm_pwm == PPM)
				set_power_func_us(&turbine.starter,atoi(param)) ;
			else
				set_power_func(&turbine.starter,atof(param)/20) ;
		}
		//Vanne 1
		len = find_value("pwmSliderValue4=",content,param) ;
		if(len > 0)
		{
			turbine.vanne1.value = atoi(param) ;
			turbine.vanne1.set_power(&turbine.vanne1.config,atoi(param)) ;
		}
		//Vanne 2
		len = find_value("pwmSliderValue5=",content,param) ;
		if(len > 0)
		{
			turbine.vanne2.value = atoi(param) ;
			turbine.vanne2.set_power(&turbine.vanne2.config,atoi(param)) ;
		}
		// GLOW
		len = find_value("pwmSliderValue6=",content,param) ;
		if(len > 0)
		{
			param_int32 = atoi(param) ;
			if(param_int32 > turbine_config.glow_power)
				param_int32 = turbine_config.glow_power ;
			turbine.glow.value = param_int32 ;	
			turbine.glow.set_power(&turbine.glow.config,param_int32) ;
		}
		//ESP_LOGI(TAG,"%s",content) ;
		/* Send a simple response */
		const char resp[] = "URI POST Response";
		httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
	}
    return ESP_OK;
}

/* favicon get handler */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "favicon_get_handler req->uri=[%s]", req->uri);
	return ESP_OK;
}

static esp_err_t curves_get_handler(httpd_req_t *req)
{
    FILE *fd = NULL;
    struct stat st;
	char FileName[] = "/html/curves.txt" ;

	update_curve_file() ;
	if (stat(FileName, &st) != 0) {
		ESP_LOGE(TAG, "[%s] not found", FileName);
		return ESP_FAIL;
	}
	ESP_LOGI(TAG, "%s st.st_size=%ld", FileName, st.st_size);

	char*	file_buffer = NULL;
	size_t file_buffer_len = st.st_size;
	file_buffer = malloc(file_buffer_len);
	if (file_buffer == NULL) {
		ESP_LOGE(TAG, "malloc fail. file_buffer_len %d", file_buffer_len);
		return ESP_FAIL;
	}

	ESP_LOGI(TAG, "logs_get_handler req->uri=[%s]", req->uri);
	fd = fopen(FileName, "r");
	if (!fd) {
       ESP_LOGE(TAG, "Failed to read existing file : logs.txt");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }
	for (int i=0;i<st.st_size;i++) {
		fread(&file_buffer[i], sizeof(char), 1, fd);
	}
	fclose(fd);

	ESP_LOGI(TAG, "Sending file : logs.txt...");
	ESP_LOGI(TAG, "%s",file_buffer);
	httpd_resp_set_type(req, "application/octet-stream");
	if (httpd_resp_send_chunk(req, file_buffer, st.st_size) != ESP_OK) {
                fclose(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
               return ESP_FAIL;
	}
	httpd_resp_sendstr_chunk(req, NULL);

    ESP_LOGI(TAG, "File sending complete");
	return ESP_OK;
}

static esp_err_t logs_get_handler(httpd_req_t *req)
{
    FILE *fd = NULL;
    struct stat st;
	char FileName[] = "/html/logs.txt" ;
    vTaskSuspend( xlogHandle );
	if (stat(FileName, &st) != 0) {
		ESP_LOGE(TAG, "[%s] not found", FileName);
		return ESP_FAIL;
	}
	ESP_LOGI(TAG, "%s st.st_size=%ld", FileName, st.st_size);

	char*	file_buffer = NULL;
	size_t file_buffer_len = st.st_size;
	if(file_buffer_len > 0)
	{
		file_buffer = malloc(file_buffer_len);
		if (file_buffer == NULL) {
			ESP_LOGE(TAG, "malloc fail. file_buffer_len %d", file_buffer_len);
			return ESP_FAIL;
		}

		ESP_LOGI(TAG, "logs_get_handler req->uri=[%s]", req->uri);
		fd = fopen(FileName, "r");
		if (!fd) {
		ESP_LOGE(TAG, "Failed to read existing file : logs.txt");
			/* Respond with 500 Internal Server Error */
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
			return ESP_FAIL;
		}
		for (int i=0;i<st.st_size;i++) {
			fread(&file_buffer[i], sizeof(char), 1, fd);
		}
		fclose(fd);

		ESP_LOGI(TAG, "Sending file : logs.txt...");
		//ESP_LOGI(TAG, "%s",file_buffer);
		httpd_resp_set_type(req, "application/octet-stream");
		if (httpd_resp_send_chunk(req, file_buffer, st.st_size) != ESP_OK) {
					fclose(fd);
					ESP_LOGE(TAG, "File sending failed!");
					/* Abort sending file */
					httpd_resp_sendstr_chunk(req, NULL);
					/* Respond with 500 Internal Server Error */
					httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
				return ESP_FAIL;
		}
		httpd_resp_sendstr_chunk(req, NULL);
		ESP_LOGI(TAG, "File sending complete");
	}
    vTaskResume( xlogHandle );
	return ESP_OK;
}



static esp_err_t logs(httpd_req_t *req)
{
	ESP_LOGI(TAG, "root_get_handler req->uri=[%s]", req->uri);

	// Send HTML header
	send_head(req) ;
	/*httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html>");
	Text2Html(req, "/html/head.html");

	httpd_resp_sendstr_chunk(req, "<h2>");
	httpd_resp_sendstr_chunk(req, turbine_config.name);
	httpd_resp_sendstr_chunk(req, "</h2>");*/
		
	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"logs.txt\">");
	httpd_resp_sendstr_chunk(req, "<button>Log 1</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");
	
	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"curves.txt\">");
	httpd_resp_sendstr_chunk(req, "<button>Courbe de gaz</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");
	
	httpd_resp_sendstr_chunk(req, "<p></p><form action=\"/\" method=\"get\"><button name="">Retour</button></form>") ;

	httpd_resp_sendstr_chunk(req, "<p></p>");

	Text2Html(req, "/html/footer.html");
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page

	return ESP_OK;
}

void save_configecu(httpd_req_t *req)
{
	char *buf = malloc(strlen(req->uri)+1) ;
	char param[20] ;
	int len;

	ESP_LOGI(TAG, "Sauvegarde config ECU");
//	ESP_LOGI(TAG, "Uri=%s",req->uri);
//	ESP_LOGI(TAG, "Uri len=%d",req->content_len);
	strcpy(buf,req->uri) ;
//	ESP_LOGI(TAG, "buf=%s",buf);
//	ESP_LOGI(TAG, "buf len=%d",strlen(buf));
	len = find_value("input=",buf,param) ;
	ESP_LOGI(TAG, "input=%c len=%d",*param,len);
	if(len==1) {
		if(*param == '0'){
			config_ECU.input_type = PPM ;
		}else{
			config_ECU.input_type = SBUS ;
		}
	}
	len = find_value("glow_type=",buf,param) ;
	ESP_LOGI(TAG, "glow_type=%c len=%d",*param,len);
	if(len==1) {
		if(*param == '0'){
			config_ECU.glow_type = GAS ;
		}else{
			config_ECU.glow_type = KERO ;
		}
	}
	len = find_value("start_type=",buf,param) ;
	ESP_LOGI(TAG, "start_type=%c len=%d",*param,len);
	if(len==1) {
		if(*param == '0'){
			config_ECU.start_type = MANUAL ;
		}else{
			config_ECU.start_type = AUTO ;
		}
	}
	len = find_value("output_pump1=",buf,param) ;
	ESP_LOGI(TAG, "output_pump1=%c len=%d",*param,len);
	if(len==1) {
		if(*param == '0'){
			config_ECU.output_pump1 = PPM ;
		}else{
			config_ECU.output_pump1 = PWM ;
		}
	}
	len = find_value("output_starter=",buf,param) ; 
	ESP_LOGI(TAG, "output_starter=%c len=%d",*param,len);
	if(len==1) {
		if(*param == '0'){
			config_ECU.output_starter = PPM ;
		}else{
			config_ECU.output_starter = PWM ;
		}
	}
	len = find_value("telem=",buf,param) ;
	ESP_LOGI(TAG, "telem=%c len=%d",*param,len);
	if(len==1) {
		switch(*param){
			case '0' : 
			config_ECU.use_telem = FUTABA ;
			break ;
			case '1' :
			config_ECU.use_telem = FRSKY ;
			break ;
			case '2' :
			config_ECU.use_telem = NONE ;
			break ;
		}
	}
	len = find_value("output_pump2=",buf,param) ;
	ESP_LOGI(TAG, "output_pump2=%c len=%d",*param,len);
	if(len==1) {
		switch(*param){
			case '0' :
			config_ECU.output_pump2 = PPM ;
			break;
			case '1' :
			config_ECU.output_pump2 = PWM ;
			break;
			case '2' :
			config_ECU.output_pump2 = NONE ;
			break;
		}
	}
	len = find_value("use_input2=",buf,param) ;
	ESP_LOGI(TAG, "use_input2=%c len=%d",*param,len);
	if(len == 2){
		config_ECU.use_input2 = YES ;
	}else{
		config_ECU.use_input2 = NO ;
	}
	
	len = find_value("use_led=",buf,param) ;
	ESP_LOGI(TAG, "use_led=%c len=%d",*param,len);
	if(len == 2){
		config_ECU.use_led = YES ;
	}else{
		config_ECU.use_led = NO ;
	}
	write_nvs_ecu() ;
	free(buf) ;
}
static esp_err_t configecu(httpd_req_t *req)
{
	ESP_LOGI(TAG, "config ECU req->uri=[%s]", req->uri);
	char * addr1 = strstr(req->uri, "save=");
	
	if (addr1 != NULL){
		 save_configecu(req) ; // Paramètres a sauvagarder
		 send_head(req) ;
		 httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Paramètre de l'ECU&nbsp;</b></legend><form method=\"GET\" action=\"configecu\"><p>") ;
	/*Voie des gaz*/
		 httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Redémarrage...</b></legend>") ;
		httpd_resp_sendstr_chunk(req, "</fieldset><p>") ;
		httpd_resp_sendstr_chunk(req, "</fieldset><p>") ;
		Text2Html(req, "/html/footer.html");
		httpd_resp_sendstr_chunk(req, NULL); //fin de la page
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		esp_restart() ;
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

	// Send HTML header
	send_head(req) ;
	/*httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html>");
	Text2Html(req, "/html/head.html");

	httpd_resp_sendstr_chunk(req, "<h2>");
	httpd_resp_sendstr_chunk(req, turbine_config.name);
	httpd_resp_sendstr_chunk(req, "</h2></div>");*/
	
	httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Paramètre de l'ECU&nbsp;</b></legend><form method=\"GET\" action=\"configecu\"><p>") ;
	/*Voie des gaz*/
	httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Voie des gaz</b></legend>") ;
		httpd_resp_sendstr_chunk(req, "<p><input id=\"input_ppm\" name=\"input\" type=\"radio\" value=\"0\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.input_type == PPM) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Standard</b>") ;
		
		httpd_resp_sendstr_chunk(req, "<p><input id=\"input_sbus\" name=\"input\" type=\"radio\" value=\"1\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.input_type == SBUS) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>SBUS</b>") ;
	httpd_resp_sendstr_chunk(req, "</fieldset><p>") ;
	/*Type de bougie*/
	httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Type de bougie</b></legend>") ;
		httpd_resp_sendstr_chunk(req, "<p><input id=\"glow_type_gas\" name=\"glow_type\" type=\"radio\" value=\"0\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.glow_type == GAS) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Gaz</b>") ;
		
		httpd_resp_sendstr_chunk(req, "<p><input id=\"glow_type_kero\" name=\"glow_type\" type=\"radio\" value=\"1\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.glow_type == KERO) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Kérostart</b>") ;
	httpd_resp_sendstr_chunk(req, "</fieldset><p>") ;
	/*Type de démarrage*/
	httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Démarrage</b></legend>") ;
		httpd_resp_sendstr_chunk(req, "<p><input id=\"start_type_manual\" name=\"start_type\" type=\"radio\" value=\"0\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.start_type == MANUAL ) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Manuel</b>") ;
	
		httpd_resp_sendstr_chunk(req, "<p><input id=\"start_type_auto\" name=\"start_type\" type=\"radio\" value=\"1\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.start_type == AUTO ) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Auto</b>") ;
	httpd_resp_sendstr_chunk(req, "</fieldset><p>") ;
	/*Type de pompe*/
	httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Pompe 1</b></legend>") ;
		httpd_resp_sendstr_chunk(req, "<p><input id=\"output_pump1_pwm\" name=\"output_pump1\" type=\"radio\" value=\"1\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.output_pump1 == PWM) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Moteur DC</b>") ;
		
		httpd_resp_sendstr_chunk(req, "<p><input id=\"output_pump1_ppm\" name=\"output_pump1\" type=\"radio\" value=\"0\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.output_pump1 == PPM) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Variateur</b>") ;
	httpd_resp_sendstr_chunk(req, "</fieldset><p>") ;
	/*Type de démarreur*/
	httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Démarreur</b></legend>") ;
		httpd_resp_sendstr_chunk(req, "<p><input id=\"output_starter_pwm\" name=\"output_starter\" type=\"radio\" value=\"1\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.output_starter == PWM) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Moteur DC</b>") ;
		
		httpd_resp_sendstr_chunk(req, "<p><input id=\"output_starter_ppm\" name=\"output_starter\" type=\"radio\" value=\"0\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.output_starter == PPM) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Variateur</b>") ;
	httpd_resp_sendstr_chunk(req, "</fieldset><p>") ;
	/*Type de télémétrie*/
	httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Télémétrie</b></legend>") ;
		httpd_resp_sendstr_chunk(req, "<p><input id=\"futaba_telem\" name=\"telem\" type=\"radio\" value=\"0\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.use_telem == FUTABA ) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Désactivée</b>") ;
	
		httpd_resp_sendstr_chunk(req, "<p><input id=\"use_frsky_telem\" name=\"telem\" type=\"radio\" value=\"1\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.use_telem == FRSKY ) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>FrSky</b>") ;

		httpd_resp_sendstr_chunk(req, "<p><input id=\"use_hott_telem\" name=\"telem\" type=\"radio\" value=\"3\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.use_telem == HOTT ) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>FrSky</b>") ;
		
		httpd_resp_sendstr_chunk(req, "<p><input id=\"no_telem\" name=\"telem\" type=\"radio\" value=\"2\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.use_telem == NONE ) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Futaba</b>") ;
	httpd_resp_sendstr_chunk(req, "</fieldset><p>") ;
	/*Pompe 2*/
	httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Pompe 2</b></legend>") ;
		httpd_resp_sendstr_chunk(req, "<p><input id=\"no_pump2\" name=\"output_pump2\" type=\"radio\" value=\"0\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.output_pump2 == PPM) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Variateur</b>") ;

		httpd_resp_sendstr_chunk(req, "<p><input id=\"output_pump2_pwm\" name=\"output_pump2\" type=\"radio\" value=\"1\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.output_pump2 == PWM) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Moteur DC</b>") ;
		
		httpd_resp_sendstr_chunk(req, "<p><input id=\"output_pump2_ppm\" name=\"output_pump2\" type=\"radio\" value=\"2\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.output_pump2 == NONE) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Désactivée</b>") ;
	httpd_resp_sendstr_chunk(req, "</fieldset><p>") ;
	/*Voie aux*/
	httpd_resp_sendstr_chunk(req, "<p><input id=\"use_input2\" type=\"checkbox\"") ;
	httpd_resp_sendstr_chunk(req, (config_ECU.use_input2 == YES) ? "checked=\"\"" :" " ) ;
	httpd_resp_sendstr_chunk(req, " name=\"use_input2\"><b>Voie 2 Activée</b>") ;
	/*Leds*/
	httpd_resp_sendstr_chunk(req, "<p><input id=\"use_led\" type=\"checkbox\" " ) ;
	httpd_resp_sendstr_chunk(req, (config_ECU.use_led == YES) ? "checked=\"\"" :" " ) ;
	httpd_resp_sendstr_chunk(req, " name=\"use_led\"><b>Leds Activées</b>") ;
	/* Save*/
	httpd_resp_sendstr_chunk(req, "</p><p><button name=\"save\" type=\"submit\" class=\"button bgrn\">Sauvegarde</button>") ;
	httpd_resp_sendstr_chunk(req, "</form></fieldset><p>") ;
	
	/*Retour*/	
	httpd_resp_sendstr_chunk(req, "<p></p><form action=\"/\" method=\"get\"><button name="">Retour</button></form>") ;

	Text2Html(req, "/html/footer.html");
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page

	return ESP_OK;
}

void save_configturbine(httpd_req_t *req)
{
	char *buf = malloc(strlen(req->uri)+1) ;
	char param[30] ;
	int len;

	strcpy(buf,req->uri) ;
	ESP_LOGI(TAG, "Sauvegarde config turbine");
	/*Nom*/
	len = find_value("name=",buf,param) ;
	ESP_LOGI(TAG, "name=%c len=%d",*param,len);
	if(len>1) {
		strcpy(turbine_config.name,param) ;
	}
	/*Puissance bougie*/
	len = find_value("glow_power=",buf,param) ;
	ESP_LOGI(TAG, "glow_power=%c len=%d",*param,len);
	if(len>1) {
		turbine_config.glow_power = atoi(param) ;		
	}
	/*Max RPM*/
	len = find_value("jet_full_power_rpm=",buf,param) ;
	ESP_LOGI(TAG, "jet_full_power_rpm=%c len=%d",*param,len);
	if(len>1) {
		turbine_config.jet_full_power_rpm = atoi(param) ;		
	}
	/*RPM ralenti*/
	len = find_value("jet_idle_rpm=",buf,param) ;
	ESP_LOGI(TAG, "jet_idle_rpm=%c len=%d",*param,len);
	if(len>1) {
		turbine_config.jet_idle_rpm = atoi(param) ;		
	}
	/*RPM mini*/
	len = find_value("jet_min_rpm=",buf,param) ;
	ESP_LOGI(TAG, "jet_min_rpm=%c len=%d",*param,len);
	if(len>1) {
		turbine_config.jet_min_rpm = atoi(param) ;		
	}
	/*Start Temp.*/
	len = find_value("start_temp=",buf,param) ;
	ESP_LOGI(TAG, "start_temp=%c len=%d",*param,len);
	if(len>1) {
		turbine_config.start_temp = atoi(param) ;		
	}
	/*Max Temp.*/
	len = find_value("max_temp=",buf,param) ;
	ESP_LOGI(TAG, "max_temp=%c len=%d",*param,len);
	if(len>1) {
		turbine_config.max_temp = atoi(param) ;		
	}
	/*Délai de stabilité*/
	len = find_value("stability_delay=",buf,param) ;
	ESP_LOGI(TAG, "stability_delay=%c len=%d",*param,len);
	if(len>1) {
		turbine_config.stability_delay = atoi(param) ;		
	}	
	/*Max pupm1*/
	len = find_value("max_pump1=",buf,param) ;
	ESP_LOGI(TAG, "max_pump1=%c len=%d",*param,len);
	if(len>1) {
		turbine_config.max_pump1 = atoi(param) ;		
	}
	/*Min pump1*/
	len = find_value("min_pump1=",buf,param) ;
	ESP_LOGI(TAG, "min_pump1=%c len=%d",*param,len);
	if(len>1) {
		turbine_config.min_pump1 = atoi(param) ;		
	}	
	/*Max pump2*/
	len = find_value("max_pump2=",buf,param) ;
	ESP_LOGI(TAG, "max_pump2=%c len=%d",*param,len);
	if(len>1) {
		turbine_config.max_pump2 = atoi(param) ;		
	}	
	/*Min pump2*/
	len = find_value("min_pump2=",buf,param) ;
	ESP_LOGI(TAG, "min_pump2=%c len=%d",*param,len);
	if(len>1) {
		turbine_config.min_pump2 = atoi(param) ;		
	}
	/*Max vanne1*/
	len = find_value("max_vanne1=",buf,param) ;
	ESP_LOGI(TAG, "max_vanne1=%c len=%d",*param,len);
	if(len>1) {
		turbine_config.max_vanne1 = atoi(param) ;		
	}	
	/*Max vanne2*/
	len = find_value("max_vanne2=",buf,param) ;
	ESP_LOGI(TAG, "max_vanne2=%c len=%d",*param,len);
	if(len>1) {
		turbine_config.max_vanne2 = atoi(param) ;		
	}
	/*RPM Start Starter*/
	len = find_value("start_starter=",buf,param) ;
	ESP_LOGI(TAG, "start_starter=%c len=%d",*param,len);
	if(len>1) {
		turbine_config.start_starter = atoi(param) ;		
	}
	write_nvs_turbine() ;
	free(buf) ;
}

void save_configwifi(httpd_req_t *req)
{
	char *buf = malloc(strlen(req->uri)+1) ;
	char param[15] ;
	int len;

	strcpy(buf,req->uri) ;
	ESP_LOGI(TAG, "Sauvegarde config wifi");
	/*SSID*/
	len = find_value("ssid=",buf,param) ;
//	ESP_LOGI(TAG, "ssid=%c len=%d",*param,len);
	if(len>1) {
		strcpy(wifi_params.ssid,param) ;
	}
	/*Password*/
	len = find_value("password=",buf,param) ;
//	ESP_LOGI(TAG, "password=%c len=%d",*param,len);
	if(len>1) {
		strcpy(wifi_params.password,param) ;
	}
		/*Min pump2*/
	len = find_value("retry=",buf,param) ;
//	ESP_LOGI(TAG, "retry=%c len=%d",*param,len);
	if(len>0) {
		wifi_params.retry = atoi(param) ;
//		ESP_LOGI(TAG, "retry=%d",wifi_params.retry);

	}
	write_nvs_wifi() ;
	free(buf) ;
}

static esp_err_t wifi_get_handler(httpd_req_t *req)
{
	//ESP_LOGI(TAG, "root_get_handler req->uri=[%s]", req->uri);

	char * addr1 = strstr(req->uri, "save=");
	if (addr1 != NULL) save_configwifi(req) ; // Paramètres a sauvagarder
		
	char tmp[10] ;
	// Send HTML header
	send_head(req) ;
	/*httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html>");
	Text2Html(req, "/html/head.html");

	httpd_resp_sendstr_chunk(req, "<h2>");
	httpd_resp_sendstr_chunk(req, turbine_config.name);
	httpd_resp_sendstr_chunk(req, "</h2></div>");*/
		
	httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Paramètres Wifi&nbsp;</b></legend><form method=\"GET\" action=\"wifi\"><p>") ;
	/*SSID*/
	httpd_resp_sendstr_chunk(req, "<b>SSID</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"ssid\" placeholder=\"\" value=\"");
	httpd_resp_sendstr_chunk(req, wifi_params.ssid) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"ssid\" minlength=\"1\" maxlength=\"32\"></p><p>");
	/*Password*/
	httpd_resp_sendstr_chunk(req, "<b>Clef</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"password\" placeholder=\"\" value=\"");
	httpd_resp_sendstr_chunk(req, wifi_params.password) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"password\" minlength=\"8\" maxlength=\"64\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>Nombre de tentatives de connection</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"retry\" placeholder=\"\" value=\"");
	itoa(wifi_params.retry,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"retry\" type=\"number\" min=\"1\" max=\"50\"></p><p>");
	
	httpd_resp_sendstr_chunk(req, "<button name=\"save\" type=\"submit\" class=\"button bgrn\">Sauvegarde</button>") ;
	httpd_resp_sendstr_chunk(req, "</form></fieldset>") ;

	httpd_resp_sendstr_chunk(req, "<p></p><form action=\"/\" method=\"get\"><button name="">Retour</button></form>") ;
	
	Text2Html(req, "/html/footer.html");
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page

	return ESP_OK;
}

static esp_err_t configmoteur(httpd_req_t *req)
{
	ESP_LOGI(TAG, "root_get_handler req->uri=[%s]", req->uri);

	char * addr1 = strstr(req->uri, "save=");
	if (addr1 != NULL) save_configturbine(req) ; // Paramètres a sauvagarder
		
	char tmp[10] ;
	// Send HTML header
	send_head(req) ;
	/*httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html>");
	Text2Html(req, "/html/head.html");

	httpd_resp_sendstr_chunk(req, "<h2>");
	httpd_resp_sendstr_chunk(req, turbine_config.name);
	httpd_resp_sendstr_chunk(req, "</h2></div>");*/
		
	httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Paramètres du moteur&nbsp;</b></legend><form method=\"GET\" action=\"configmoteur\"><p>") ;

	httpd_resp_sendstr_chunk(req, "<b>Nom du moteur</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"name\" placeholder=\"\" value=\"");
	httpd_resp_sendstr_chunk(req, turbine_config.name) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"name\" minlength=\"1\" maxlength=\"20\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>Puissance de la bougie 0-255</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"glow_power\" placeholder=\"\" value=\"");
	itoa(turbine_config.glow_power,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"glow_power\" type=\"number\" min=\"0\" max=\"255\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>RPM plein gaz</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"jet_full_power_rpm\" placeholder=\"\" value=\"");
	itoa(turbine_config.jet_full_power_rpm,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"jet_full_power_rpm\" type=\"number\" min=\"0\" max=\"300000\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>RPM ralenti</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"jet_idle_rpm\" placeholder=\"\" value=\"");
	itoa(turbine_config.jet_idle_rpm,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"\"  type=\"number\" min=\"0\" max=\"300000\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>RPM mini</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"jet_min_rpm\" placeholder=\"\" value=\"");
	itoa(turbine_config.jet_min_rpm,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"jet_min_rpm\"  type=\"number\" min=\"0\" max=\"100000\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>Température de démarrage en °C</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"start_temp\" placeholder=\"\" value=\"");
	itoa(turbine_config.start_temp,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"start_temp\"  type=\"number\" min=\"0\" max=\"500\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>Température max en °C</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"max_temp\" placeholder=\"\" value=\"");
	itoa(turbine_config.max_temp,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"max_temp\"  type=\"number\" min=\"0\" max=\"1000\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>Délai d'accélération (0-100)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"acceleration_delay\" placeholder=\"\" value=\"");
	itoa(turbine_config.acceleration_delay,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"acceleration_delay\"  type=\"number\" min=\"0\" max=\"30\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>Délai de décélération (0-100)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"deceleration_delay\" placeholder=\"\" value=\"");
	itoa(turbine_config.deceleration_delay,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"deceleration_delay\"  type=\"number\" min=\"0\" max=\"30\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>Délai de stabilité (0-100)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"stability_delay\" placeholder=\"\" value=\"");
	itoa(turbine_config.stability_delay,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"stability_delay\"  type=\"number\" min=\"0\" max=\"30\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>PWM Max pompe 1 (0-1024)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"max_pump1\" placeholder=\"\" value=\"");
	itoa(turbine_config.max_pump1,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"max_pump1\"  type=\"number\" min=\"0\" max=\"1024\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>PWM Min pompe 1 (0-1024)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"min_pump1\" placeholder=\"\" value=\"");
	itoa(turbine_config.min_pump1,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"min_pump1\" type=\"number\" min=\"0\" max=\"1024\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>PWM Max pompe 2 (0-1024)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"max_pump2\" placeholder=\"\" value=\"");
	itoa(turbine_config.max_pump2,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"max_pump2\" type=\"number\" min=\"0\" max=\"1024\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>PWM Min pompe 2 (0-1024)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"min_pump2\" placeholder=\"\" value=\"");
	itoa(turbine_config.min_pump2,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"min_pump2\" type=\"number\" min=\"0\" max=\"1024\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>PWM Max vanne 1 (0-1024)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"max_vanne1\" placeholder=\"\" value=\"");
	itoa(turbine_config.max_vanne1,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"max_vanne1\" type=\"number\" min=\"0\" max=\"1024\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>PWM Max vanne 2 (0-1024)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"max_vanne2\" placeholder=\"\" value=\"");
	itoa(turbine_config.max_vanne2,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"max_vanne2\" type=\"number\" min=\"0\" max=\"1024\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>RPM Allumage (0-5000RPM)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"start_starter\" placeholder=\"\" value=\"");
	itoa(turbine_config.start_starter,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"start_starter\" type=\"number\" min=\"0\" max=\"5000\"></p><p>");	

	httpd_resp_sendstr_chunk(req, "<button name=\"save\" type=\"submit\" class=\"button bgrn\">Sauvegarde</button>") ;
	httpd_resp_sendstr_chunk(req, "</form></fieldset>") ;

	httpd_resp_sendstr_chunk(req, "<p></p><form action=\"/\" method=\"get\"><button name="">Retour</button></form>") ;
	
	Text2Html(req, "/html/footer.html");
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page

	return ESP_OK;
}

static esp_err_t slider(httpd_req_t *req)
{
	//ESP_LOGI(TAG, "slider_get_handler req->uri=[%s]", req->uri);
	Text2Html(req, "/html/slider.html");
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page
	return ESP_OK;
}




static const char* get_path_from_uri(char *dest, const char *uri, size_t destsize)
{
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) 
    {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) 
    {
        pathlen = MIN(pathlen, hash - uri);
    }

    // construit le chemin complet (base + path)
    strlcpy(dest, uri, pathlen + 1);

    // retourne le pointeur ver les chemin (sans la base)
    return dest;
}

static esp_err_t gauges_get_handler(httpd_req_t *req){
	httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html>");
	Text2Html(req, "/html/head.html");
	Text2Html(req, "/html/head2.html");
	httpd_resp_sendstr_chunk(req, "<h2>");
	httpd_resp_sendstr_chunk(req, turbine_config.name);
	httpd_resp_sendstr_chunk(req, "</h2>");
	Text2Html(req, "/html/gauges.html");
	httpd_resp_sendstr_chunk(req, "<script>") ;
	Text2Html(req, "/html/gauge.min.js");
	Text2Html(req, "/html/script_gauges.js");
	httpd_resp_sendstr_chunk(req, "</script>") ;
	Text2Html(req, "/html/footer.html");
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page
	return ESP_OK;
}

extern long int time_ecu ;

static esp_err_t g_params_get_handler(httpd_req_t *req){
	static cJSON *myjson;
	char rpms[8][4] ;
	uint32_t max_rpm = turbine_config.jet_full_power_rpm*1.1/1000 ;
	//char errors[200] ;
	//uint32_t rpm = 0 ;
	//ESP_LOGI(TAG, "readings_get_handler req->uri=[%s]", req->uri);
	if(max_rpm <= 160)
	{
		strcpy(rpms[0],"20") ;
		strcpy(rpms[1],"40") ;
		strcpy(rpms[2],"60") ;
		strcpy(rpms[3],"80") ;
		strcpy(rpms[4],"100") ;
		strcpy(rpms[5],"120") ;
		strcpy(rpms[6],"140") ;
		strcpy(rpms[7],"160") ;
		max_rpm = 160 ;
	}
	else 	if(max_rpm <= 200)
	{
		strcpy(rpms[0],"25") ;
		strcpy(rpms[1],"50") ;
		strcpy(rpms[2],"75") ;
		strcpy(rpms[3],"100") ;
		strcpy(rpms[4],"125") ;
		strcpy(rpms[5],"150") ;
		strcpy(rpms[6],"175") ;
		strcpy(rpms[7],"200") ;
		max_rpm = 200 ;
	}
	else 	if(max_rpm <= 240)
	{
		strcpy(rpms[0],"30") ;
		strcpy(rpms[1],"60") ;
		strcpy(rpms[2],"90") ;
		strcpy(rpms[3],"120") ;
		strcpy(rpms[4],"150") ;
		strcpy(rpms[5],"180") ;
		strcpy(rpms[6],"210") ;
		strcpy(rpms[7],"240") ;
		max_rpm = 240 ;
	}
	else 	if(max_rpm <= 280)
	{
		strcpy(rpms[0],"35") ;
		strcpy(rpms[1],"70") ;
		strcpy(rpms[2],"105") ;
		strcpy(rpms[3],"140") ;
		strcpy(rpms[4],"175") ;
		strcpy(rpms[5],"210") ;
		strcpy(rpms[6],"245") ;
		strcpy(rpms[7],"280") ;
		max_rpm = 280 ;
	}
	else 	if(max_rpm <= 320)
	{
		strcpy(rpms[0],"40") ;
		strcpy(rpms[1],"80") ;
		strcpy(rpms[2],"120") ;
		strcpy(rpms[3],"160") ;
		strcpy(rpms[4],"200") ;
		strcpy(rpms[5],"240") ;
		strcpy(rpms[6],"280") ;
		strcpy(rpms[7],"320") ;
		max_rpm = 320 ;		
	}

	myjson = cJSON_CreateObject();
//	if( xSemaphoreTake(xTimeMutex,( TickType_t ) 10) == pdTRUE ) {

	cJSON_AddNumberToObject(myjson, "egt_red", turbine_config.max_temp);
	cJSON_AddNumberToObject(myjson, "rpm_max", max_rpm);
	cJSON_AddNumberToObject(myjson, "rpm_red", turbine_config.jet_full_power_rpm/1000);
	cJSON_AddStringToObject(myjson, "rpm1", rpms[0]);
	cJSON_AddStringToObject(myjson, "rpm2", rpms[1]);
	cJSON_AddStringToObject(myjson, "rpm3", rpms[2]);
	cJSON_AddStringToObject(myjson, "rpm4", rpms[3]);
	cJSON_AddStringToObject(myjson, "rpm5", rpms[4]);
	cJSON_AddStringToObject(myjson, "rpm6", rpms[5]);
	cJSON_AddStringToObject(myjson, "rpm7", rpms[6]);
	cJSON_AddStringToObject(myjson, "rpm8", rpms[7]);
	char *my_json_string = cJSON_Print(myjson);
	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr_chunk(req, my_json_string); //fin de la page
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page
	cJSON_Delete(myjson) ;
	free(my_json_string) ; 
	return ESP_OK;
}		

static esp_err_t readings_get_handler(httpd_req_t *req){
	static cJSON *myjson;
	char status[50] ;
	char errors[200] ;
	uint8_t minutes,secondes ;
	get_time_up(&turbine,&secondes,&minutes,NULL) ;
	//uint32_t rpm = 0 ;
	//ESP_LOGI(TAG, "readings_get_handler req->uri=[%s]", req->uri);
	myjson = cJSON_CreateObject();
//	if( xSemaphoreTake(xTimeMutex,( TickType_t ) 10) == pdTRUE ) {
		phase_to_str(status) ;
		cJSON_AddNumberToObject(myjson, "ppm_gaz", get_gaz(&turbine));
		cJSON_AddNumberToObject(myjson, "ppm_aux", get_aux(&turbine));
		cJSON_AddNumberToObject(myjson, "egt", get_EGT(&turbine));
		cJSON_AddNumberToObject(myjson, "rpm", get_RPM(&turbine));
		cJSON_AddNumberToObject(myjson, "pump1", turbine.pump1.value);
		cJSON_AddNumberToObject(myjson, "pump2", turbine.pump2.value);
		cJSON_AddNumberToObject(myjson, "vanne1", turbine.vanne1.get_power(&turbine.vanne1));
		cJSON_AddNumberToObject(myjson, "vanne2", turbine.vanne2.get_power(&turbine.vanne2));
		cJSON_AddNumberToObject(myjson, "glow", turbine.glow.get_power(&turbine.glow));
		cJSON_AddStringToObject(myjson, "status", status);
		get_errors(errors); 
		cJSON_AddStringToObject(myjson, "error", errors);
		sprintf(status,"%02d:%02d",minutes,secondes);
		cJSON_AddStringToObject(myjson, "time", status);
	
//	}xSemaphoreGive(xTimeMutex) ;
	char *my_json_string = cJSON_Print(myjson);
	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr_chunk(req, my_json_string); //fin de la page
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page
	cJSON_Delete(myjson) ;
	free(my_json_string) ; 
	return ESP_OK;
}

/*static esp_err_t events_get_handler(httpd_req_t *req){
//	ESP_LOGI(TAG, "events_get_handler req->uri=[%s]", req->uri);
	httpd_resp_set_type(req, "text/event-stream");
	httpd_resp_sendstr_chunk(req, "Alive"); //fin de la page
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page
	readings_get_handler(req) ;
	return ESP_OK;
}*/

/*esp_err_t upgrade_get_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "Reboot to factory partition");
	esp_partition_t *factory_partition = esp_partition_find_first(ESP_PARTITION_TYPE_ANY,ESP_PARTITION_SUBTYPE_ANY,"factory") ;
	esp_ota_set_boot_partition(factory_partition) ;
	vTaskDelay(500 / portTICK_PERIOD_MS);
	esp_restart();
}*/

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
esp_err_t upgrade_get_handler(httpd_req_t *req)
{
	httpd_resp_send(req, (const char *) index_html_start, index_html_end - index_html_start);
	return ESP_OK;
}



static esp_err_t frontpage(httpd_req_t *req)
{
//	ESP_LOGI(TAG, "root_get_handler req->uri=[%s]", req->uri);
    char filepath[20];
	//static int i=0 ;
    //ESP_LOGI(TAG, "URI : %s", req->uri);
	xTimerStop( xTimer60s,0) ;
    const char *filename = get_path_from_uri(filepath, req->uri, sizeof(filepath));
	
    if (!filename) 
    {
        ESP_LOGE(TAG, "Filename is too long");
        // retourne une erreur 500 (Internal Server Error)
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }
	//ESP_LOGI(TAG, "File : %s", filename);

	if(strcmp(filename, "/configecu") == 0) 
		configecu(req) ;
	else if(strcmp(filename, "/configmoteur") == 0) 
		configmoteur(req) ;
	else if(strcmp(filename, "/logs") == 0) 
		logs(req) ;
	else if(strcmp(filename, "/slider") == 0) 
		slider(req) ;
	else if(strcmp(filename, "/start") == 0) 
		turbine.phase_fonctionnement = START ;
	else if(strcmp(filename, "/stop") == 0) 
		turbine.phase_fonctionnement = STOP ;


	else if(strcmp(filename, "/curves.txt") == 0) 
		curves_get_handler(req) ;
	else if(strcmp(filename, "/logs.txt") == 0) 
		logs_get_handler(req) ;
	else if(strcmp(filename, "/favicon.ico") == 0) 
		favicon_get_handler(req) ;
	else if(strcmp(filename, "/wifi") == 0) 
		wifi_get_handler(req) ;
	else if(strcmp(filename, "/stopwifi") == 0) {
		ESP_ERROR_CHECK(esp_wifi_stop() );
		vTaskDelete( xWebHandle ); }
	else if(strcmp(filename, "/gauges") == 0) 
	{
//		ESP_ERROR_CHECK( heap_trace_start(HEAP_TRACE_LEAKS) );
		gauges_get_handler(req) ;
/*		i++ ;
		if(i>10)
		{
			ESP_ERROR_CHECK( heap_trace_stop() );
			heap_trace_dump();
			i = 0 ;
		}*/
	}
	else if(strcmp(filename, "/readings") == 0) 
		readings_get_handler(req) ;
	else if(strcmp(filename, "/g_params") == 0) 
		g_params_get_handler(req) ;
	//else if(strcmp(filename, "/events") == 0) 
	//	events_get_handler(req) ;
	else if(strcmp(filename, "/upgrade") == 0) 
		upgrade_get_handler(req) ;
	else if(strcmp(filename, "/") == 0 ) 
	{
		// Send HTML header
	
	/*httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html>");
	Text2Html(req, "/html/head.html");

	httpd_resp_sendstr_chunk(req, "<h2>");
	httpd_resp_sendstr_chunk(req, turbine_config.name);
	httpd_resp_sendstr_chunk(req, "</h2>");*/
	send_head(req) ;

	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"configecu\">");
	httpd_resp_sendstr_chunk(req, "<button>Paramètres ECU</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");
	
	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"configmoteur\">");
	httpd_resp_sendstr_chunk(req, "<button>Paramètres moteur</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");
	
	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"info\">");
	httpd_resp_sendstr_chunk(req, "<button>Information</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");
	
	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"logs\">");
	httpd_resp_sendstr_chunk(req, "<button>Logs</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");

	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"wifi\">");
	httpd_resp_sendstr_chunk(req, "<button>WiFi</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");

	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"slider\">");
	httpd_resp_sendstr_chunk(req, "<button>Slider</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");

	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"gauges\">");
	httpd_resp_sendstr_chunk(req, "<button>Jauges</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");

	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"start\">");
	httpd_resp_sendstr_chunk(req, "<button>Start engine</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");

	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"stop\">");
	httpd_resp_sendstr_chunk(req, "<button>Stop engine</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");

	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"upgrade\">");
	httpd_resp_sendstr_chunk(req, "<button class=\"button bred\">Mise à jour</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");

	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"stopwifi\">");
	httpd_resp_sendstr_chunk(req, "<button class=\"button bred\">Couper le WiFi</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");

	Text2Html(req, "/html/footer.html");
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page
	}
	

	return ESP_OK;
}

/* Function to start the web server */
esp_err_t start_server(const char *base_path, int port)
{
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.stack_size = 16392 ; // Default 4K -> 20K
	config.server_port = port;

	/* Use the URI wildcard matching function in order to
	 * allow the same handler to respond to multiple different
	 * target URIs which match the wildcard scheme */
	config.uri_match_fn = httpd_uri_match_wildcard;

	ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
	if (httpd_start(&server, &config) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to start file server!");
		return ESP_FAIL;
	}

	/* URI handler for get */
	httpd_uri_t _root_get_handler = {
		.uri		 = "/*",
		.method		 = HTTP_GET,
		.handler	 = frontpage, //root_get_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};

	httpd_register_uri_handler(server, &_root_get_handler);


	/* URI handler for post */
	httpd_uri_t _root_post_handler = {
		.uri		 = "/*",
		.method		 = HTTP_POST,
		.handler	 = root_post_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &_root_post_handler);



	return ESP_OK;
}


void http_server_task(void *pvParameters)
{
	char *task_parameter = (char *)pvParameters;
	ESP_LOGI(TAG, "Take semaphore") ;
	xSemaphoreTake(http_task_start, portMAX_DELAY) ;
	ESP_LOGI(TAG, "Start task_parameter=%s", task_parameter);
	char url[64];
	sprintf(url, "http://%s:%d", task_parameter, CONFIG_WEB_PORT);

	// Start Server
	ESP_LOGI(TAG, "Starting server on %s", url);
	ESP_ERROR_CHECK(start_server("/spiffs", CONFIG_WEB_PORT));
	
	//URL_t urlBuf;
	while(1) {
		vTaskDelay(10 / portTICK_PERIOD_MS);
		// Waiting for submit
		/*
		if (xQueueReceive(xQueueHttp, &urlBuf, portMAX_DELAY) == pdTRUE) {
			ESP_LOGI(TAG, "url=%s", urlBuf.url);
			ESP_LOGI(TAG, "parameter=%s", urlBuf.parameter);

			
			// save key & value to NVS
			esp_err_t err = save_key_value(urlBuf.url, urlBuf.parameter);
			if (err != ESP_OK) {
				ESP_LOGE(TAG, "Error (%s) saving to NVS", esp_err_to_name(err));
			}

			// load key & value from NVS
			err = load_key_value(urlBuf.url, urlBuf.parameter, sizeof(urlBuf.parameter));
			if (err != ESP_OK) {
				ESP_LOGE(TAG, "Error (%s) loading to NVS", esp_err_to_name(err));
			}
		}*/
	}

	// Never reach here
	ESP_LOGI(TAG, "finish");
	vTaskDelete(NULL);
}