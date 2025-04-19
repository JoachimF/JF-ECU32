/*  
  websocket.c

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
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
//#include "freertos/queue.h"
//#include "freertos/event_groups.h"
#include "freertos/message_buffer.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"

#include "jf-ecu32.h"
#include "inputs.h"
#include "mpu6050.h"
#include "http_server.h"
#include "error.h"
#ifdef IMU
	#include "imu.h"
#endif
#include "outputs.h"



static const char *TAG = "Websocket";

TaskHandle_t xWSHandle ;

#include "websocket_server.h"

struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

extern MessageBufferHandle_t xMessageBufferToClient;

void ws_task(void* pvParameters) {
	
    httpd_ws_frame_t ws_pkt;
	cJSON *myjson;
	char status[50] ;
	char errors[200] ;
	char tmp[20] ;
	uint8_t heures,minutes,secondes ;
	char *my_json_string ;
	mpu6050_acce_value_t acce;	
    
	ESP_LOGI(TAG, "WS_TASK start") ;

    static size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
    size_t fds = max_clients;
	ESP_LOGI(TAG, "WS_TASK Max client = %d",fds) ;
    int client_fds[max_clients];
	int client_info ;
	esp_err_t ret ;

    while(1)
	{
		ESP_LOGD(TAG, "Loop start") ;

		ESP_LOGD(TAG, "Time") ;
		get_time_total(&turbine,&secondes,&minutes,&heures) ;
		ESP_LOGD(TAG, "Phase") ;
		phase_to_str(status) ;
		
		ESP_LOGD(TAG, "JSON") ;
		myjson = cJSON_CreateObject();
		cJSON_AddNumberToObject(myjson, "ppm_gaz", get_gaz(&turbine));
		cJSON_AddNumberToObject(myjson, "ppm_aux", get_aux(&turbine));
		cJSON_AddNumberToObject(myjson, "egt", get_EGT(&turbine));
		cJSON_AddNumberToObject(myjson, "rpm", get_RPM(&turbine));
		sprintf(tmp,"%0.1f%%",get_power(&turbine.pump1));
		cJSON_AddStringToObject(myjson, "pump1",tmp );
		sprintf(tmp,"%0.1f%%",get_power(&turbine.pump2));
		cJSON_AddStringToObject(myjson, "pump2",tmp );
//		cJSON_AddNumberToObject(myjson, "pump1", get_power(&turbine.pump1));
//		cJSON_AddNumberToObject(myjson, "pump2", get_power(&turbine.pump2));
		cJSON_AddNumberToObject(myjson, "vanne1", get_vanne_power(&turbine.vanne1));
		cJSON_AddNumberToObject(myjson, "vanne2", get_vanne_power(&turbine.vanne2));
		cJSON_AddNumberToObject(myjson, "glow", get_glow_power(&turbine.glow));
		sprintf(tmp,"%0.3fA",get_glow_current(&turbine.glow));
		cJSON_AddStringToObject(myjson, "glowcurrent",tmp );
		sprintf(tmp,"%0.3fV",get_vbatt(&turbine));
		cJSON_AddStringToObject(myjson, "vbatt",tmp );
		//cJSON_AddNumberToObject(myjson, "starter", get_power(&turbine.starter));
		sprintf(tmp,"%0.1f%%",get_power(&turbine.starter));
		cJSON_AddStringToObject(myjson, "starter",tmp );
		cJSON_AddStringToObject(myjson, "status", status);
		ESP_LOGD(TAG, "Get errors") ;
		get_errors(errors); 
		cJSON_AddStringToObject(myjson, "error", errors);
		sprintf(status,"%02d:%02d:%02d",heures,minutes,secondes);
		cJSON_AddStringToObject(myjson, "time", status);
		ESP_LOGD(TAG, "Get tick") ;
		cJSON_AddNumberToObject(myjson, "ticks",xTaskGetTickCount() - Ticks);
		//ESP_LOGD(TAG, "Tick = %d",xTaskGetTickCount() - Ticks) ;
		
		
		/*
		if(xQueueReceive(xQueueIMU, &acce, portMAX_DELAY)) {
			cJSON_AddNumberToObject(myjson, "accx", acce.acce_x);
			cJSON_AddNumberToObject(myjson, "accy", acce.acce_y);
			cJSON_AddNumberToObject(myjson, "accz", acce.acce_z);
		}else {
				ESP_LOGE(TAG, "xQueueReceive fail");
		}*/

		ESP_LOGD(TAG, "ws_pkt") ;
		memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
	    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
		my_json_string = cJSON_Print(myjson);
	    ws_pkt.payload = (uint8_t *)my_json_string;
    	ws_pkt.len = strlen(my_json_string);
		//ESP_LOGI(TAG, "JSON: %s",my_json_string);
		ESP_LOGD(TAG, "httpd_get_client_list");
		ESP_LOGD(TAG, "iN TASK SERVER HANDLE : %ld", server);
		struct httpd_data *hd = (struct httpd_data *) server;
		if (hd == NULL || fds == 0 || client_fds == NULL) {
        	ESP_LOGD(TAG, "hd ou fds ou client_fds est null fds: %d",fds);
    	}
		ESP_LOGD(TAG, "fds: %d",fds);
		fds = CONFIG_LWIP_MAX_LISTENING_TCP ;
		
		ret = httpd_get_client_list(server, &fds, client_fds);
		if (ret != ESP_OK) {
			ESP_LOGE(TAG, "Plus de client - pause");
			vTaskSuspend(xWSHandle);
			//return;
		} else {
			for (int i = 0; i < fds; i++) {
				client_info = httpd_ws_get_fd_info(server, client_fds[i]);
				if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
					//ESP_LOGI(TAG, "httpd_ws_send_frame_async");
					httpd_ws_send_frame_async(server, client_fds[i], &ws_pkt);
				}
			}
		}
		cJSON_Delete(myjson) ;
		free(my_json_string) ; 
		vTaskDelay(100/portTICK_PERIOD_MS);
	}	
	vTaskDelete(NULL);
}
/*void ws_task(void* pvParameters) {
	int32_t task_parameter = (int32_t)pvParameters;
	ESP_LOGI(TAG,"Starting. task_parameter=0x%"PRIx32, task_parameter);

	char cRxBuffer[512];
	char DEL = 0x04;
	char outBuffer[64];

	while (1) {
		size_t readBytes = xMessageBufferReceive(xMessageBufferToClient, cRxBuffer, sizeof(cRxBuffer), portMAX_DELAY );
		ESP_LOGI(TAG, "readBytes=%d", readBytes);
		ESP_LOGI(TAG, "cRxBuffer=[%.*s]", readBytes, cRxBuffer);
		cJSON *root = cJSON_Parse(cRxBuffer);
		if (cJSON_GetObjectItem(root, "id")) {
			char *id = cJSON_GetObjectItem(root,"id")->valuestring;
			ESP_LOGI(TAG, "id=[%s]",id);

			if ( strcmp (id, "init") == 0) {
				sprintf(outBuffer,"HEAD%cWebsocket using ESP32", DEL);
				ESP_LOGI(TAG, "outBuffer=[%s]", outBuffer);
				ws_async_send(req->handle,outBuffer) ;

			} // end if

			if ( strcmp (id, "data-request") == 0) {

				sprintf(outBuffer,"DATA%c%ld%c%ld%c%ld%c%ld%c%f%c%f%c%u%c%u%c%u",DEL,get_gaz(&turbine),DEL,get_aux(&turbine),DEL,get_EGT(&turbine),DEL,get_EGT(&turbine),DEL,get_power(&turbine.pump1),DEL,get_power(&turbine.pump2),DEL,get_vanne_power(&turbine.vanne1),DEL,get_vanne_power(&turbine.vanne2),DEL,get_glow_power(&turbine.glow));
				ESP_LOGI(TAG, "outBuffer=[%s]", outBuffer);
				ws_async_send(req->handle,outBuffer) ;

//				ESP_LOGI(TAG,"free_size:%d %d", heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_free_size(MALLOC_CAP_32BIT));

			} // end if
		} // end if

		// Delete a cJSON structure
		cJSON_Delete(root);
		vTaskDelay(10/portTICK_PERIOD_MS);
	} // end while

	// Never reach here
	vTaskDelete(NULL);
}*/

/*static void ws_async_send(void *arg)
{
    httpd_ws_frame_t ws_pkt;
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;

    //led_state = !led_state;
    //gpio_set_level(LED_PIN, led_state);
    
    char buff[128];
	char DEL = 0x04;

    //memset(buff, 0, sizeof(buff));
    //sprintf(buff, "%d",led_state);
    //sprintf(buff,"HEAD%cWebsocket using ESP32", DEL);
	

	sprintf(buff,"DATA%c%ld%c%ld%c%ld%c%ld%c%f%c%f%c%u%c%u%c%u",DEL,get_gaz(&turbine),DEL,get_aux(&turbine),DEL,get_EGT(&turbine),DEL,get_EGT(&turbine),DEL,get_power(&turbine.pump1),DEL,get_power(&turbine.pump2),DEL,get_vanne_power(&turbine.vanne1),DEL,get_vanne_power(&turbine.vanne2),DEL,get_glow_power(&turbine.glow));
    ESP_LOGI(TAG, "outBuffer=[%s]", buff);
	memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)buff;
    ws_pkt.len = strlen(buff);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    static size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
    size_t fds = max_clients;
    int client_fds[max_clients];

    esp_err_t ret = httpd_get_client_list(server, &fds, client_fds);

    if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Erreur dans la récupération des clients");
        return;
    }
	ESP_LOGI(TAG, "httpd_get_client_list");
    for (int i = 0; i < fds; i++) {
        int client_info = httpd_ws_get_fd_info(server, client_fds[i]);
        if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
			ESP_LOGI(TAG, "httpd_ws_send_frame_async");
            httpd_ws_send_frame_async(hd, client_fds[i], &ws_pkt);
        }
    }
    free(resp_arg);
}*/

/*static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
{
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    return httpd_queue_work(handle, ws_async_send, resp_arg);
}*/


esp_err_t handle_ws_req(httpd_req_t *req)
{
	if (req->method == HTTP_GET)
    {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
       return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
	
	ESP_LOGI(TAG, "Zeroing ws frame");
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
	//ws_pkt.payload = buf;
	ESP_LOGI(TAG, "Reading ws frame");
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    
	if (ws_pkt.len)
    {
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL)
        {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
		ESP_LOGI(TAG, "Got packet with type: %d", ws_pkt.type);
    }

    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
	ESP_LOGI(TAG, "match with ini %d", strcmp((char *)ws_pkt.payload, "init"));

/*    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && strstr((char *)ws_pkt.payload, "init"))
    {
        free(buf);
		ESP_LOGI(TAG, "Réponse à init");
        return trigger_async_send(req->handle, req);
    }
*/
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && strstr((char *)ws_pkt.payload, "init"))
    {
		//char outBuffer[128];
		//char DEL = 0x04;
		ESP_LOGI(TAG, "Wake up WS_Task");
		ESP_LOGI(TAG, "iN ws_handler SERVER HANDLE : %ld", req->handle);
		vTaskResume(xWSHandle);
		//return trigger_async_send(req->handle,req) ;
	}/*	
	if (ws_pkt.type == HTTPD_WS_TYPE_TEXT)
    {
	size_t xBytesSent = xMessageBufferSend(xMessageBufferToClient, ws_pkt.payload, strlen(ws_pkt.payload), 100);
			if (xBytesSent != strlen(ws_pkt.payload)) {
				ESP_LOGE(TAG, "xMessageBufferSend fail");
			}
	}*/
    return ESP_OK;
}


/* Start Websocket server */

/*void setup_websocket_server(void)
{
	// URI Handler for Websocket 
	httpd_uri_t ws = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_ws_req,
        .user_ctx = NULL,
        .is_websocket = true};

	httpd_register_uri_handler(server, &ws);
	
	xMessageBufferToClient = xMessageBufferCreate(1024);
    ESP_LOGI(TAG, "Serveur WS créé") ;
}*/
