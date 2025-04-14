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
#include "Langues/fr_FR.h"
#include "html.h"


#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "jf-ecu32.h"

const uint16_t CHUNKED_BUFFER_SIZE = 500;                // Chunk buffer size (needs to be well below stack space (4k for ESP8266, 8k for ESP32) but large enough to cache some small messages)

#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
//#define PSTR(x) (x)
//#define nullptr                                     NULL

//const char HTTP_SNS_HR[] = "<tr><td colspan=2 style='font-size:2px'><hr/></td></tr>";

const char htmlRadioParamEcu[]  =
  RD_THROTTLE_TYP "|" RD_GLOW_TYP "|" RD_START_TYP "|" RD_PUMP1_TYP "|" 
  RD_STARTER_TYP "|" RD_TELEM_TYP "|" RD_PUMP2_TYP ;

const int htmlRadioParamEcuChoices[] =
  {2,2,2,2,2,4,3} ;

const char htmlRadioParamEcuChoicesTxt[]  =
  CT_THROTTLE_STD "&" CT_THROTTLE_SBUS "|"
  CT_GLOW_GAZ "&" CT_GLOW_KERO "|"
  CT_START_MAN "&" CT_START_AUTO "|"
  CT_PUMP1_DC "&" CT_PUMP1_PPM "|" 
  CT_STARTER_DC "&" CT_STARTER_PPM "|"
  CT_TELEM_DIS "&" CT_TELEM_FR "&" CT_TELEM_FUT "&" CT_TELEM_GRA "|"
  CT_PUMP2_DC "&" CT_PUMP2_PPM "&" CT_PUMP2_DIS;

const char htmlCheckParamEcu[]  =
  CH_AUX_EN "|" CH_LEDS_EN ;


const char htmlBoutonFrontpage[]  =
  B_PARAMECU "|" B_PARAM_MOTEUR "|"
  B_INFORMATION "|" B_LOG "|" B_WIFI "|" B_SLIDER "|" B_JAUGES "|"
  B_CALIBRATION "|" B_STARTCALIBRATION "|" B_STOPCALIBRATION "|" B_SAVE "|"
  B_CHART "|"
  B_START_ENGINE "|" B_STOP_ENGINE"|" B_FILES "|" B_MAJ "|" B_CUT_WIFI ;


const char BoutonFrontpageAction[]  =
  "c_ecu|c_moteur|"
  "c_info|sdcard/logs/|c_wifi|c_slider|c_gauges|c_cals|c_st_cal|c_stop_st_cal|c_save_st_cal|"
  "c_chart|"
  "c_start|c_stop|html/|c_upgrade|c_stopwifi";

const char htmlInputParamEng[] =
  IN_NAME "|" IN_GLOWPOWER "|" 
  IN_RPMMAX "|" IN_RPMIDLE "|" IN_RPMMIN "|" 
  IN_TEMPSTART "|" IN_TEMPMAX "|" 
  IN_DELAYACC "|" IN_DELAYDEC "|" IN_DELAYSTAB "|" 
  IN_PUMP1MAX "|" IN_PUMP1MIN "|" IN_PUMP2MAX "|" IN_PUMP2MIN "|" 
  IN_VANNE1MAX "|" IN_VANNE2MAX "|" 
  IN_RPMSTARTER "|" IN_MAXSTARTERRPM "|" IN_LIPO_ELEMENTS "|" IN_VMIN_START;

const uint32_t htmlParamEngMinMax[2][20] =
 {{MIN_NAME,MIN_GLOWPOWER,
  MIN_RPMMAX,MIN_RPMIDLE,MIN_RPMMIN,
  MIN_TEMPSTART,MIN_TEMPMAX,
  MIN_DELAYACC,MIN_DELAYDEC,MIN_DELAYSTAB,
  MIN_PUMP1MAX,MIN_PUMP1MIN,MIN_PUMP2MAX,MIN_PUMP2MIN,
  MIN_VANNE1MAX,MIN_VANNE2MAX,
  MIN_RPMSTARTER,MIN_RPMMAXSTARTER,
  MIN_LIPO_ELEMENTS,MIN_VMIN_START},
  {MAX_NAME,MAX_GLOWPOWER,
  MAX_RPMMAX,MAX_RPMIDLE,MAX_RPMMIN,
  MAX_TEMPSTART,MAX_TEMPMAX,
  MAX_DELAYACC,MAX_DELAYDEC,MAX_DELAYSTAB,
  MAX_PUMP1MAX,MAX_PUMP1MIN,MAX_PUMP2MAX,MAX_PUMP2MIN,
  MAX_VANNE1MAX,MAX_VANNE2MAX,
  MAX_RPMSTARTER,MAX_RPMMAXSTARTER,
  MAX_LIPO_ELEMENTS,MAX_VMIN_START}} ;

enum BoutonParmEcuEnum {
  BT_SAVE, BT_RETOUR  };

const char htmlBoutonParamEcu[]  =
  B_SAVE "|" B_RETOUR  ;

const char htmlBoutonParamEcuAction[]  =
  "|c_ecu|/|";

const char kButtonConfirm[]  = B_CONFIRM_SAVE ;

char* GetTextChoice(char* destination, size_t destination_size, uint32_t rdindex, uint32_t chindex, const char* haystack)
{
  char* write = destination;
  const char* read = haystack;
  int searchindex = 0 ;
  
  if(rdindex > 0) //cherche les choix en fonction de l'index entre |
    while (rdindex && read[searchindex] != '\0') {
      if(read[searchindex++] == '|')
        rdindex-- ;
    }

  if(chindex > 0) //cherche le choix en fonction de l'index entre &
    while (chindex && (read[searchindex] != '\0')) {
      if(read[searchindex++] == '&')
        chindex-- ;
    }
  //copy le choix en fonction entre &
  while ((read[searchindex] != '&') && (read[searchindex] != '\0') && (read[searchindex] != '|')) {
      *write++ = read[searchindex++] ;
    }

  if(chindex || rdindex)
    write = destination;
  *write = '\0' ;
  return destination;
}

char* GetTextIndexed(char* destination, size_t destination_size, uint32_t index, const char* haystack)
{                       // dest             100                   10                "titre1|titre2|titre3....."
  // Returns empty string if not found
  // Returns text of found
  char* write = destination;
  const char* read = haystack;

  index++; // 11
  while (index--) {// 10
    size_t size = destination_size -1; //99
    write = destination;  
    char ch = '.';
    while ((ch != '\0') && (ch != '|')) {
      ch = pgm_read_byte(read++); //ch = t
      if (size && (ch != '|'))  {
        *write++ = ch;
        size--;
      }
    }
    if (0 == ch) {
      if (index) {
        write = destination;
      }
      break;
    }
  }
  *write = '\0';
  return destination;
}

// Boutons de la frontpage
void WSContentButton(httpd_req_t *req,uint32_t title_index, bool show) {
  char action[20];
  char title[100]; 
  char buffer[500] ;

  sprintf(buffer,"<form id=but%ld style=\"display: %s;\" method=\"GET\" action=\"%s\"",
  title_index,show ? "block":"none",GetTextIndexed(action, sizeof(action), title_index, BoutonFrontpageAction));
  httpd_resp_sendstr_chunk(req,buffer) ;

  if (title_index == BT_START_ENGINE || title_index == BT_CUT_WIFI) {
    //char confirm[100];
    sprintf(buffer," onsubmit='return confirm(\"%s %s?\");'><button name='red_%ld' class='button bred'>%s</button></form></p>",
      SURE,GetTextIndexed(title, sizeof(title), title_index, htmlBoutonFrontpage),
      title_index,
      GetTextIndexed(title, sizeof(title), title_index, htmlBoutonFrontpage));
    httpd_resp_sendstr_chunk(req,buffer) ;

  } else {
    sprintf(buffer,"><button>%s</button></form><p></p>",
      GetTextIndexed(title, sizeof(title), title_index, htmlBoutonFrontpage));
    httpd_resp_sendstr_chunk(req,buffer) ;
  }
}

void WSRadio(httpd_req_t *req,uint32_t title_index,uint8_t param, bool show)
{
  char title[100];
  char buffer[500] ;
  int choices = htmlRadioParamEcuChoices[title_index] ;
  
  sprintf(buffer,"<fieldset><legend><b>&nbsp;%s</b></legend>",GetTextIndexed(title, sizeof(title), title_index, htmlRadioParamEcu)) ;
  httpd_resp_sendstr_chunk(req,buffer) ;
  for(int i=0;i < choices ; i++)
  {
    sprintf(buffer,"<p><input id=\"rd_input_%ld_%d\" name=\"rd_input_%ld\" type=\"radio\" value=\"%d\"",title_index,i,title_index,i) ;
    sprintf(buffer+strlen(buffer),(param == i) ? "checked=\"\"" : " ") ;
    sprintf(buffer+strlen(buffer),"><b>%s</b>",GetTextChoice(title, sizeof(title), title_index,i, htmlRadioParamEcuChoicesTxt)) ;
    httpd_resp_sendstr_chunk(req,buffer) ;
  }
  httpd_resp_sendstr_chunk(req, "</fieldset><p>") ;
  ESP_LOGI("CONFIG_ECU","%s : %d",GetTextIndexed(title, sizeof(title), title_index,htmlRadioParamEcuChoicesTxt),param) ;
}

void WSCheckBox(httpd_req_t *req,uint32_t title_index,uint8_t param, bool show)
{
  char title[100];
  char buffer[500] ;
  sprintf(buffer,"<p><input id=\"ch_input%ld\" type=\"checkbox\"",title_index) ;
  sprintf(buffer+strlen(buffer),(param == YES) ? "checked=\"\"" : " ") ;
  sprintf(buffer+strlen(buffer)," name=\"ch_input%ld\"><b>%s</b>",title_index,GetTextIndexed(title, sizeof(title), title_index, htmlCheckParamEcu)) ;
  httpd_resp_sendstr_chunk(req,buffer) ;
}

void WSInputBox(httpd_req_t *req,uint32_t title_index,uint32_t intparam,char *charparam,float floatparam, int type, bool show)
{
  char title[100];
  char buffer[500] ;
  sprintf(buffer,"<b>%s</b><br>",GetTextIndexed(title, sizeof(title), title_index, htmlInputParamEng)) ;
  switch(type)
  {
    case NUMBER : 
      sprintf(buffer+strlen(buffer),"<input id=\"input_%ld\" placeholder=\"\" value=\"%ld\" name=\"input_%ld\" type=\"number\" min=\"%ld\" max=\"%ld\"></p><p>",title_index,intparam,title_index,htmlParamEngMinMax[0][title_index],htmlParamEngMinMax[1][title_index]);
      break ;
    case TEXT :
      sprintf(buffer+strlen(buffer),"<input id=\"input_%ld\" placeholder=\"\" value=\"%s\" name=\"input_%ld\" minlength=\"%ld\" maxlength=\"%ld\"></p><p>",title_index,charparam,title_index,htmlParamEngMinMax[0][title_index],htmlParamEngMinMax[1][title_index]);
      break ;
    case FLOAT : 
      sprintf(buffer+strlen(buffer),"<input id=\"input_%ld\" placeholder=\"\" value=\"%0.2f\" name=\"input_%ld\" type=\"number\" step=\"0.01\" min=\"%0.2f\" max=\"%0.2f\"></p><p>",title_index,floatparam,title_index,(float)(htmlParamEngMinMax[0][title_index]),(float)(htmlParamEngMinMax[1][title_index]));
      break ;
  }
  httpd_resp_sendstr_chunk(req,buffer) ;
}

void WSSaveBouton(httpd_req_t *req)
{
  const char htmlBoutonSave[] = "</p><p><button name=\"save\" type=\"submit\" class=\"button bgrn\">"B_SAVE"</button></form></fieldset><p>" ;
  httpd_resp_sendstr_chunk(req, htmlBoutonSave) ;
}

void WSRetourBouton(httpd_req_t *req)
{
  const char htmlBoutonRetour[] = "<p></p><form action=\"/\" method=\"get\"><button name="">"B_RETOUR"</button></form>" ;
  httpd_resp_sendstr_chunk(req,htmlBoutonRetour ) ;
}

void WSBouton(httpd_req_t *req,int bouton)
{
  char title[50] ;
  char action[50] ;
  char html[200] ;
  sprintf(html,"<p></p><form action=\"%s\" method=\"get\"><button name="">%s</button></form>",GetTextIndexed(action, sizeof(action), bouton, BoutonFrontpageAction),GetTextIndexed(title, sizeof(title), bouton, htmlBoutonFrontpage)) ;
  httpd_resp_sendstr_chunk(req,html) ;
}
