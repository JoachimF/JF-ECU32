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
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/message_buffer.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "client_task";

#include "websocket_server.h"

MessageBufferHandle_t xMessageBufferToClient;



void client_task(void* pvParameters) {
	int32_t task_parameter = (int32_t)pvParameters;
	ESP_LOGI(TAG,"Starting. task_parameter=0x%"PRIx32, task_parameter);

	char meter1[8];
	memset(meter1, 0, sizeof(meter1));
	if (task_parameter & 0x0001) strcpy(meter1, "ROLL");
	char meter2[8];
	memset(meter2, 0, sizeof(meter2));
	if (task_parameter & 0x0010) strcpy(meter2, "PITCH");
	char meter3[8];
	memset(meter3, 0, sizeof(meter3));
	if (task_parameter & 0x0100) strcpy(meter3, "YAW");

	char cRxBuffer[512];
	char DEL = 0x04;
	char outBuffer[64];

	while (1) {
		size_t readBytes = xMessageBufferReceive(xMessageBufferToClient, cRxBuffer, sizeof(cRxBuffer), portMAX_DELAY );
		ESP_LOGD(TAG, "readBytes=%d", readBytes);
		ESP_LOGD(TAG, "cRxBuffer=[%.*s]", readBytes, cRxBuffer);
		cJSON *root = cJSON_Parse(cRxBuffer);
		if (cJSON_GetObjectItem(root, "id")) {
			char *id = cJSON_GetObjectItem(root,"id")->valuestring;
			ESP_LOGD(TAG, "id=[%s]",id);

			if ( strcmp (id, "init") == 0) {
				sprintf(outBuffer,"HEAD%cEuler Display using ESP32", DEL);
				ESP_LOGD(TAG, "outBuffer=[%s]", outBuffer);
				ws_server_send_text_all(outBuffer,strlen(outBuffer));

				sprintf(outBuffer,"METER%c%s%c%s%c%s", DEL,meter1,DEL,meter2,DEL,meter3);
				ESP_LOGD(TAG, "outBuffer=[%s]", outBuffer);
				ws_server_send_text_all(outBuffer,strlen(outBuffer));
			} // end if

			if ( strcmp (id, "data-request") == 0) {
				double roll = cJSON_GetObjectItem(root,"roll")->valuedouble;
				double pitch = cJSON_GetObjectItem(root,"pitch")->valuedouble;
				double yaw = cJSON_GetObjectItem(root,"yaw")->valuedouble;
				ESP_LOGD(TAG,"roll=%f pitch=%f yaw=%f", roll, pitch, yaw);

				sprintf(outBuffer,"DATA%c%f%c%f%c%f", DEL, roll, DEL, pitch, DEL, yaw);
				ESP_LOGD(TAG, "outBuffer=[%s]", outBuffer);
				ws_server_send_text_all(outBuffer,strlen(outBuffer));

				ESP_LOGD(TAG,"free_size:%d %d", heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_free_size(MALLOC_CAP_32BIT));

			} // end if
		} // end if

		// Delete a cJSON structure
		cJSON_Delete(root);

	} // end while

	// Never reach here
	vTaskDelete(NULL);
}

void websocket_callback(uint8_t num,WEBSOCKET_TYPE_t type,char* msg,uint64_t len) {
	const static char* TAG = "websocket_callback";
	//int value;

	switch(type) {
		case WEBSOCKET_CONNECT:
			ESP_LOGI(TAG,"client %i connected!",num);
			break;
		case WEBSOCKET_DISCONNECT_EXTERNAL:
			ESP_LOGI(TAG,"client %i sent a disconnect message",num);
			break;
		case WEBSOCKET_DISCONNECT_INTERNAL:
			ESP_LOGI(TAG,"client %i was disconnected",num);
			break;
		case WEBSOCKET_DISCONNECT_ERROR:
			ESP_LOGI(TAG,"client %i was disconnected due to an error",num);
			break;
		case WEBSOCKET_TEXT:
			if(len) { // if the message length was greater than zero
				ESP_LOGI(TAG, "got message length %i: %s", (int)len, msg);
				size_t xBytesSent = xMessageBufferSendFromISR(xMessageBufferToClient, msg, len, NULL);
				if (xBytesSent != len) {
					ESP_LOGE(TAG, "xMessageBufferSend fail");
				}
			}
			break;
		case WEBSOCKET_BIN:
			ESP_LOGI(TAG,"client %i sent binary message of size %"PRIu32":\n%s",num,(uint32_t)len,msg);
			break;
		case WEBSOCKET_PING:
			ESP_LOGI(TAG,"client %i pinged us with message of size %"PRIu32":\n%s",num,(uint32_t)len,msg);
			break;
		case WEBSOCKET_PONG:
			ESP_LOGI(TAG,"client %i responded to the ping",num);
			break;
	}
}

void init_websocket(void)
{
	// Create Message Buffer
	xMessageBufferToClient = xMessageBufferCreate(1024);
	configASSERT( xMessageBufferToClient );

    // Start websocket task
	xTaskCreate(&client_task, "CLIENT", 1024*3, (void *)0x011, 5, NULL);

}