/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* HTTP File Server Example, common declarations

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

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


#ifdef __cplusplus
}
#endif
