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

typedef struct {
	char url[32];
	char parameter[128];
} URL_t;

//#define MIN(iA,iB)  ((iA)<(iB) ? (iA) : (iB))
static const char* get_path_from_uri(char *dest, const char *uri, size_t destsize) ;


#endif