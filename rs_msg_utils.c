/*
 * Copyright (c) 2017 Shaun Feakes - All rights reserved
 *
 * You may use redistribute and/or modify this code under the terms of
 * the GNU General Public License version 2 as published by the 
 * Free Software Foundation. For the terms of this license, 
 * see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 *  https://github.com/sfeakes/aqualinkd
 */

#define _GNU_SOURCE 1 // for strcasestr

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "utils.h"

/*
int check_panel_conf(char *panel)
{
"RS-16 Combo"
"PD-8 Only"
"PD-8 Combo"
"RS-2/14 Dual"
"RS-2/10 Dual"
"RS-16 Only"
"RS-12 Only"
"RS-16 Combo"
"RS-12 Combo"
"RS-2/6 Dual"
"RS-4 Only"
"RS-6 Only"
"RS-8 Only"
"RS-4 Combo"
"RS-6 Combo"
"RS-8 Combo"
}
*/
char *rsm_strstr(const char *haystack, const char *needle)
{
  char *sp1 = (char *)haystack;
  char *sp2 = (char *)needle;
  //int i=0;
  // Get rid of all padding
  while(isspace(*sp1)) sp1++;
  while(isspace(*sp2)) sp2++;

  if (strlen(sp1) == 0 || strlen(sp2) == 0)
    return NULL;
  // Need to write this myself for speed
  //LOG(AQUA_LOG,LOG_DEBUG, "Compare (reset)%d chars of '%s' to '%s'\n",strlen(sp2),sp1,sp2);
  return strcasestr(sp1, sp2);
}
// Check s2 exists in s1
int rsm_strcmp(const char *haystack, const char *needle)
{
  char *sp1 = (char *)haystack;
  char *sp2 = (char *)needle;
  //int i=0;
  // Get rid of all padding
  while(isspace(*sp1)) sp1++;
  while(isspace(*sp2)) sp2++;

  if (strlen(sp1) == 0 || strlen(sp2) == 0)
    return -1;
  // Need to write this myself for speed
  //LOG(AQUA_LOG,LOG_DEBUG, "Compare (reset)%d chars of '%s' to '%s'\n",strlen(sp2),sp1,sp2);
  return strncasecmp(sp1, sp2, strlen(sp2));
}

int _rsm_strncpy(char *dest, const unsigned char *src, int dest_len, int src_len, bool nulspace)
{
  int i;
  int end = dest_len < src_len ? dest_len:src_len;
  //0x09 is Tab and means next field on table.
  for(i=0; i < end; i++) {
    //0x00 on button is space
    //0x00 on message is end
    if (src[i] == 0x00 && nulspace)
      dest[i] = ' ';
    else if (src[i] == 0x00 && !nulspace)
    {
      dest[i] = '\0';
      break;
    }
    else if ( (src[i] < 32 || src[i] > 126) && src[1] != 10 ) // only printable chars
      dest[i] = ' ';
    else
      dest[i] = src[i];

    //printf("Char %c to %c\n",src[i],dest[i]);
  }

  //printf("--'%s'--\n",dest);

  if (dest[i] != '\0') {
    if (i < (dest_len-1))
      i++;

    dest[i] = '\0';
  }

  return i;
}

int rsm_strncpy(char *dest, const unsigned char *src, int dest_len, int src_len)
{
  return _rsm_strncpy(dest, src, dest_len, src_len, false);
}
int rsm_strncpy_nul2sp(char *dest, const unsigned char *src, int dest_len, int src_len)
{
  return _rsm_strncpy(dest, src, dest_len, src_len, true);
}

#define INT_MAX +2147483647
#define INT_MIN -2147483647

// atoi that can have blank start
int rsm_atoi(const char* str) 
{ 
  int sign = 1, base = 0, i = 0; 
    // if whitespaces then ignore. 
  if (str == NULL)
    return -1;
  
  while (str[i] == ' ') { 
    i++; 
  } 

    // checking for valid input 
    while (str[i] >= '0' && str[i] <= '9') { 
        // handling overflow test case 
        if (base > INT_MAX / 10 || (base == INT_MAX / 10 && str[i] - '0' > 7)) { 
            if (sign == 1) 
                return INT_MAX; 
            else
                return INT_MIN; 
        } 
        base = 10 * base + (str[i++] - '0'); 
    } 
    return base * sign; 
}

float rsm_atof(const char* str)
{
  int i=0;

  while (str[i] == ' ') { 
    i++; 
  }

  return atof(&str[i]);
}