/*  http_server.h

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

#ifndef __HTTP_SERVER_H_
#define __HTTP_SERVER_H_

#include "freertos/FreeRTOS.h"
#include "esp_http_server.h"
#include "esp_err.h"
#include "esp_vfs.h"

extern httpd_handle_t server ;
extern TickType_t Ticks ;

typedef struct {
	char url[32];
	char parameter[128];
} URL_t;

/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (200*1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  8192

struct file_server_data {
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};

static esp_err_t delete_post_handler(httpd_req_t *req) ;
static esp_err_t upload_post_handler(httpd_req_t *req) ;
static esp_err_t download_get_handler(httpd_req_t *req) ;
static esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath) ;

//#define MIN(iA,iB)  ((iA)<(iB) ? (iA) : (iB))
const char* get_path_from_uri(char *, const char *, size_t ) ;

//const char* get_path_from_uri(char *dest, const char *uri, size_t destsize) ;

#endif