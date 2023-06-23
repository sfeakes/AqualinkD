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
#include "rs_msg_utils.h"

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

/*
Pull revision from string  examples
'E0260801 REV. O.2'
'    REV. O.2    '
'B0029221 REV T.0.1'
'   REV T.0.1'
*/
bool rsm_get_revision(char *dest, const char *src, int src_len)
{
  char *sp = NULL;
  char *ep = NULL;
  
  sp = rsm_strnstr(src, "REV", src_len);
  if (sp == NULL) {
    return false;
  }
  sp = sp+3;
  while ( *sp == ' ' || *sp == '.') {
    sp = sp+1;
  }
  // sp is now the start of string revision #
  ep = sp;
  while ( *ep != ' ' && *ep != '\0') {
    ep = ep+1;
  }
  
  int len=ep-sp;
  // Check we got something usefull
  if (len > 5) {
    return false;
  }

  memcpy(dest, sp, len);
  dest[len] = '\0';

  return true;
}

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
  // Maybe use stristr from utils.c in the future. Needs a lot of testing.
  //LOG(AQUA_LOG,LOG_DEBUG, "Compare (reset)%d chars of '%s' to '%s'\n",strlen(sp2),sp1,sp2);
  return strcasestr(sp1, sp2);
}


char *rsm_strncasestr(const char *haystack, const char *needle, int length)
{
  // NEED TO WRITE THIS MYSELF.  Same as below but limit length
  return strcasestr(haystack, needle);
}

/*
 * Find the first occurrence of find in s, where the search is limited to the
 * first slen characters of s.
 */
char *rsm_strnstr(const char *s, const char *find, size_t slen)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if (slen-- < 1 || (sc = *s++) == '\0')
					return (NULL);
			} while (sc != c);
			if (len > slen)
				return (NULL);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
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

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int rsm_strncmp(const char *haystack, const char *needle, int length)
{
  char *sp1 = (char *)haystack;
  char *sp2 = (char *)needle;
  char *ep1 = (sp1+length);
  //int i=0;
  // Get rid of all padding
  while(isspace(*sp1)) sp1++;
  while(isspace(*sp2)) sp2++;

  if (strlen(sp1) == 0 || strlen(sp2) == 0)
    return -1;

  // Work out last char in haystack
  while(isspace(*ep1)) ep1--;


  LOG(AQUA_LOG,LOG_DEBUG, "CHECK haystack SP1='%c' EP1='%c' SP2='%c' '%.*s' for '%s' length=%d\n",*sp1,*ep1,*sp2,(ep1-sp1)+1,sp1,sp2,(ep1-sp1)+1);
  // Need to write this myself for speed
  // Need to check if full length string (no space on end), that the +1 is accurate. MIN should do it
  return strncasecmp(sp1, sp2, MIN((ep1-sp1)+1,length));
}

// NSF Check is this works correctly.
char *rsm_strncpycut(char *dest, const char *src, int dest_len, int src_len)
{
  char *sp = (char *)src;
  char *ep = (sp+dest_len);

  while(isspace(*sp)) sp++;
  while(isspace(*ep)) ep--;

  int length=MIN((ep-sp)+1,dest_len);

  memset(dest, '\0',dest_len);
  return strncpy(dest, sp, length);
  //dest[length] = '\0';
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

// atof that can have blank start
float rsm_atof(const char* str)
{
  int i=0;

  while (str[i] == ' ') { 
    i++; 
  }

  return atof(&str[i]);
}