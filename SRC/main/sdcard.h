/*  
  sdcard.h

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

#ifndef _SDCARD_H_
#define _SDCARD_H_

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#define PIN_NUM_MISO  19
#define PIN_NUM_MOSI  23
#define PIN_NUM_CLK   18
#define PIN_NUM_CS    16
#define PIN_NUM_DET   39 //VN_SENSOR

#define MOUNT_POINT "/sdcard"
#define EXAMPLE_MAX_CHAR_SIZE    64

void init_sdcard(sdmmc_card_t *card) ;

#endif