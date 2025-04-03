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
#include <math.h>

#include "esp_heap_trace.h"

#include <esp_ota_ops.h>

#include "jf-ecu32.h"
#include "inputs.h"
#include "nvs_ecu.h"
#include "http_server.h"
#include "wifi.h"
#include "error.h"
#include "calibration.h"
#include "html.h"
#ifdef IMU
#include "imu.h"
#endif
#include "websocket.h"
#include "file_server.h"
#include <dirent.h>

extern TimerHandle_t xTimer60s ;
static const char *TAG = "HTTP";
TickType_t Ticks ;
httpd_handle_t server = NULL;

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
	if(strcmp(filename, "/p_update") == 0) 
		update_post_handler(req) ;
	else if(strcmp(filename, "/p_post") == 0) {
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
			/*turbine.pump1.value = atoi(param) ;
			if(turbine.pump1.config.ppm_pwm == PPM)
				set_power_func_us(&turbine.pump1,atoi(param)) ;
			else
				set_power_func(&turbine.pump1,atof(param)/20) ;*/
			set_power(&turbine.pump1,atof(param)) ;
		}

		len = find_value("pwmSliderValue2=",content,param) ;
		if(len > 0)
		{
			set_power(&turbine.pump2,atof(param)) ;
			/*turbine.pump2.value = atoi(param) ;
			if(turbine.pump2.config.ppm_pwm == PPM)
				set_power_func_us(&turbine.pump2,atoi(param)) ;
			else
				set_power_func(&turbine.pump2,atof(param)/20) ;*/
		}

		len = find_value("pwmSliderValue3=",content,param) ;
		if(len > 0)
		{
			set_power(&turbine.starter,atof(param)) ;
			/*turbine.starter.value = atoi(param) ;
			if(turbine.starter.config.ppm_pwm == PPM)
				set_power_func_us(&turbine.starter,atoi(param)) ;
			else
				set_power_func(&turbine.starter,atof(param)/20) ;*/
		}
		//Vanne 1
		len = find_value("pwmSliderValue4=",content,param) ;
		if(len > 0)
		{
			//turbine.vanne1.value = atoi(param) ;
			param_int32 = atoi(param) ;
			if(param_int32 > turbine_config.max_vanne1)
				param_int32 = turbine_config.max_vanne1 ;
			set_power_vanne(&turbine.vanne1,param_int32) ;
		}
		//Vanne 2
		len = find_value("pwmSliderValue5=",content,param) ;
		if(len > 0)
		{
			param_int32 = atoi(param) ;
			if(param_int32 > turbine_config.max_vanne2)
				param_int32 = turbine_config.max_vanne2 ;
			set_power_vanne(&turbine.vanne2,param_int32) ;
			/*turbine.vanne2.value = atoi(param) ;
			turbine.vanne2.set_power(&turbine.vanne2.config,atoi(param)) ;*/
		}
		// GLOW
		len = find_value("pwmSliderValue6=",content,param) ;
		if(len > 0)
		{
			param_int32 = atoi(param) ;
			if(param_int32 > turbine_config.glow_power)
				param_int32 = turbine_config.glow_power ;
			set_power_glow(&turbine.glow,param_int32) ;
			//turbine.glow.value = param_int32 ;	
			//turbine.glow.set_power(&turbine.glow.config,param_int32) ;
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
		
	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"c_logs.txt\">");
	httpd_resp_sendstr_chunk(req, "<button>Log 1</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");
	
	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"c_curves.txt\">");
	httpd_resp_sendstr_chunk(req, "<button>Courbe de gaz</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");
	
	httpd_resp_sendstr_chunk(req, "<p></p><form action=\"/\" method=\"get\"><button name="">Retour</button></form>") ;

	httpd_resp_sendstr_chunk(req, "<p></p>");

	Text2Html(req, "/html/footer.html");
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page

	return ESP_OK;
}

int find_param_radio(int param,char *buf,char *output)
{
	int len ;
	char input[20] ;
	char title[40] ;
	sprintf(input,"rd_input_%d=",param) ;
	len = find_value(input,buf,output) ;
	ESP_LOGI(TAG, "%s=%s len=%d",GetTextIndexed(title, sizeof(title), param, htmlRadioParamEcu),output,len);
	return len ;
}

int find_param_checkbox(int param,char *buf,char *output)
{
	int len ;
	char input[20] ;
	char title[40] ;
	sprintf(input,"ch_input%d=",param) ;
	len = find_value(input,buf,output) ;
	ESP_LOGI(TAG, "%s=%s len=%d",GetTextIndexed(title, sizeof(title), param, htmlCheckParamEcu),output,len);
	return len ;
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
	//len = find_value("input=",buf,param) ;
	len = find_param_radio(R_THROTTLE_TYP,buf,param) ;
	if(len==1) {
		if(*param == '0'){
			config_ECU.input_type = PPM ;
		}else{
			config_ECU.input_type = SBUS ;
		}
	}
	//len = find_value("glow_type=",buf,param) ;
	len = find_param_radio(R_GLOW_TYP,buf,param) ;
	if(len==1) {
		if(*param == '0'){
			config_ECU.glow_type = GAS ;
		}else{
			config_ECU.glow_type = KERO ;
		}
	}
	//len = find_value("start_type=",buf,param) ;
	len = find_param_radio(R_START_TYP,buf,param) ;
	if(len==1) {
		if(*param == '0'){
			config_ECU.start_type = MANUAL ;
		}else{
			config_ECU.start_type = AUTO ;
		}
	}
	//len = find_value("output_pump1=",buf,param) ;
	len = find_param_radio(R_PUMP1_TYP,buf,param) ;
	if(len==1) {
		if(*param == '0'){
			config_ECU.output_pump1 = PPM ;
		}else{
			config_ECU.output_pump1 = PWM ;
		}
	}
	//len = find_value("output_starter=",buf,param) ; 
	len = find_param_radio(R_STARTER_TYP,buf,param) ;
	if(len==1) {
		if(*param == '0'){
			config_ECU.output_starter = PPM ;
		}else{
			config_ECU.output_starter = PWM ;
		}
	}
	//len = find_value("telem=",buf,param) ;
	len = find_param_radio(R_TELEM_TYP,buf,param) ;
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
	//len = find_value("output_pump2=",buf,param) ;
	len = find_param_radio(R_PUMP2_TYP,buf,param) ;
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
	//len = find_value("use_input2=",buf,param) ;
	len = find_param_checkbox(C_AUX_EN,buf,param) ;
	if(len == 2){
		config_ECU.use_input2 = YES ;
	}else{
		config_ECU.use_input2 = NO ;
	}
	
	//len = find_value("use_led=",buf,param) ;
	len = find_param_checkbox(C_LEDS_EN,buf,param) ;
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

	/*ESP_LOGI("CONFIG_ECU","input : %d",config_ECU.input_type) ;
	ESP_LOGI("CONFIG_ECU","glow_type : %d",config_ECU.glow_type) ;	
	ESP_LOGI("CONFIG_ECU","start_type : %d",config_ECU.start_type) ;
	ESP_LOGI("CONFIG_ECU","output_pump1 : %d",config_ECU.output_pump1) ;
	ESP_LOGI("CONFIG_ECU","output_pump2 : %d",config_ECU.output_pump2) ;
	ESP_LOGI("CONFIG_ECU","output_starter : %d",config_ECU.output_starter) ;
	ESP_LOGI("CONFIG_ECU","use_telem : %d",config_ECU.use_telem) ;
	ESP_LOGI("CONFIG_ECU","use_input2 : %d",config_ECU.use_input2) ;
	ESP_LOGI("CONFIG_ECU","use_led : %d",config_ECU.use_led) ;*/

	// Send HTML header
	send_head(req) ;
	/*httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html>");
	Text2Html(req, "/html/head.html");

	httpd_resp_sendstr_chunk(req, "<h2>");
	httpd_resp_sendstr_chunk(req, turbine_config.name);
	httpd_resp_sendstr_chunk(req, "</h2></div>");*/
	
	
	
	httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Paramètre de l'ECU&nbsp;</b></legend><form method=\"GET\" action=\"configecu\"><p>") ;
	/*Voie des gaz*/
	WSRadio(req,R_THROTTLE_TYP,config_ECU.input_type,true);
	WSRadio(req,R_GLOW_TYP,config_ECU.glow_type,true);
	WSRadio(req,R_START_TYP,config_ECU.start_type,true);
	WSRadio(req,R_PUMP1_TYP,config_ECU.output_pump1,true);
	WSRadio(req,R_STARTER_TYP,config_ECU.output_starter,true);
	WSRadio(req,R_TELEM_TYP,config_ECU.use_telem,true);
	WSRadio(req,R_PUMP2_TYP,config_ECU.output_pump2,true);
	WSCheckBox(req,C_AUX_EN,config_ECU.use_input2,true) ;
	WSCheckBox(req,C_LEDS_EN,config_ECU.use_led,true) ;

	
	/* Save*/
	WSSaveBouton(req) ;
	

	/*Retour*/	
	WSRetourBouton(req) ;
	//httpd_resp_sendstr_chunk(req, "<p></p><form action=\"/\" method=\"get\"><button name="">Retour</button></form>") ;

	Text2Html(req, "/html/footer.html");
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page

	return ESP_OK;
}

int find_param_input(int param,char *buf,char *output)
{
	int len ;
	char input[20] ;
	char title[40] ;
	sprintf(input,"input_%d=",param) ;
	len = find_value(input,buf,output) ;
	ESP_LOGI(TAG, "%s=%s len=%d",GetTextIndexed(title, sizeof(title), param, htmlInputParamEng),output,len);
	return len ;
}

void save_configturbine(httpd_req_t *req)
{
	char *buf = malloc(strlen(req->uri)+1) ;
	char param[30] ;
	int len;

	strcpy(buf,req->uri) ;
	ESP_LOGI(TAG, "Sauvegarde config turbine");
	/*Nom*/
	len = find_param_input(I_NAME,buf,param) ;
	if(len>0) {
		strcpy(turbine_config.name,param) ;
	}
	/*Puissance bougie*/
	len = find_param_input(I_GLOWPOWER,buf,param) ;
	if(len>0) {
		turbine_config.glow_power = atoi(param) ;		
	}
	/*Max RPM*/
	len = find_param_input(I_RPMMAX,buf,param) ;
	if(len>0) {
		turbine_config.jet_full_power_rpm = atoi(param) ;		
	}
	/*RPM ralenti*/
	len = find_param_input(I_RPMIDLE,buf,param) ;
	if(len>0) {
		turbine_config.jet_idle_rpm = atoi(param) ;		
	}
	/*RPM mini*/
	len = find_param_input(I_RPMMIN,buf,param) ;
	ESP_LOGI(TAG, "jet_min_rpm=%c len=%d",*param,len);
	if(len>0) {
		turbine_config.jet_min_rpm = atoi(param) ;		
	}
	/*Start Temp.*/
	len = find_param_input(I_TEMPSTART,buf,param) ;
	if(len>0) {
		turbine_config.start_temp = atoi(param) ;		
	}
	/*Max Temp.*/
	len = find_param_input(I_TEMPMAX,buf,param) ;
	if(len>0) {
		turbine_config.max_temp = atoi(param) ;		
	}
	/*Délai de stabilité*/
	len = find_param_input(I_DELAYSTAB,buf,param) ;
	if(len>0) {
		turbine_config.stability_delay = atoi(param) ;		
	}	
	/*Max pupm1*/
	len = find_param_input(I_PUMP1MAX,buf,param) ;
	if(len>0) {
		turbine_config.max_pump1 = atoi(param) ;		
	}
	/*Min pump1*/
	len = find_param_input(I_PUMP1MIN,buf,param) ;
	if(len>0) {
		turbine_config.min_pump1 = atoi(param) ;		
	}	
	/*Max pump2*/
	len = find_param_input(I_PUMP2MAX,buf,param) ;
	if(len>0) {
		turbine_config.max_pump2 = atoi(param) ;		
	}	
	/*Min pump2*/
	len = find_param_input(I_PUMP2MIN,buf,param) ;
	if(len>0) {
		turbine_config.min_pump2 = atoi(param) ;		
	}
	/*Max vanne1*/
	len = find_param_input(I_VANNE1MAX,buf,param) ;
	if(len>0) {
		turbine_config.max_vanne1 = atoi(param) ;		
	}	
	/*Max vanne2*/
	len = find_param_input(I_VANNE2MAX,buf,param) ;
	if(len>0) {
		turbine_config.max_vanne2 = atoi(param) ;		
	}
	/*RPM Start Starter*/
	len = find_param_input(I_RPMSTARTER,buf,param) ;
	if(len>0) {
		turbine_config.starter_rpm_start = atoi(param) ;		
	}
	len = find_param_input(I_MAXSTARTERRPM,buf,param) ;
	if(len>0) {
		turbine_config.starter_max_rpm = atoi(param) ;		
	}

	len = find_param_input(I_LIPO_ELEMENTS,buf,param) ;
	if(len>0) {
		ESP_LOGI(TAG, "ELEMENT %s",param);
		turbine_config.lipo_elements = atoi(param) ;		
	}
	len = find_param_input(I_VMIN_START,buf,param) ;
	if(len>0) {
		ESP_LOGI(TAG, "Vmin %s",param);
		turbine_config.Vmin_decollage = atof(param) ;		
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
	if(len>0) {
		strcpy(wifi_params.ssid,param) ;
	}
	/*Password*/
	len = find_value("password=",buf,param) ;
//	ESP_LOGI(TAG, "password=%c len=%d",*param,len);
	if(len>0) {
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
	
	//httpd_resp_sendstr_chunk(req, "<button name=\"save\" type=\"submit\" class=\"button bgrn\">Sauvegarde</button>") ;
	//httpd_resp_sendstr_chunk(req, "</form></fieldset>") ;
	WSSaveBouton(req) ;
	WSRetourBouton(req) ;

//	httpd_resp_sendstr_chunk(req, "<p></p><form action=\"/\" method=\"get\"><button name="">Retour</button></form>") ;
	
	Text2Html(req, "/html/footer.html");
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page

	return ESP_OK;
}

static esp_err_t calibrations(httpd_req_t *req)
{
	ESP_LOGI(TAG, "root_get_handler req->uri=[%s]", req->uri);

	// Send HTML header
	send_head(req) ;

	// Send buttons
	WSBouton(req,BT_STARTCALIBRATION) ;
	//httpd_resp_sendstr_chunk(req, "<p></p><form action=\"starter_calibration\" method=\"get\"><button name="">Calibrer le démarreur</button></form>") ;

	
	WSRetourBouton(req) ;
	//httpd_resp_sendstr_chunk(req, "<p></p><form action=\"/\" method=\"get\"><button name="">Retour</button></form>") ;

	// Send footer
	Text2Html(req, "/html/footer.html");
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page

	return ESP_OK;
}

static esp_err_t starter_calibration_page(httpd_req_t *req)
{
	ESP_LOGI(TAG, "root_get_handler req->uri=[%s]", req->uri);

	// Send HTML header
	send_head(req) ;

	// Send buttons
	WSBouton(req,BT_STOPCALIBRATION) ;
	//httpd_resp_sendstr_chunk(req, "<p></p><form action=\"stop_starter_calibration\" method=\"get\"><button name="">"B_STOPCALIBRATION"</button></form>") ;

	// Send footer
	Text2Html(req, "/html/chart_starter.html");
	WSBouton(req,BT_SAVE_ST_CAL) ;
	WSRetourBouton(req) ;
	Text2Html(req, "/html/footer.html");
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page
	
	xTaskCreatePinnedToCore(starter_calibration, "Starter calibration", configMINIMAL_STACK_SIZE * 8, NULL, (configMAX_PRIORITIES -1 )|( 1UL | portPRIVILEGE_BIT ), &starter_calibration_h,1);	
	return ESP_OK;
}

static esp_err_t stop_starter_calibration(httpd_req_t *req)
{
	ESP_LOGI(TAG, "root_get_handler req->uri=[%s]", req->uri) ;
	stop_starter_cal() ;
	
	calibrations(req) ;
	
	return ESP_OK;
}

static esp_err_t configmoteur(httpd_req_t *req)
{
	ESP_LOGI(TAG, "root_get_handler req->uri=[%s]", req->uri);

	char * addr1 = strstr(req->uri, "save=");
	if (addr1 != NULL) save_configturbine(req) ; // Paramètres a sauvagarder
		
	// Send HTML header
	send_head(req) ;
	httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Paramètres du moteur&nbsp;</b></legend><form method=\"GET\" action=\"c_moteur\"><p>") ;
	WSInputBox(req,I_NAME,0,turbine_config.name,TEXT,true) ;
	WSInputBox(req,I_GLOWPOWER,turbine_config.glow_power,NULL,NUMBER,true) ;
	WSInputBox(req,I_RPMMAX,turbine_config.jet_full_power_rpm,NULL,NUMBER,true) ;
	WSInputBox(req,I_RPMIDLE,turbine_config.jet_idle_rpm,NULL,NUMBER,true) ;
	WSInputBox(req,I_RPMMIN,turbine_config.jet_min_rpm,NULL,NUMBER,true) ;
	WSInputBox(req,I_TEMPSTART,turbine_config.start_temp,NULL,NUMBER,true) ;
	WSInputBox(req,I_TEMPMAX,turbine_config.max_temp,NULL,NUMBER,true) ;
	WSInputBox(req,I_DELAYACC,turbine_config.acceleration_delay,NULL,NUMBER,false) ;
	WSInputBox(req,I_DELAYDEC,turbine_config.deceleration_delay,NULL,NUMBER,false) ;
	WSInputBox(req,I_DELAYSTAB,turbine_config.stability_delay,NULL,NUMBER,true) ;
	WSInputBox(req,I_PUMP1MAX,turbine_config.max_pump1,NULL,NUMBER,true) ;
	WSInputBox(req,I_PUMP1MIN,turbine_config.min_pump1,NULL,NUMBER,true) ;
	WSInputBox(req,I_PUMP2MAX,turbine_config.max_pump2,NULL,NUMBER,true) ;
	WSInputBox(req,I_PUMP2MIN,turbine_config.min_pump2,NULL,NUMBER,true) ;
	WSInputBox(req,I_VANNE1MAX,turbine_config.max_vanne1,NULL,NUMBER,true) ;
	WSInputBox(req,I_VANNE2MAX,turbine_config.max_vanne2,NULL,NUMBER,true) ;
	WSInputBox(req,I_RPMSTARTER,turbine_config.starter_rpm_start,NULL,NUMBER,true) ;
	WSInputBox(req,I_MAXSTARTERRPM,turbine_config.starter_max_rpm,NULL,NUMBER,true) ;
	WSInputBox(req,I_LIPO_ELEMENTS,turbine_config.lipo_elements,NULL,NUMBER,true) ;
	WSInputBox(req,I_VMIN_START,turbine_config.Vmin_decollage,NULL,NUMBER,true) ;
	
	WSSaveBouton(req) ;
	/*httpd_resp_sendstr_chunk(req, "<button name=\"save\" type=\"submit\" class=\"button bgrn\">Sauvegarde</button>") ;
	httpd_resp_sendstr_chunk(req, "</form></fieldset>") ;*/
	WSBouton(req,BT_CALIBRATION) ;
	//httpd_resp_sendstr_chunk(req, "<p></p><form action=\"calibrations\" method=\"get\"><button name="">Calibrations</button></form>") ;	
	//httpd_resp_sendstr_chunk(req, "<p></p><form action=\"/\" method=\"get\"><button name="">Retour</button></form>") ;
	WSRetourBouton(req) ;
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




const char* get_path_from_uri(char *dest, const char *uri, size_t destsize)
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

static esp_err_t chart_get_handler(httpd_req_t *req){
	httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html>");
	Text2Html(req, "/html/head.html");
	Text2Html(req, "/html/head2.html");
	httpd_resp_sendstr_chunk(req, "<h2>");
	httpd_resp_sendstr_chunk(req, turbine_config.name);
	httpd_resp_sendstr_chunk(req, "</h2>");
	Text2Html(req, "/html/chart.html");
	WSRetourBouton(req) ;
	Text2Html(req, "/html/footer.html");
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page
	Ticks = xTaskGetTickCount() ;
	return ESP_OK;
}

static esp_err_t gauges_get_handler(httpd_req_t *req){
	httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html>");
	httpd_resp_sendstr_chunk(req, "<script src=\"https://cdn.jsdelivr.net/npm/chart.js@4.0.1/dist/chart.umd.min.js\">") ;
	httpd_resp_sendstr_chunk(req, "</script>") ;
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
	Ticks = xTaskGetTickCount() ;
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

static esp_err_t g_chartdata_get_handler(httpd_req_t *req){
	//char labels[500] ;
	//char datas[200] ;
	static cJSON *myjson;
	cJSON *labels,*datas;
	myjson = cJSON_CreateObject();

	labels = cJSON_CreateIntArray(turbine_config.power_table.RPM,50); 
	cJSON_AddItemToObject(myjson,"labels",labels) ;
	datas = cJSON_CreateIntArray(turbine_config.power_table.pump,50);
	cJSON_AddItemToObject(myjson,"data",datas) ;
	
	char *my_json_string = cJSON_Print(myjson) ;
	httpd_resp_set_type(req, "application/json") ;
	httpd_resp_sendstr_chunk(req, my_json_string) ; //fin de la page

	httpd_resp_sendstr_chunk(req, NULL); //fin de la page
	cJSON_Delete(myjson) ;
	free(my_json_string) ; 
	return ESP_OK;
}

float decround(float f, int places) { 
      float multiplier = pow(10, places); 
      return round((int)(f*multiplier+0.5))/multiplier; 
} 
 

static esp_err_t g_chartstarter_get_handler(httpd_req_t *req){
	//char labels[500] ;
	//char datas[200] ;
	_chart_data_t c_datas_rx ;
	BaseType_t Q_Result ;
	static cJSON *myjson;
	//cJSON *labels,*datas;
	if(Q_Calibration_Values != NULL)
	{
		myjson = cJSON_CreateObject();
		Q_Result = xQueueReceive(Q_Calibration_Values,&c_datas_rx,100) ;
		if(Q_Result == pdTRUE)
		{
			cJSON_AddNumberToObject(myjson,"data",c_datas_rx.data);
			cJSON_AddNumberToObject(myjson,"labels",decround(c_datas_rx.label,1));
			cJSON_AddNumberToObject(myjson,"end",c_datas_rx.end);
			cJSON_AddNumberToObject(myjson,"power_start",c_datas_rx.power_start);
			cJSON_AddNumberToObject(myjson,"rpmstart",c_datas_rx.rpmstart);
			cJSON_AddNumberToObject(myjson,"powermin",c_datas_rx.powermin);
			cJSON_AddNumberToObject(myjson,"rpmmax",c_datas_rx.rpmmax);
			cJSON_AddNumberToObject(myjson,"rpm",c_datas_rx.rpm);
			cJSON_AddNumberToObject(myjson,"time",c_datas_rx.time);
			char *my_json_string = cJSON_Print(myjson) ;
			httpd_resp_set_type(req, "application/json") ;
			httpd_resp_sendstr_chunk(req, my_json_string) ; //fin de la page
			httpd_resp_sendstr_chunk(req, NULL); //fin de la page
			cJSON_Delete(myjson) ;
			free(my_json_string) ; 
		}
	}
	return ESP_OK;
}

static esp_err_t readings_get_handler(httpd_req_t *req){
	static cJSON *myjson;
	char status[50] ;
	char errors[200] ;
	#ifdef IMU
	mpu6050_acce_value_t acce;	
	#endif

	uint8_t heures,minutes,secondes ;
	//ESP_LOGI(TAG, "readings_get_handler req->uri=[%s]", req->uri);
	get_time_total(&turbine,&secondes,&minutes,&heures) ;

	//uint32_t rpm = 0 ;
	//ESP_LOGI(TAG, "cJSON_CreateObject");
	myjson = cJSON_CreateObject();
//	if( xSemaphoreTake(xTimeMutex,( TickType_t ) 10) == pdTRUE ) {
		phase_to_str(status) ;
		//ESP_LOGI(TAG, "get_gaz");
		cJSON_AddNumberToObject(myjson, "ppm_gaz", get_gaz(&turbine));
		//ESP_LOGI(TAG, "get_aux");
		cJSON_AddNumberToObject(myjson, "ppm_aux", get_aux(&turbine));
		//ESP_LOGI(TAG, "get_EGT");
		cJSON_AddNumberToObject(myjson, "egt", get_EGT(&turbine));
		//ESP_LOGI(TAG, "get_RPM");
		cJSON_AddNumberToObject(myjson, "rpm", get_EGT(&turbine));
		//ESP_LOGI(TAG, "pump1");
		cJSON_AddNumberToObject(myjson, "pump1", get_power(&turbine.pump1));
		//ESP_LOGI(TAG, "pump2");
		cJSON_AddNumberToObject(myjson, "pump2", get_power(&turbine.pump2));
		//ESP_LOGI(TAG, "vanne1");
		cJSON_AddNumberToObject(myjson, "vanne1", get_vanne_power(&turbine.vanne1));
		//ESP_LOGI(TAG, "vanne2");
		cJSON_AddNumberToObject(myjson, "vanne2", get_vanne_power(&turbine.vanne2));
		//ESP_LOGI(TAG, "glow");
		cJSON_AddNumberToObject(myjson, "glow", get_glow_power(&turbine.glow));
		//ESP_LOGI(TAG, "statut : %s", status);
		cJSON_AddStringToObject(myjson, "status", status);
		//ESP_LOGI(TAG, "get_errors");
		get_errors(errors); 
		cJSON_AddStringToObject(myjson, "error", errors);
		//ESP_LOGI(TAG, "minutes,secondes");
		sprintf(status,"%02d:%02d:%02d",heures,minutes,secondes);
		//ESP_LOGI(TAG, "time : %s", status);
		cJSON_AddStringToObject(myjson, "time", status);

		cJSON_AddNumberToObject(myjson, "ticks",xTaskGetTickCount() - Ticks);
		#ifdef IMU
		if(xQueueReceive(xQueueIMU, &acce, portMAX_DELAY)) {
			cJSON_AddNumberToObject(myjson, "accx", acce.acce_x);
			cJSON_AddNumberToObject(myjson, "accy", acce.acce_y);
			cJSON_AddNumberToObject(myjson, "accz", acce.acce_z);

		}else {
				ESP_LOGE(TAG, "xQueueReceive fail");
		}
		#endif
	
//	}xSemaphoreGive(xTimeMutex) ;
	//ESP_LOGI(TAG, "Send http");
	char *my_json_string = cJSON_Print(myjson);
	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr_chunk(req, my_json_string); //fin de la page
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page
	//ESP_LOGI(TAG, "cJSON_Delete");
	cJSON_Delete(myjson) ;
	//ESP_LOGI(TAG, "free");
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


static esp_err_t configs(httpd_req_t *req)
{
//ESP_LOGI(TAG, "root_get_handler req->uri=[%s]", req->uri);
    char filepath[20];
	esp_err_t err ;
	//static int i=0 ;
    //ESP_LOGI(TAG, "configs - URI : %s", req->uri);
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
	err = ESP_OK ;

	if(strcmp(filename, "/c_ecu") == 0) 
		err = configecu(req) ;
	else if(strcmp(filename, "/c_moteur") == 0) 
		err = configmoteur(req) ;
	else if(strcmp(filename, "/c_cals") == 0) 
		err = calibrations(req) ;
	else if(strcmp(filename, "/c_st_cal") == 0) 
		err = starter_calibration_page(req) ;
	else if(strcmp(filename, "/c_stop_st_cal") == 0) 
		err = stop_starter_calibration(req) ;
	else if(strcmp(filename, "/c_save_st_cal") == 0) 
		write_nvs_turbine() ;
	else if(strcmp(filename, "/c_start") == 0) 
		turbine.phase_fonctionnement = START ;
	else if(strcmp(filename, "/c_stop") == 0) 
		turbine.phase_fonctionnement = STOP ;
	else if(strcmp(filename, "/c_slider") == 0) 
		slider(req) ;
	else if(strcmp(filename, "/c_logs") == 0) 
		logs(req) ;
	else if(strcmp(filename, "/c_slider") == 0) 
		slider(req) ;
	else if(strcmp(filename, "/c_curves.txt") == 0) 
		curves_get_handler(req) ;
	else if(strcmp(filename, "/c_logs.txt") == 0) 
		logs_get_handler(req) ;
	else if(strcmp(filename, "/c_wifi") == 0) 
		wifi_get_handler(req) ;
	else if(strcmp(filename, "/c_stopwifi") == 0) {
		ESP_ERROR_CHECK(esp_wifi_stop() );
		vTaskDelete( xWebHandle ); }
	else if(strcmp(filename, "/c_gauges") == 0) 
	{
//		ESP_ERROR_CHECK( heap_trace_start(HEAP_TRACE_LEAKS) );
		gauges_get_handler(req) ;
		/*i++ ;
		if(i>10)
		{
			ESP_ERROR_CHECK( heap_trace_stop() );
			heap_trace_dump();
			i = 0 ;
		}*/
	}
	else if(strcmp(filename, "/c_readings") == 0) 
	{
		#ifdef IMU
			vTaskResume(xIMUHandle) ;
		#endif
		readings_get_handler(req) ;
	}
	else if(strcmp(filename, "/c_g_params") == 0) 
		g_params_get_handler(req) ;
	//else if(strcmp(filename, "/events") == 0) 
	//	events_get_handler(req) ;
	else if(strcmp(filename, "/c_upgrade") == 0) 
		upgrade_get_handler(req) ;
	else if(strcmp(filename, "/c_chart") == 0) 
		chart_get_handler(req) ;
	else if(strcmp(filename, "/c_chartdata") == 0) 
		g_chartdata_get_handler(req) ;
	// Courbe calibration du démarreur
	else if(strcmp(filename, "/c_chart_starter_cal") == 0) 
		g_chartstarter_get_handler(req) ;
	return err ;	
}

static esp_err_t frontpage(httpd_req_t *req)
{
	//ESP_LOGI(TAG, "root_get_handler req->uri=[%s]", req->uri);
    char filepath[20];
	//static int i=0 ;
    //ESP_LOGI(TAG, "frontapge - URI : %s", req->uri);
	xTimerStop( xTimer60s,0) ;
    const char *filename = get_path_from_uri(filepath, req->uri, sizeof(filepath));
	
	/*if (req->method != HTTP_GET)
    {
		ESP_LOGI(TAG, "Non HTTP_GET request method : %d",req->method);
		//ESP_LOGI(TAG, "frontpage req->uri=[%s]", req->uri);
		handle_ws_req(req);
	}*/

    if (!filename) 
    {
        ESP_LOGE(TAG, "Filename is too long");
        // retourne une erreur 500 (Internal Server Error)
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }
	//ESP_LOGI(TAG, "File : %s", filename);

	/*if(strcmp(filename, "/configecu") == 0) 
		configecu(req) ;
	else if(strcmp(filename, "/configmoteur") == 0) 
		configmoteur(req) ;
	else if(strcmp(filename, "/calibrations") == 0) 
		calibrations(req) ;
	else if(strcmp(filename, "/starter_calibration") == 0) 
		starter_calibration_page(req) ;
	else if(strcmp(filename, "/stop_starter_calibration") == 0) 
		stop_starter_calibration(req) ;
	else if(strcmp(filename, "/save_st_calibration") == 0) 
		write_nvs_turbine() ;
		
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
		//i++ ;
		//if(i>10)
		//{
		//	ESP_ERROR_CHECK( heap_trace_stop() );
		//	heap_trace_dump();
		//	i = 0 ;
		//}
	}
	else if(strcmp(filename, "/readings") == 0) 
	{
		vTaskResume(xIMUHandle) ;
		readings_get_handler(req) ;
	}
	else if(strcmp(filename, "/g_params") == 0) 
		g_params_get_handler(req) ;
	//else if(strcmp(filename, "/events") == 0) 
	//	events_get_handler(req) ;
	else if(strcmp(filename, "/upgrade") == 0) 
		upgrade_get_handler(req) ;
	else if(strcmp(filename, "/chart") == 0) 
		chart_get_handler(req) ;
	else if(strcmp(filename, "/chartdata") == 0) 
		g_chartdata_get_handler(req) ;
	// Courbe calibration du démarreur
	else if(strcmp(filename, "/chart_starter_cal") == 0) 
		g_chartstarter_get_handler(req) ;
	// Websocket 
	//else if(strcmp(filename, "/ws") == 0) 
	//	handle_ws_req(req) ;
	else if(strcmp(filename, "/") == 0 ) 
	{*/
		// Frontpage
	#ifdef IMU
		vTaskSuspend(xIMUHandle);
	#endif
	send_head(req) ;
	
	WSContentButton(req,BT_PARAMECU, true) ;
	WSContentButton(req,BT_PARAM_MOTEUR, true) ;
	WSContentButton(req,BT_INFORMATION, true) ;
	WSContentButton(req,BT_LOG, true) ;
	WSContentButton(req,BT_WIFI, true) ;
	WSContentButton(req,BT_SLIDER, true) ;
	WSContentButton(req,BT_JAUGES, true) ;
	WSContentButton(req,BT_CHART, true) ;
	WSContentButton(req,BT_FILES, true) ;
	WSContentButton(req,BT_MAJ, true) ;
	WSContentButton(req,BT_CUT_WIFI, true) ;
	WSContentButton(req,BT_START_ENGINE, true) ;
	WSContentButton(req,BT_STOP_ENGINE, true) ;
	

	Text2Html(req, "/html/footer.html");
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page
	//}

	return ESP_OK;
}

/* Function to start the web server */
esp_err_t start_server(const char *base_path, int port)
{
	
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
	httpd_uri_t _config_get_handler = {
		.uri		 = "/c_*",
		.method		 = HTTP_GET,
		.handler	 = configs, //root_get_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &_config_get_handler);

	httpd_uri_t _root_get_handler = {
		.uri		 = "/",
		.method		 = HTTP_GET,
		.handler	 = frontpage, //root_get_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &_root_get_handler);

	httpd_uri_t _ws_get_handler = {
		.uri		 = "/ws*",
		.method		 = HTTP_GET,
		.handler	 = handle_ws_req, //root_get_handler,
		//.user_ctx  = server_data	// Pass server data as context
		.is_websocket = true
	};
	httpd_register_uri_handler(server, &_ws_get_handler);

	

	/* URI handler for post */
	httpd_uri_t _root_post_handler = {
		.uri		 = "/p_*",
		.method		 = HTTP_POST,
		.handler	 = root_post_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &_root_post_handler);

	/******** File server ********************/
	/* URI handler for getting uploaded files */
	httpd_uri_t file_download = {
		.uri       = "/html*",  // Match all URIs of type /path/to/file
		.method    = HTTP_GET,
		.handler   = download_get_handler,
		.user_ctx  = server_data    // Pass server data as context
	};
	httpd_register_uri_handler(server, &file_download);

	/* URI handler for uploading files to server */
	httpd_uri_t file_upload = {
		.uri       = "/upload/*",   // Match all URIs of type /upload/path/to/file
		.method    = HTTP_POST,
		.handler   = upload_post_handler,
		.user_ctx  = server_data    // Pass server data as context
	};
	httpd_register_uri_handler(server, &file_upload);

	/* URI handler for deleting files from server */
	httpd_uri_t file_delete = {
		.uri       = "/delete/*",   // Match all URIs of type /delete/path/to/file
		.method    = HTTP_POST,
		.handler   = delete_post_handler,
		.user_ctx  = server_data    // Pass server data as context
	};
	httpd_register_uri_handler(server, &file_delete);

	//setup_websocket_server(server) ;
	ESP_LOGI(TAG, "HTTP Server Started");
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
	
	/* declare context for file server*/
	const char* base_path = "";
	

    if (server_data) {
        ESP_LOGE(TAG, "File server already started");
        //return ESP_ERR_INVALID_STATE;
    }

    /* Allocate memory for server data */
    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for server data");
        //return ESP_ERR_NO_MEM;
    }
    strlcpy(server_data->base_path, base_path,
            sizeof(server_data->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	/*                                     */

	ESP_LOGI(TAG, "Starting server on %s", url);
	ESP_ERROR_CHECK(start_server("/spiffs", CONFIG_WEB_PORT));
	http_task_start = NULL ;
	vTaskDelete(NULL) ;
	
	//URL_t urlBuf;
	while(1) {
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}

	// Never reach here
	ESP_LOGI(TAG, "finish");
	vTaskDelete(NULL);
}

/* File server functions */


/* Handler to redirect incoming GET request for /index.html to /
 * This can be overridden by uploading file with same name */
static esp_err_t index_html_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);  // Response body can be empty
    return ESP_OK;
}

/* Handler to redirect incoming GET request for /index.html to /
 * This can be overridden by uploading file with same name */
static esp_err_t path_html_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/html/");
    httpd_resp_send(req, NULL, 0);  // Response body can be empty
    return ESP_OK;
}


/* Send HTTP response with a run-time generated html consisting of
 * a list of all files and folders under the requested path.
 * In case of SPIFFS this returns empty list when path is any
 * string other than '/', since SPIFFS doesn't support directories */
static esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath)
{
    char entrypath[FILE_PATH_MAX];
    char entrysize[16];
	char URL[255] ;
    const char *entrytype;

    struct dirent *entry;
    struct stat entry_stat;

    DIR *dir = opendir(dirpath);
    const size_t dirpath_len = strlen(dirpath);

    /* Retrieve the base path of file storage to construct the full path */
    strlcpy(entrypath, dirpath, sizeof(entrypath));

    if (!dir) {
        ESP_LOGE(TAG, "Failed to stat dir : %s", dirpath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
        return ESP_FAIL;
    }

    /* Send HTML file header */
    send_head(req) ;
    /* Get handle to embedded file upload script */
	
    extern const unsigned char upload_script_start[] asm("_binary_upload_script_html_start");
    extern const unsigned char upload_script_end[]   asm("_binary_upload_script_html_end");
    const size_t upload_script_size = (upload_script_end - upload_script_start);

    /* Add file upload form and script which on execution sends a POST request to /upload */
    httpd_resp_send_chunk(req, (const char *)upload_script_start, upload_script_size);

    /* Send file-list table definition and column labels */
    httpd_resp_sendstr_chunk(req,
        "<table class=\"fixed\" border=\"1\">"
        "<col width=\"150px\" /><col width=\"50px\" /><col width=\"50px\" /><col width=\"50px\" />"
        "<thead><tr><th>Name</th><th>Type</th><th>Size (Bytes)</th><th>Delete</th></tr></thead>"
        "<tbody>");

    /* Iterate over all files / folders and fetch their names and sizes */
    while ((entry = readdir(dir)) != NULL) {
        entrytype = (entry->d_type == DT_DIR ? "directory" : "file");

        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
        if (stat(entrypath, &entry_stat) == -1) {
            ESP_LOGE(TAG, "Failed to stat %s : %s", entrytype, entry->d_name);
            continue;
        }
        sprintf(entrysize, "%ld", entry_stat.st_size);
        ESP_LOGI(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);

		
		strcpy(URL,req->uri) ;
		if(URL[strlen(URL)-1] == '?')
			URL[strlen(URL)-1] = '\0' ;

        /* Send chunk of HTML file containing table entries with file name and size */
        httpd_resp_sendstr_chunk(req, "<tr><td><a href=\"");
        httpd_resp_sendstr_chunk(req, URL);
        httpd_resp_sendstr_chunk(req, entry->d_name);
        if (entry->d_type == DT_DIR) {
            httpd_resp_sendstr_chunk(req, "/");
        }
        httpd_resp_sendstr_chunk(req, "\">");
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "</a></td><td>");
        httpd_resp_sendstr_chunk(req, entrytype);
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, entrysize);
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/delete");
        httpd_resp_sendstr_chunk(req, URL);
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Delete</button></form>");
        httpd_resp_sendstr_chunk(req, "</td></tr>\n");
    }
    closedir(dir);

    /* Finish the file list table */
    httpd_resp_sendstr_chunk(req, "</tbody></table>");

    /* Send remaining chunk of HTML file to complete it */
    //httpd_resp_sendstr_chunk(req, "</body></html>");

    /* Send empty chunk to signal HTTP response completion */
	WSRetourBouton(req) ;
	Text2Html(req, "/html/footer.html");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
static const char* get_path_from_uri_fs(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

/* Handler to download a file kept on the server */
static esp_err_t download_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;
	
    char *filename = get_path_from_uri_fs(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri, sizeof(filepath));
	
	update_curve_file() ;
	//ESP_LOGI(TAG,"filename : %s",filename) ;
	//filename[strlen(filename)-3] = '\0' ;
	//filepath[strlen(filepath)-3] = '\0' ;
    //ESP_LOGI(TAG,"filename : %s",filename) ;
	if (!filename) {
        ESP_LOGE(TAG, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }
	ESP_LOGI(TAG,"filepath : %s",filepath) ;
    /* If name has trailing '/', respond with directory contents */
    if (filename[strlen(filename) - 1] == '/') {
        return http_resp_dir_html(req, filepath);
    }

    if (stat(filepath, &file_stat) == -1) {
        /* If file not present on SPIFFS check if URI
         * corresponds to one of the hardcoded paths */
        if (strcmp(filename, "/index.html") == 0) {
            return index_html_get_handler(req);
        } else if (strcmp(filename, "/favicon.ico") == 0) {
            return favicon_get_handler(req);
        } else if (strcmp(filename, "/html/?") == 0) {
            return index_html_get_handler(req);
        }
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    set_content_type_from_file(req, filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    size_t chunksize;
    do {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        if (chunksize > 0) {
            /* Send the buffer contents as HTTP response chunk */
			httpd_resp_set_type(req, "application/octet-stream");
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                fclose(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
               return ESP_FAIL;
           }
        }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    /* Close file after sending complete */
    fclose(fd);
    ESP_LOGI(TAG, "File sending complete");

    /* Respond with an empty chunk to signal HTTP response completion */
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Handler to upload a file onto the server */
static esp_err_t upload_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    /* Skip leading "/upload" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri_fs(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri + sizeof("/upload") - 1, sizeof(filepath));
	
	ESP_LOGI(TAG, "upload filename : %s", filename);
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == 0) {
        ESP_LOGE(TAG, "File already exists : %s", filepath);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
        return ESP_FAIL;
    }

    /* File cannot be larger than a limit */
    if (req->content_len > MAX_FILE_SIZE) {
        ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than "
                            MAX_FILE_SIZE_STR "!");
        /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to create file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Receiving file : %s...", filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    int remaining = req->content_len;

    while (remaining > 0) {

        ESP_LOGI(TAG, "Remaining size : %d", remaining);
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry if timeout occurred */
                continue;
            }

            /* In case of unrecoverable error,
             * close and delete the unfinished file*/
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "File reception failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        /* Write buffer content to file on storage */
        if (received && (received != fwrite(buf, 1, received, fd))) {
            /* Couldn't write everything to file!
             * Storage may be full? */
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "File write failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        remaining -= received;
    }

    /* Close file upon upload completion */
    fclose(fd);
    ESP_LOGI(TAG, "File reception complete");

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/html/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;
}

/* Handler to delete a file from the server */
static esp_err_t delete_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    /* Skip leading "/delete" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri_fs(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri  + sizeof("/delete") - 1, sizeof(filepath));
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "File does not exist : %s", filename);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Deleting file : %s", filename);
    /* Delete file */
    unlink(filepath);

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/html/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File deleted successfully");
    return ESP_OK;
}


