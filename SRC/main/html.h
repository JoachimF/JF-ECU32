/*  html.h

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
#ifndef __HTML_H_
#define __HTML_H_

#include <stdio.h>
#include "esp_http_server.h"

#define MIN_NAME 1
#define MIN_GLOWPOWER 0
#define MIN_RPMMAX 0
#define MIN_RPMIDLE 0
#define MIN_RPMMIN 0
#define MIN_TEMPSTART 0
#define MIN_TEMPMAX 0
#define MIN_DELAYACC 0
#define MIN_DELAYDEC 0
#define MIN_DELAYSTAB 0
#define MIN_PUMP1MAX 0
#define MIN_PUMP1MIN 0
#define MIN_PUMP2MAX 0
#define MIN_PUMP2MIN 0
#define MIN_VANNE1MAX 0
#define MIN_VANNE2MAX 0
#define MIN_RPMSTARTER 0
#define MIN_RPMMAXSTARTER 0
#define MIN_LIPO_ELEMENTS 2
#define MIN_VMIN_START 7

#define MAX_NAME 20
#define MAX_GLOWPOWER 255
#define MAX_RPMMAX 300000
#define MAX_RPMIDLE 100000
#define MAX_RPMMIN 100000
#define MAX_TEMPSTART 500
#define MAX_TEMPMAX 1000
#define MAX_DELAYACC 30
#define MAX_DELAYDEC 30
#define MAX_DELAYSTAB 30
#define MAX_PUMP1MAX 1024
#define MAX_PUMP1MIN 512
#define MAX_PUMP2MAX 1024
#define MAX_PUMP2MIN 512
#define MAX_VANNE1MAX 255
#define MAX_VANNE2MAX 255
#define MAX_RPMSTARTER 10000
#define MAX_RPMMAXSTARTER 30000
#define MAX_VMIN_START 13
#define MAX_LIPO_ELEMENTS 3

#define NBFrontpageButton 11
#define NBParamEcuRadio 7

extern const char htmlInputParamEng[] ;
extern const char htmlRadioParamEcu[] ;
extern const char htmlCheckParamEcu[] ;

enum BoutonFrontpageEnum {
  BT_PARAMECU, BT_PARAM_MOTEUR,
  BT_INFORMATION, BT_LOG, BT_WIFI, BT_SLIDER, BT_JAUGES, BT_CALIBRATION, BT_STARTCALIBRATION, BT_STOPCALIBRATION,BT_SAVE_ST_CAL,
  BT_CHART,
  BT_START_ENGINE, BT_STOP_ENGINE, BT_FILES, BT_MAJ, BT_CUT_WIFI 
} ;

enum ParamEcuRadiosEnum{
  R_THROTTLE_TYP,R_GLOW_TYP,R_START_TYP,R_PUMP1_TYP,
  R_STARTER_TYP,R_TELEM_TYP,R_PUMP2_TYP
} ;

enum ParamEcuCheckEnum{
  C_AUX_EN,C_LEDS_EN
} ;

enum InputParamEng {
  I_NAME,I_GLOWPOWER,
  I_RPMMAX,I_RPMIDLE,I_RPMMIN,
  I_TEMPSTART,I_TEMPMAX,
  I_DELAYACC,I_DELAYDEC,I_DELAYSTAB,
  I_PUMP1MAX,I_PUMP1MIN,I_PUMP2MAX,I_PUMP2MIN,
  I_VANNE1MAX,I_VANNE2MAX,
  I_RPMSTARTER,I_MAXSTARTERRPM,
  I_LIPO_ELEMENTS,I_VMIN_START
} ;

enum InputParamTyp{
    NUMBER,TEXT
};

void WSContentButton(httpd_req_t *req,uint32_t title_index, bool show) ;
void WSRadio(httpd_req_t *req,uint32_t title_index,uint8_t param, bool show) ;
void WSCheckBox(httpd_req_t *req,uint32_t title_index,uint8_t param, bool show) ;
void WSInputBox(httpd_req_t *req,uint32_t title_index,uint32_t intparam,char *charparam, int type, bool show);
void WSSaveBouton(httpd_req_t *req) ;
void WSRetourBouton(httpd_req_t *req) ;
char* GetTextIndexed(char* destination, size_t destination_size, uint32_t index, const char* haystack) ;
void WSBouton(httpd_req_t *req,int bouton) ;

#endif