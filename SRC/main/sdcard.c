/*
  SDCARD.c

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

#include "sdcard.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_err.h"
#include <dirent.h>

//#include "sd_test_io.h"

static const char *TAG = "SDCard";
FILE *log_file ;


static esp_err_t s_example_write_file(const char *path, char *data)
{
    ESP_LOGI(TAG, "Opening file %s", path);
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    fprintf(f, data);
    fclose(f);
    ESP_LOGI(TAG, "File written");

    return ESP_OK;
}

static esp_err_t s_example_read_file(const char *path)
{
    ESP_LOGI(TAG, "Reading file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }
    char line[EXAMPLE_MAX_CHAR_SIZE];
    fgets(line, sizeof(line), f);
    fclose(f);

    // strip newline
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    return ESP_OK;
}

esp_err_t init_sdcard(sdmmc_card_t *card2)
{
    esp_err_t ret;

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#if 0
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card ;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");
    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.
    ESP_LOGI(TAG, "Using SPI peripheral");

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 20MHz for SDSPI)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI_PORT ;
    host.max_freq_khz = 15000 ;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    
    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA ) ;//SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return ret ;
    }

    // This initializes the slot with card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;
    //slot_config.gpio_cd =  PIN_NUM_DET;
    
    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
#ifdef CONFIG_EXAMPLE_DEBUG_PIN_CONNECTIONS
            check_sd_card_pins(&config, pin_count);
#endif
        }
        return ret ;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    // Use POSIX and C standard library functions to work with files.

    // All done, unmount partition and disable SPI peripheral
    //esp_vfs_fat_sdcard_unmount(mount_point, card);
    //ESP_LOGI(TAG, "Card unmounted");

    //deinitialize the bus after all devices are removed
    //spi_bus_free(host.slot);

    DIR *dir = opendir(LOGPATH);
    if (!dir) {
        ESP_LOGI(TAG, "Création de %s", LOGPATH);
        int mk_ret = mkdir(LOGPATH,0755) ;
        ESP_LOGI(TAG, "mkdir ret %d", mk_ret);
    }
    file_space() ;
    return ret ;    
}

int file_space(void)
{
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    /* Get volume information and free clusters of drive 0 */
    int res = f_getfree("/sdcard", &fre_clust, &fs);
    /* Get total sectors and free sectors */
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;
    /* Print the free space (assuming 512 bytes/sector) */
    ESP_LOGI(TAG,"%10u KiB total drive space.\r\n%10u KiB available.\r\n%10u free clust.\r\n",tot_sect / 2, fre_sect / 2,fre_clust);
    return (fre_sect / 2);
}

static int print_to_sd_card(const char *fmt, va_list list)
{
    static int counter = 0 ;
    if (log_file == NULL) {
        return -1;
    }
    int res = vfprintf(log_file, fmt, list);
    res = vprintf(fmt,list) ;

    // Committing changes to the file on each write is slower,
    // but ensures that no data will be lost.
    //
    // You may have to figure out when to call fsync.
    // For example, only call fsync after every 50 log messages,
    // or after 100ms passed since last fsync, and so on.
    if(counter < 10 ) {
        counter ++ ;
    } else {
        fsync(fileno(log_file));
        counter = 0 ;
    }
    return res;
}

void redirect_sytems_logs(void)
{
    ESP_LOGI(TAG,"Redirection to SDCard");
    log_file = fopen(LOGPATH "/"SYSTEM_LOG_FILE,"a") ;
    if(log_file) {
        ESP_LOGI(TAG,"Now redirecting stdout to log file as well");
        esp_log_set_vprintf(&print_to_sd_card);
        // Save UART stdout stream
        //FILE* uart_stdout = stdout;
        // Change stdout for THIS TASK ONLY
        //stdout = log_file;
        // Change stdout for all new tasks which will be created
        //_GLOBAL_REENT->_stdout = log_file;
        printf("SD Log start");
        ESP_LOGI(TAG,"SD Log start");
    } else {
        ESP_LOGI(TAG,"Unable to redirect LOG");
    }
}