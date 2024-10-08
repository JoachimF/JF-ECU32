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

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_http_server.h"
#include "jf-ecu32.h"

const uint16_t CHUNKED_BUFFER_SIZE = 500;                // Chunk buffer size (needs to be well below stack space (4k for ESP8266, 8k for ESP32) but large enough to cache some small messages)

#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#define PSTR(x) (x)
#define nullptr                                     NULL

const char HTTP_SNS_HR[] = "<tr><td colspan=2 style='font-size:2px'><hr/></td></tr>";

enum ParamEcuRadiosEnum{
  R_THROTTLE_TYP,R_GLOW_TYP,R_START_TYP,R_PUMP1_TYP,
  R_STARTER_TYP,R_TELEM_TYP,R_PUMP2_TYP
} ;

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


enum ParamEcuCheckEnum{
  C_AUX_EN,C_LEDS_EN
} ;

const char htmlCheckParamEcu[]  =
  CH_AUX_EN "|" CH_LEDS_EN ;

enum BoutonFrontpageEnum {
  BT_PARAMECU, BT_PARAM_MOTEUR,
  BT_INFORMATION, BT_LOG, BT_WIFI, BT_SLIDER, BT_JAUGES,
  BT_START_ENGINE, BT_STOP_ENGINE, BT_MAJ, BT_CUT_WIFI } ;

const char htmlBoutonFrontpage[]  =
  B_PARAMECU "|" B_PARAM_MOTEUR "|"
  B_INFORMATION "|" B_LOG "|" B_WIFI "|" B_SLIDER "|" B_JAUGES "|"
  B_START_ENGINE "|" B_STOP_ENGINE"|" B_MAJ "|" B_CUT_WIFI ;


const char BoutonFrontpageAction[]  =
  "configecu|configmoteur|"
  "info|logs|wifi|slider|gauges|"
  "start|stop|upgrade|stopwifi";

enum BoutonParmEcuEnum {
  BT_SAVE, BT_RETOUR  };

const char htmlBoutonParamEcu[]  =
  B_SAVE "|" B_RETOUR  ;

const char htmlBoutonParamEcuAction[]  =
  "|configecu|/|";

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

  if (title_index >= BT_START_ENGINE) {
    char confirm[100];
    sprintf(buffer," onsubmit='return confirm(\"%s\");'><button name='%s' class='button bred'>%s</button></form></p>",
      GetTextIndexed(confirm, sizeof(confirm), title_index, kButtonConfirm),
      (!title_index) ? PSTR("rst") : PSTR("non"),
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
    sprintf(buffer,"<p><input id=\"input_%ld_%d\" name=\"input_%ld\" type=\"radio\" value=\"%d\"",title_index,i,title_index,i) ;
    sprintf(buffer+strlen(buffer),(param == i) ? "checked=\"\"" : " ") ;
    sprintf(buffer+strlen(buffer),"><b>%s</b>",GetTextChoice(title, sizeof(title), title_index,i, htmlRadioParamEcuChoicesTxt)) ;
    httpd_resp_sendstr_chunk(req,buffer) ;
  }
  httpd_resp_sendstr_chunk(req, "</fieldset><p>") ;
}
